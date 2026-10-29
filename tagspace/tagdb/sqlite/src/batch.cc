#include "tagdb/sqlite.h"

namespace tagdb {

// This preflight supplies durability, not synchronization with other filepile users.
tagd::code sqlite::prepare_store() {
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	if (!_db || !sqlite3_get_autocommit(_db)) {
		return error(tagd::TS_MISUSE, "filepile requires an idle open backend");
	}

	if (sqlite3_db_readonly(_db, "main") != 0) {
		return error(tagd::TS_MISUSE, "filepile requires writable persistent storage");
	}

	sqlite3_stmt* statement = nullptr;
	if (sqlite3_prepare_v2(_db, "PRAGMA journal_mode", -1, &statement, nullptr) != SQLITE_OK) {
		return error(tagd::TS_INTERNAL_ERR, "cannot read journal mode");
	}
	std::string mode;
	if (sqlite3_step(statement) == SQLITE_ROW) {
		mode = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
	}
	sqlite3_finalize(statement);
	if (mode != "wal" && mode != "delete" && mode != "truncate" && mode != "persist") {
		return error(tagd::TS_MISUSE, "filepile requires a durable journal mode");
	}
	if (exec("PRAGMA synchronous=EXTRA") != tagd::TAGD_OK) {
		return code();
	}
	if (sqlite3_prepare_v2(_db, "PRAGMA synchronous", -1, &statement, nullptr) != SQLITE_OK) {
		return error(tagd::TS_INTERNAL_ERR, "cannot verify synchronous setting");
	}
	const bool durable = sqlite3_step(statement) == SQLITE_ROW
		&& sqlite3_column_int(statement, 0) == 3;
	sqlite3_finalize(statement);
	return durable ? code(tagd::TAGD_OK)
		: error(tagd::TS_MISUSE, "filepile requires synchronous=EXTRA");
}

tagd::code sqlite::put_batch(std::span<const tagd::abstract_tag* const> tags,
	session* ssn, flags_t flags) {
	reset(ssn);
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	if (!_db || !sqlite3_get_autocommit(_db)) {
		return error(tagd::TS_MISUSE, "atomic PUT requires an idle open backend");
	}
	for (auto* tag : tags) {
		if (!tag) {
			return error(tagd::TS_MISUSE, "null tag in atomic PUT");
		}
	}
	mutation_scope mutation(*this);
	if (exec("BEGIN") != tagd::TAGD_OK) {
		return code();
	}

	// Every exit must release this transaction before another operation can use the backend.
	auto unwind = [this](void*) {
		if (sqlite3_exec(_db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
			_write_poisoned = true;
		}
	};
	std::unique_ptr<void, decltype(unwind)> rollback(_db, unwind);

	std::set<tagd::id_string> affected;
	// Borrowed objects retain URL/referent dynamic types until the synchronous call returns.
	tagd::code result = tagd::TAGD_OK;
	for (auto* tag : tags) {
		result = put_transaction(*tag, ssn, flags, &affected);
		if (result != tagd::TAGD_OK) {
			break;
		}
	}
	if (result == tagd::TAGD_OK) {
		result = validate_effective(affected);
	}
	if (result == tagd::TAGD_OK) {
		result = exec("COMMIT");
		if (result == tagd::TAGD_OK) {
			if (ssn) {
				ssn->code(result);
			}
			rollback.release();
			return code(result);
		}
	}

	rollback.release();
	// A successful rollback proves failure; loss of that proof forbids further writes.
	if (!sqlite3_get_autocommit(_db) && exec("ROLLBACK") == tagd::TAGD_OK) {
		if (ssn) {
			ssn->code(result);
		}
		return code(result);
	}
	_write_poisoned = true;
	result = error(tagd::TS_INTERNAL_ERR, "atomic PUT commit outcome unknown; reopen and check storage");
	if (ssn) {
		ssn->code(result);
	}
	return result;
}

}
