#include "tagdb/sqlite.h"
#include <algorithm>
#include <iterator>

namespace tagdb {

tagd::code sqlite::collect_dependents(std::set<tagd::id_string>& result,
	const tagd::id_string& id) {
	// UNION visits each subject once, even when structural and type paths converge.
	auto rc = prepare(stmt_t::DEPENDENTS,
		"WITH RECURSIVE affected(tag) AS (VALUES(tid(?)) "
		"UNION SELECT t.tag FROM tags t JOIN affected a ON t.super_object=a.tag "
		"UNION SELECT r.subject FROM type_relations r JOIN affected a ON r.type_object=a.tag) "
		"SELECT idt(tag) FROM affected JOIN tags USING(tag)");
	if (rc != tagd::TAGD_OK
		|| (rc = bind_text(stmt_t::DEPENDENTS, 1, id.c_str())) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(stmt_t::DEPENDENTS);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		result.emplace(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}
	sqlite3_reset(stmt);
	return step == SQLITE_DONE ? tagd::TAGD_OK
		: error(tagd::TS_INTERNAL_ERR, "cannot read affected subjects");
}

tagd::code sqlite::collect_subtree(std::set<tagd::id_string>& result,
	const tagd::id_string& id) {
	auto rc = prepare(stmt_t::SUBTREE,
		"WITH RECURSIVE subtree(tag) AS (VALUES(tid(?)) "
		"UNION SELECT t.tag FROM tags t JOIN subtree s ON t.super_object=s.tag) "
		"SELECT idt(tag) FROM subtree JOIN tags USING(tag)");
	if (rc != tagd::TAGD_OK
		|| (rc = bind_text(stmt_t::SUBTREE, 1, id.c_str())) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(stmt_t::SUBTREE);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		result.emplace(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}
	sqlite3_reset(stmt);
	return step == SQLITE_DONE ? tagd::TAGD_OK
		: error(tagd::TS_INTERNAL_ERR, "cannot read structural subtree");
}

tagd::code sqlite::load_relation_declarations(relation_resolver& state,
	const std::set<tagd::id_string>& subjects, const std::set<tagd::id_string>& rank_ids) {
	relation_resolver::declarations_t declarations;
	relation_resolver::ranks_t ranks;
	std::set<tagd::id_string> components = rank_ids;
	std::vector<tagd::id_string> pending(subjects.begin(), subjects.end());
	while (!pending.empty()) {
		auto id = std::move(pending.back());
		pending.pop_back();
		if (declarations.contains(id)) {
			continue;
		}

		tagd::abstract_tag t;
		tagd::code rc;
		if (id == tagd::HARD_TAG_ENTITY) {
			// The axiomatic root has no stored rank string.
			rc = hard_tag::get(t, id);
		} else {
			canonical_identity identity;
			tagd::rank rank;
			rc = load_identity(identity, rank, id);
			if (rc == tagd::TAGD_OK) {
				t = tagd::abstract_tag(tagd::abstract_tag(id, identity.sub_relator,
					identity.super_object, identity.pos), rank);
			}
		}
		if (rc != tagd::TAGD_OK) {
			return error(rc, "unresolved relation source: " + id);
		}
		ranks.emplace(id, t.rank());
		rc = get_relations(t.relations, id, nullptr, F_NO_TRANSFORM_REFERENTS | F_NO_RESET);
		if (rc != tagd::TAGD_OK && !(rc == tagd::TS_NOT_FOUND && t.relations.empty())) {
			return rc;
		}
		if ((rc = get_type_relations(t.type_relations, id)) != tagd::TAGD_OK) {
			return rc;
		}

		if (pos(t.sub_relator(), nullptr) != tagd::POS_SUB_RELATOR) {
			return error(tagd::TS_MISUSE, "identity requires a subordinate relator: " + id);
		}
		pending.push_back(t.super_object());
		for (const auto& p : t.type_relations) {
			if (pos(p.relator, nullptr) != tagd::POS_TYPE_RELATOR) {
				return error(tagd::TS_MISUSE, "type declaration requires a type relator");
			}
			pending.push_back(p.object);
			components.insert(p.relator);
		}
		for (const auto& p : t.relations) {
			if (pos(p.relator, nullptr) == tagd::POS_TYPE_RELATOR) {
				return error(tagd::TS_MISUSE, "type relator used in an ordinary predicate");
			}
			components.insert(p.relator);
			components.insert(p.object);
		}
		declarations.emplace(id, std::move(t));
	}

	// Component ranks support specialization without loading their unrelated predicates.
	for (const auto& id : components) {
		if (ranks.contains(id)) {
			continue;
		}
		tagd::rank rank;
		const auto rc = lookup_rank(rank, id);
		if (rc == tagd::TAGD_OK) {
			ranks.emplace(id, rank);
		} else if (rc != tagd::TS_NOT_FOUND) {
			return rc;
		}
	}
	state = relation_resolver(std::move(declarations), std::move(ranks));
	return tagd::TAGD_OK;
}

tagd::code sqlite::expand_relations(tagd::abstract_tag& t, const tagd::id_string& id,
	const relation_resolver& state, session* ssn, flags_t flags) {
	tagd::predicate_set predicates;
	auto rc = state.evaluate(predicates, id, *this);
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	if (ssn && !(flags & F_NO_TRANSFORM_REFERENTS)) {
		predicates.transform(f_encode_referent(ssn));
	}

	// Preserve GET's referent marker while combining direct and inherited predicates.
	predicates.insert(t.relations.begin(), t.relations.end());
	tagd::normalize_predicates(predicates);
	t.relations.swap(predicates);
	return tagd::TAGD_OK;
}

tagd::code sqlite::validate_effective(const std::set<tagd::id_string>& affected) {
	if (_relation_semantics == relation_semantics::profile_overrides) {
		return tagd::TAGD_OK;
	}

	// A changed path passes through a modified subject; their final dependents cover old paths too.
	std::set<tagd::id_string> subjects;
	for (const auto& source : affected) {
		const auto rc = collect_dependents(subjects, source);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
	}

	relation_resolver state;
	auto rc = load_relation_declarations(state, subjects);
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	for (const auto& subject : subjects) {
		if (!state.declarations().contains(subject)) {
			continue;
		}
		tagd::predicate_set view;
		rc = state.evaluate(view, subject, *this);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
	}
	return tagd::TAGD_OK;
}

tagd::code sqlite::check_inherited_deletion(const tagd::abstract_tag& request,
	const tagd::abstract_tag& local) {
	if (_relation_semantics == relation_semantics::profile_overrides
		|| (request.relations.empty() && request.type_relations.empty())) {
		return tagd::TAGD_OK;
	}
	relation_resolver state;
	auto rc = load_relation_declarations(state, {request.id()});
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	for (const auto& source : state.sources(request.id())) {
		if (source == request.id()) {
			continue;
		}
		const auto& origin = state.declarations().at(source);
		for (const auto& p : request.relations) {
			if (!local.related(p.relator, p.object)
				&& (p.modifier.empty() ? origin.related(p.relator, p.object) : origin.related(p))) {
				return error(tagd::TS_MISUSE, "cannot delete inherited predicate from " + source);
			}
		}
		for (const auto& p : request.type_relations) {
			if (!local.type_relations.contains(p) && origin.type_relations.contains(p)) {
				return error(tagd::TS_MISUSE, "cannot delete inherited type relation from " + source);
			}
		}
	}
	return tagd::TAGD_OK;
}

tagd::code sqlite::relation_candidates(std::set<tagd::id_string>& result,
	const tagd::predicate& p) {
	const bool valued = !p.modifier.empty();
	const bool relator = !p.relator.empty(), object = !p.object.empty();
	// Each cache key has one SQL shape; parameter values never change its access predicates.
	const stmt_t bare_keys[] = {stmt_t::RELATION_CANDIDATES_ALL,
		stmt_t::RELATION_CANDIDATES_RELATOR, stmt_t::RELATION_CANDIDATES_OBJECT,
		stmt_t::RELATION_CANDIDATES_PAIR};
	const stmt_t valued_keys[] = {stmt_t::VALUED_CANDIDATES_ALL,
		stmt_t::VALUED_CANDIDATES_RELATOR, stmt_t::VALUED_CANDIDATES_OBJECT,
		stmt_t::VALUED_CANDIDATES_PAIR};
	const auto key = (valued ? valued_keys : bare_keys)[relator + 2 * object];
	std::string sql =
		"WITH RECURSIVE relators(tag) AS (VALUES(tid(?1)) "
		"UNION SELECT t.tag FROM tags t JOIN relators r ON t.super_object=r.tag), "
		"objects(tag) AS (VALUES(tid(?2)) "
		"UNION SELECT t.tag FROM tags t JOIN objects o ON t.super_object=o.tag), "
		"affected(tag) AS (SELECT subject FROM relations WHERE 1";
	if (relator) {
		sql += valued ? " AND relator=tid(?1)" : " AND relator IN relators";
	}
	if (object) {
		sql += valued ? " AND object=tid(?2)" : " AND object IN objects";
	}
	if (valued) {
		// Joint entailment may need several bounds; selecting by query value would lose witnesses.
		sql += " AND modifier IS NOT NULL";
	}

	// UNION deduplicates all origins and their dependents in one recursive traversal.
	// Foreign keys guarantee stored subjects; a final tag-table filter is unnecessary.
	sql += " UNION SELECT t.tag FROM tags t JOIN affected a ON t.super_object=a.tag "
		"UNION SELECT r.subject FROM type_relations r JOIN affected a ON r.type_object=a.tag) "
		"SELECT idt(tag) FROM affected";

	auto rc = prepare(key, sql.c_str());
	if (rc != tagd::TAGD_OK
		|| (rc = bind_text(key, 1, p.relator.c_str())) != tagd::TAGD_OK
		|| (rc = bind_text(key, 2, p.object.c_str())) != tagd::TAGD_OK) {
		return rc;
	}

	auto* stmt = get_stmt(key);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		result.emplace(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}
	sqlite3_reset(stmt);
	if (step != SQLITE_DONE) {
		return error(tagd::TS_INTERNAL_ERR, "cannot select explicit relation candidates");
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::query_effective(tagd::tag_set& result, const tagd::interrogator& q,
	session* ssn, flags_t flags) {
	std::set<tagd::id_string> candidates, rank_ids;
	bool selected = false;
	auto intersect = [&](std::set<tagd::id_string> matches) {
		if (!selected) {
			candidates = std::move(matches);
			selected = true;
			return;
		}
		std::set<tagd::id_string> common;
		std::set_intersection(candidates.begin(), candidates.end(), matches.begin(), matches.end(),
			std::inserter(common, common.end()));
		candidates.swap(common);
	};

	std::map<tagd::rank, tagd::id_string> search_ranks;
	bool searched = false;
	bool semantic_condition = !q.type_relations.empty();
	for (const auto& p : q.relations) {
		if (p.object != tagd::HARD_TAG_TERMS) {
			semantic_condition = true;
			std::set<tagd::id_string> matches;
			const auto rc = relation_candidates(matches, p);
			if (rc != tagd::TAGD_OK) {
				return rc;
			}
			intersect(std::move(matches));
			if (!p.relator.empty()) {
				rank_ids.insert(p.relator);
			}
			if (!p.object.empty()) {
				rank_ids.insert(p.object);
			}
			continue;
		}

		tagd::tag_set hits;
		const auto rc = search(hits, p.modifier, flags & ~F_INCLUDE_INHERITED_PREDICATES);
		if (rc != tagd::TAGD_OK) {
			result.clear();
			return rc;
		}
		std::map<tagd::rank, tagd::id_string> ranks;
		for (const auto& hit : hits) {
			ranks.emplace(hit.rank(), hit.pos() == tagd::POS_URL ? tagd::url(hit.id()).hduri() : hit.id());
		}
		if (!searched) {
			search_ranks = std::move(ranks);
		} else {
			// Full-text conjunction selects the more specific matching identity, without merging predicates.
			std::map<tagd::rank, tagd::id_string> common;
			for (const auto& a : search_ranks) {
				for (const auto& b : ranks) {
					if (a.first.empty() || a.first.contains(b.first)) {
						common.insert(b);
					} else if (b.first.empty() || b.first.contains(a.first)) {
						common.insert(a);
					}
				}
			}
			search_ranks.swap(common);
		}
		searched = true;
	}

	for (const auto& p : q.type_relations) {
		std::set<tagd::id_string> matches;
		const auto rc = collect_dependents(matches, p.object);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		intersect(std::move(matches));
		rank_ids.insert(p.relator);
	}
	if (searched) {
		std::set<tagd::id_string> matches;
		for (const auto& [rank, id] : search_ranks) {
			if (semantic_condition) {
				const auto rc = collect_subtree(matches, id);
				if (rc != tagd::TAGD_OK) {
					return rc;
				}
			} else {
				matches.insert(id);
			}
		}
		intersect(std::move(matches));
	}
	if (!selected) {
		std::set<tagd::id_string> matches;
		const auto rc = collect_subtree(matches, q.super_object().empty()
			? std::string(tagd::HARD_TAG_ENTITY) : q.super_object());
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		intersect(std::move(matches));
	}

	if (!q.super_object().empty()) {
		rank_ids.insert(q.super_object());
	}

	relation_resolver state;
	auto rc = load_relation_declarations(state, candidates, rank_ids);
	if (rc != tagd::TAGD_OK) {
		return rc;
	}
	tagd::tag_set found;
	for (const auto& id : candidates) {
		if (!q.super_object().empty() && !state.structurally_contains(q.super_object(), id)) {
			continue;
		}

		tagd::predicate_set view;
		rc = state.evaluate(view, id, *this);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		if (!std::all_of(q.relations.begin(), q.relations.end(), [&](const auto& p) {
			return p.object == tagd::HARD_TAG_TERMS || state.matches(view, p);
		}) || !std::all_of(q.type_relations.begin(), q.type_relations.end(), [&](const auto& p) {
			return state.matches_type_relation(id, p);
		})) {
			continue;
		}

		tagd::abstract_tag output;
		rc = format_declaration(output, state.declarations().at(id), ssn, flags);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		if (flags & F_INCLUDE_INHERITED_PREDICATES) {
			if (ssn && !(flags & F_NO_TRANSFORM_REFERENTS)) {
				view.transform(f_encode_referent(ssn));
			}
			view.insert(output.relations.begin(), output.relations.end());
			tagd::normalize_predicates(view);
			output.relations.swap(view);
		}
		found.insert(std::move(output));
	}
	result.swap(found);
	return result.empty() ? tagd::TS_NOT_FOUND : tagd::TAGD_OK;
}

} // namespace tagdb
