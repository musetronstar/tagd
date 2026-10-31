#include "tagdb/sqlite.h"

namespace tagdb {

tagd::code sqlite::write_transaction::begin() {
	if (_db._write_poisoned)
		return _db.error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	if (!_db._db || _active || (_mode != kind::relation_put && !sqlite3_get_autocommit(_db._db)))
		return _db.error(tagd::TS_MISUSE, "write transaction requires an idle open backend");

	const auto rc = _db.exec(_mode == kind::relation_put ? "SAVEPOINT relation_put"
		: _mode == kind::immediate ? "BEGIN IMMEDIATE" : "BEGIN");
	_active = rc == tagd::TAGD_OK;
	return rc;
}

tagd::code sqlite::write_transaction::rollback() {
	for (auto& [key, stmt] : _db._stmts) {
		sqlite3_reset(stmt);
	}
	bool restored;
	if (_mode == kind::relation_put) {
		const auto rollback = _db.exec("ROLLBACK TO relation_put");
		const auto release = _db.exec("RELEASE relation_put");
		restored = rollback == tagd::TAGD_OK && release == tagd::TAGD_OK;
	} else {
		// Only a confirmed rollback proves that a failed commit changed nothing.
		restored = !sqlite3_get_autocommit(_db._db) && _db.exec("ROLLBACK") == tagd::TAGD_OK;
	}
	_active = false;
	if (restored)
		return tagd::TAGD_OK;
	_db._write_poisoned = true;
	return _db.error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
}

tagd::code sqlite::write_transaction::finish(tagd::code result, bool discard) {
	if (!_active)
		return result;
	if (result == tagd::TAGD_OK && !discard) {
		result = _db.exec(_mode == kind::relation_put ? "RELEASE relation_put" : "COMMIT");
		if (result == tagd::TAGD_OK) {
			_active = false;
			return result;
		}
	}
	const auto restored = rollback();
	return restored == tagd::TAGD_OK ? result : restored;
}

sqlite::write_transaction::~write_transaction() {
	// Callers finish explicitly to propagate cleanup errors; early exits still release ownership.
	if (_active)
		(void)rollback();
}

}
