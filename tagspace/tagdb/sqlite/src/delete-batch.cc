#include "tagdb/sqlite.h"
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace tagdb {

tagd::code sqlite::prepare_delete() {
	if (_write_poisoned)
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");

	// Storage diagnostics name the database role; its physical location stays private.
	struct stat status{};
	if (::stat(_db_fname.c_str(), &status) < 0) {
		const auto saved = errno;
		return error(saved == ENOENT ? tagd::TS_NOT_FOUND : tagd::TS_NOT_AVAILABLE,
			"required database: " + std::string(std::strerror(saved)));
	}
	if (::access(_db_fname.c_str(), R_OK | W_OK) < 0)
		return error(tagd::TS_NOT_AVAILABLE,
			"required database: " + std::string(std::strerror(errno)));

	const auto durable = prepare_store();
	if (durable != tagd::TAGD_OK)
		return error(tagd::TS_NOT_AVAILABLE,
			"required database: " + std::string(sqlite3_errmsg(_db)));

	// A lock is not an empty reference set. Check writability before planning a cascade.
	if (sqlite3_exec(_db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK)
		return error(tagd::TS_NOT_AVAILABLE,
			"required database: " + std::string(sqlite3_errmsg(_db)));
	if (sqlite3_exec(_db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
		_write_poisoned = true;
		return error(tagd::TS_INTERNAL_ERR, "cannot release database preflight transaction");
	}
	return code(tagd::TAGD_OK);
}

tagd::code sqlite::del_batch(std::span<const tagd::id_string> ids, session* ssn) {
	reset(ssn);
	if (_write_poisoned || !_db || !sqlite3_get_autocommit(_db))
		return error(tagd::TS_MISUSE, "atomic DELETE requires a usable idle backend");
	if (ids.empty())
		return code(tagd::TAGD_OK);

	std::string members;
	for (const auto& id : ids) {
		if (id.empty() || id.front() == '_')
			return error(tagd::TS_MISUSE, "atomic DELETE cannot remove empty IDs or hard tags");
		char* quoted = sqlite3_mprintf("tid(%Q)", id.c_str());
		if (!quoted)
			return error(tagd::TS_INTERNAL_ERR, "cannot allocate atomic DELETE selection");
		if (!members.empty())
			members += ',';
		members += quoted;
		sqlite3_free(quoted);
	}
	members = '(' + members + ')';
	mutation_scope mutation(*this);
	write_transaction transaction(*this, write_transaction::kind::immediate);
	if (transaction.begin() != tagd::TAGD_OK)
		return error(tagd::TS_NOT_AVAILABLE,
			"required database: " + std::string(sqlite3_errmsg(_db)));

	const auto result = [&]() -> tagd::code {
		std::set<tagd::id_string> affected;
		for (const auto& id : ids) {
			tagd::abstract_tag tag;
			const auto rc = get(tag, id, nullptr, F_NO_TRANSFORM_REFERENTS);
			if (rc != tagd::TAGD_OK)
				return rc;
			tag_affected(affected, tag);
		}

		// Only selected subjects lose their outgoing facts. Literal modifiers are not references.
		const std::string dependencies =
			"SELECT 1 FROM tags WHERE tag NOT IN " + members +
			" AND (sub_relator IN " + members + " OR super_object IN " + members + ") UNION ALL "
			"SELECT 1 FROM relations WHERE subject NOT IN " + members +
			" AND (relator IN " + members + " OR object IN " + members + ") UNION ALL "
			"SELECT 1 FROM type_relations WHERE subject NOT IN " + members +
			" AND (type_relator IN " + members + " OR type_object IN " + members + ") UNION ALL "
			"SELECT 1 FROM referents WHERE refers_to IN " + members + " OR context IN " + members + " LIMIT 1";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(_db, dependencies.c_str(), -1, &statement, nullptr) != SQLITE_OK)
			return error(tagd::TS_NOT_AVAILABLE,
				"cannot check database references: " + std::string(sqlite3_errmsg(_db)));
		const auto step = sqlite3_step(statement);
		sqlite3_finalize(statement);
		if (step == SQLITE_ROW)
			return error(tagd::TS_DEPENDENCY, "unrelated references block managed deletion");
		if (step != SQLITE_DONE)
			return error(tagd::TS_NOT_AVAILABLE,
				"cannot check database references: " + std::string(sqlite3_errmsg(_db)));

		for (const auto& id : ids) {
			const auto rc = delete_fts_tag(id);
			if (rc != tagd::TAGD_OK)
				return rc;
		}
		for (const std::string table : {"relations", "type_relations"}) {
			const auto rc = exec(("DELETE FROM " + table + " WHERE subject IN " + members).c_str());
			if (rc != tagd::TAGD_OK)
				return rc;
		}
		// One SQL statement permits references wholly inside the selected deletion set.
		const auto removed = exec(("DELETE FROM tags WHERE tag IN " + members).c_str());
		if (removed != tagd::TAGD_OK)
			return removed;
		for (const auto& id : affected) {
			const auto rc = update_pos_occurence(id);
			if (rc != tagd::TAGD_OK)
				return rc;
		}
		return tagd::TAGD_OK;
	}();
	const auto rc = transaction.finish(result);
	if (ssn)
		ssn->code(rc);
	return code(rc);
}

}
