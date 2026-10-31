#include "tagdb/sqlite.h"
#include <algorithm>
#include <map>
#include <set>

namespace tagdb {

tagd::code sqlite::check_storage_schema() {
	auto rc = prepare(stmt_t::SCHEMA_TABLES, "SELECT 1 FROM sqlite_master WHERE type='table'");
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	auto* tables = get_stmt(stmt_t::SCHEMA_TABLES);
	const auto step = sqlite3_step(tables);
	sqlite3_reset(tables);
	if (step == SQLITE_DONE) {
		return tagd::TAGD_OK;
	}
	if (step != SQLITE_ROW) {
		return error(tagd::TS_INTERNAL_ERR, "cannot inspect storage schema");
	}

	rc = prepare(stmt_t::STORAGE_VERSION, "PRAGMA user_version");
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	auto* version = get_stmt(stmt_t::STORAGE_VERSION);
	const auto version_step = sqlite3_step(version);
	const bool compatible = version_step == SQLITE_ROW && sqlite3_column_int(version, 0) == SCHEMA_VERSION;
	sqlite3_reset(version);
	if (version_step != SQLITE_ROW) {
		return error(tagd::TS_INTERNAL_ERR, "cannot inspect storage version");
	}
	if (!compatible) {
		return error(tagd::TS_MISUSE,
			"incompatible tagd database version; recreate from reviewed explicit declarations");
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::type_dependency_errors(const tagd::id_string& id, session* ssn) {
	auto rc = prepare(stmt_t::TYPE_DEPENDENCIES,
		"SELECT 'type_relator' FROM type_relations WHERE type_relator=tid(?1) "
		"UNION SELECT 'type_object' FROM type_relations WHERE type_object=tid(?1)");
	if (rc != tagd::TAGD_OK
		|| (rc = bind_text(stmt_t::TYPE_DEPENDENCIES, 1, id.c_str())) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(stmt_t::TYPE_DEPENDENCIES);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		ssn->error(tagd::TS_DEPENDENCY, tagd::predicate(tagd::HARD_TAG_CAUSED_BY,
			reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), id));
	}
	sqlite3_reset(stmt);
	return step == SQLITE_DONE ? tagd::TAGD_OK
		: ssn->error(tagd::TS_INTERNAL_ERR, "cannot read type dependencies");
}

tagd::code sqlite::get_type_relations(tagd::type_predicate_set& values,
	const tagd::id_string& id) {
	auto rc = prepare(stmt_t::GET_TYPE_RELATIONS,
		"SELECT idt(type_relator), idt(type_object) FROM type_relations WHERE subject=tid(?)");
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	if ((rc = bind_text(stmt_t::GET_TYPE_RELATIONS, 1, id.c_str())) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(stmt_t::GET_TYPE_RELATIONS);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		values.emplace(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
	}
	sqlite3_reset(stmt);
	if (step != SQLITE_DONE) {
		return error(tagd::TS_INTERNAL_ERR, "cannot read type relations");
	}

	return values.order([this](tagd::rank& rank, tagd::id_view id) {
		return lookup_rank(rank, id);
	});
}

tagd::code sqlite::validate_declaration(const tagd::abstract_tag& t) {
	if (!t.super_object().empty() && pos(t.sub_relator(), nullptr) != tagd::POS_SUB_RELATOR) {
		return ferror(tagd::TS_MISUSE, "identity requires a subordinate relator: %s",
			t.sub_relator().c_str());
	}

	if (_relation_semantics == relation_semantics::profile_overrides && !t.type_relations.empty()) {
		return error(tagd::TS_MISUSE, "configuration profiles use structural overrides, not type declarations");
	}

	// Even self/root no-ops must resolve references and preserve grammatical roles.
	for (const auto& p : t.type_relations) {
		const auto relator_pos = pos(p.relator, nullptr);
		if (relator_pos == tagd::POS_UNKNOWN) {
			return ferror(tagd::TS_RELATOR_UNK, "unknown type relator: %s", p.relator.c_str());
		}
		if (relator_pos != tagd::POS_TYPE_RELATOR) {
			return ferror(tagd::TS_MISUSE, "not a type relator: %s", p.relator.c_str());
		}
		if (p.object.starts_with(tagd::TAGD_SCHEME)) {
			return error(tagd::TS_MISUSE, "foreign type references are not supported");
		}
		if (p.object != t.id() && !exists(p.object)) {
			return ferror(tagd::TS_OBJECT_UNK, "unknown local type object: %s", p.object.c_str());
		}
	}
	for (const auto& p : t.relations) {
		if (!exists(p.relator)) {
			return ferror(tagd::TS_RELATOR_UNK, "unknown relator: %s", p.relator.c_str());
		}
		if (p.object != t.id() && !exists(p.object)) {
			return ferror(tagd::TS_OBJECT_UNK, "unknown object: %s", p.object.c_str());
		}
		if (pos(p.relator, nullptr) == tagd::POS_TYPE_RELATOR) {
			return error(tagd::TS_MISUSE, "type relators require modifier-free type relations");
		}
	}

	return validate_dependencies(t);
}

bool sqlite::only_type_axioms(const tagd::abstract_tag& t) {
	// Axioms alone succeed; they must not hide duplicates of other direct assertions.
	return t.relations.empty() && !t.type_relations.empty()
		&& std::all_of(t.type_relations.begin(), t.type_relations.end(), [&](const auto& p) {
			return p.object == t.id() || p.object == HARD_TAG_ENTITY;
		});
}

tagd::code sqlite::insert_type_relations(const tagd::abstract_tag& t, size_t& changed) {
	for (const auto& p : t.type_relations) {
		if (p.object == t.id() || p.object == tagd::HARD_TAG_ENTITY) {
			// Reflexivity and the root axiom need no stored edge or change count.
			continue;
		}

		auto rc = prepare(stmt_t::INSERT_TYPE_RELATION,
			"INSERT INTO type_relations(subject,type_relator,type_object) VALUES(tid(?),tid(?),tid(?)) "
			"ON CONFLICT(subject,type_relator,type_object) DO NOTHING");
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		if ((rc = bind_text(stmt_t::INSERT_TYPE_RELATION, 1, t.id().c_str())) != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::INSERT_TYPE_RELATION, 2, p.relator.c_str())) != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::INSERT_TYPE_RELATION, 3, p.object.c_str())) != tagd::TAGD_OK) {
			return rc;
		}

		auto* stmt = get_stmt(stmt_t::INSERT_TYPE_RELATION);
		const int step = sqlite3_step(stmt);
		changed += sqlite3_changes(_db);
		sqlite3_reset(stmt);
		if (step != SQLITE_DONE) {
			return error(tagd::TS_INTERNAL_ERR, "cannot insert type relation");
		}
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_type_relations(const tagd::abstract_tag& t) {
	for (const auto& p : t.type_relations) {
		auto rc = prepare(stmt_t::DELETE_TYPE_RELATION,
			"DELETE FROM type_relations WHERE subject=tid(?) AND type_relator=tid(?) AND type_object=tid(?)");
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		if ((rc = bind_text(stmt_t::DELETE_TYPE_RELATION, 1, t.id().c_str())) != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::DELETE_TYPE_RELATION, 2, p.relator.c_str())) != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::DELETE_TYPE_RELATION, 3, p.object.c_str())) != tagd::TAGD_OK) {
			return rc;
		}

		auto* stmt = get_stmt(stmt_t::DELETE_TYPE_RELATION);
		const int step = sqlite3_step(stmt);
		sqlite3_reset(stmt);
		if (step != SQLITE_DONE) {
			return error(tagd::TS_INTERNAL_ERR, "cannot delete type relation");
		}
		if (!sqlite3_changes(_db)) {
			return ferror(tagd::TS_NOT_FOUND, "no explicit type assertion: %s %s %s",
				t.id().c_str(), p.relator.c_str(), p.object.c_str());
		}
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::validate_dependencies(const tagd::abstract_tag& candidate) {
	std::set<tagd::id_string> sources;
	if (!candidate.super_object().empty()) {
		sources.insert(candidate.super_object());
	}
	for (const auto& p : candidate.type_relations) {
		if (p.object != candidate.id() && p.object != tagd::HARD_TAG_ENTITY) {
			sources.insert(p.object);
		}
	}

	// Adding s -> y closes a cycle precisely when the existing graph reaches s from y.
	for (const auto& source : sources) {
		auto rc = prepare(stmt_t::CYCLE_CHECK,
			"WITH RECURSIVE sources(tag) AS (VALUES(tid(?1)) "
			"UNION SELECT t.super_object FROM tags t JOIN sources s ON t.tag=s.tag "
			"UNION SELECT r.type_object FROM type_relations r JOIN sources s ON r.subject=s.tag) "
			"SELECT 1 FROM sources WHERE tag=tid(?2)");
		if (rc != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::CYCLE_CHECK, 1, source.c_str())) != tagd::TAGD_OK
			|| (rc = bind_text(stmt_t::CYCLE_CHECK, 2, candidate.id().c_str())) != tagd::TAGD_OK) {
			return rc;
		}

		auto* stmt = get_stmt(stmt_t::CYCLE_CHECK);
		const auto step = sqlite3_step(stmt);
		sqlite3_reset(stmt);
		if (step == SQLITE_ROW) {
			return error(tagd::TS_MISUSE,
				"inheritance cycle: " + candidate.id() + " -> " + source + " -> " + candidate.id());
		}
		if (step != SQLITE_DONE) {
			return error(tagd::TS_INTERNAL_ERR, "cannot check relation cycle");
		}
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::validate_reparented_roles(const tagd::id_string& id) {
	// Grammatical uses can lie outside the moved subtree and its inheritors.
	auto rc = prepare(stmt_t::REPARENT_ROLES,
		"WITH RECURSIVE moved(tag) AS (VALUES(tid(?1)) "
		"UNION SELECT t.tag FROM tags t JOIN moved m ON t.super_object=m.tag) "
		"SELECT 1 FROM tags WHERE sub_relator IN "
		"(SELECT tag FROM moved JOIN tags USING(tag) WHERE pos<>?2) "
		"UNION ALL SELECT 1 FROM relations WHERE relator IN "
		"(SELECT tag FROM moved JOIN tags USING(tag) WHERE pos=?3) "
		"UNION ALL SELECT 1 FROM type_relations WHERE type_relator IN "
		"(SELECT tag FROM moved JOIN tags USING(tag) WHERE pos<>?3) LIMIT 1");
	if (rc != tagd::TAGD_OK
		|| (rc = bind_text(stmt_t::REPARENT_ROLES, 1, id.c_str())) != tagd::TAGD_OK
		|| (rc = bind_int(stmt_t::REPARENT_ROLES, 2, tagd::POS_SUB_RELATOR)) != tagd::TAGD_OK
		|| (rc = bind_int(stmt_t::REPARENT_ROLES, 3, tagd::POS_TYPE_RELATOR)) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(stmt_t::REPARENT_ROLES);
	const auto step = sqlite3_step(stmt);
	sqlite3_reset(stmt);
	if (step == SQLITE_ROW) {
		return error(tagd::TS_MISUSE, "reparenting invalidates a stored relator use: " + id);
	}
	return step == SQLITE_DONE ? tagd::TAGD_OK
		: error(tagd::TS_INTERNAL_ERR, "cannot check reparented relators");
}

tagd::code sqlite::put(const tagd::abstract_tag& t, session* ssn, flags_t flags) {
	return put_transaction(t, ssn, flags, nullptr);
}

tagd::code sqlite::check_put(const tagd::abstract_tag& input, flags_t flags) {
	// Referent insertion has its own transaction path and cannot participate in this dry run.
	if (input.pos() == tagd::POS_REFERENT && !(flags & F_NO_POS_CAST))
		return error(tagd::TS_MISUSE, "check_put does not accept referent declarations");

	const auto previous_errors = errors().size();
	_checking_put = true;
	const auto rc = put_transaction(input, nullptr, flags & ~F_IGNORE_DUPLICATES, nullptr);
	_checking_put = false;
	// A duplicate PUT may accompany changed bytes; it is not yet a command error.
	if (rc == tagd::TS_DUPLICATE && _errors)
		_errors->resize(previous_errors);
	return code(rc);
}

tagd::code sqlite::normalize_declaration(tagd::abstract_tag& canonical,
	const tagd::abstract_tag& input, session* ssn, flags_t flags) {
	canonical = input;
	if (!(flags & F_NO_POS_CAST) && input.pos() == tagd::POS_URL) {
		const auto& u = static_cast<const tagd::url&>(input);
		if (!u.ok()) {
			return error(u.code(), "invalid URL declaration");
		}
		canonical = tagd::abstract_tag(u.hduri(), u.sub_relator(), u.super_object(), u.pos());
		canonical.relations = u.relations;
		canonical.type_relations = u.type_relations;
		tagd::url::insert_url_part_relations(canonical.relations, u);
	}
	if (ssn && !(flags & F_NO_TRANSFORM_REFERENTS)) {
		canonical = decode_referents(canonical, ssn);
		if (code() != tagd::TAGD_OK) {
			return code();
		}
	}

	canonical.relations.transform(tagd::canonical_url_id);
	canonical.type_relations.transform(tagd::canonical_url_id);
	return tagd::TAGD_OK;
}

tagd::code sqlite::put_transaction(const tagd::abstract_tag& input, session* ssn,
	flags_t flags, std::set<tagd::id_string>* batch_affected) {
	mutation_scope mutation(*this);
	if (!(flags & F_NO_RESET)) {
		reset(ssn);
	}
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}

	// Preserve dynamic URL/referent contracts before constructing a canonical declaration.
	if (!(flags & F_NO_POS_CAST) && input.pos() == tagd::POS_REFERENT) {
		return insert_referent(static_cast<const tagd::referent&>(input), ssn, flags);
	}
	tagd::abstract_tag canonical;
	const auto normalized = normalize_declaration(canonical, input, ssn, flags);
	if (normalized != tagd::TAGD_OK)
		return normalized;

	// Savepoints compose with atomic batches; failure rolls back all local effects.
	write_transaction transaction(*this, write_transaction::kind::relation_put);
	auto rc = transaction.begin();
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	rc = put_impl(canonical, ssn, flags | F_NO_TRANSFORM_REFERENTS | F_NO_POS_CAST);
	if (rc == tagd::TAGD_OK) {
		if (batch_affected) {
			batch_affected->insert(canonical.id());
		} else {
			rc = validate_effective({canonical.id()});
		}
	}
	rc = transaction.finish(rc, _checking_put);

	if (ssn) {
		ssn->code(rc);
		return rc;
	}

	return code(rc);
}

} // namespace tagdb
