#include "tagdb/sqlite.h"

namespace tagdb {

int sqlite::data_version() {
	if (prepare(stmt_t::DATA_VERSION, "PRAGMA data_version", "data version") != tagd::TAGD_OK)
		return -1;

	auto* stmt = get_stmt(stmt_t::DATA_VERSION);
	const int version = sqlite3_step(stmt) == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
	sqlite3_reset(stmt);
	return version;
}

tagd::code sqlite::snapshot_statement(stmt_t key, const char* sql) {
	auto rc = prepare(key, sql, "read snapshot");
	if (rc != tagd::TAGD_OK)
		return rc;

	auto* stmt = get_stmt(key);
	int step;
	while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
		// Stepping the pinning SELECT establishes a snapshot even when the table is empty.
	}
	sqlite3_reset(stmt);
	if (step != SQLITE_DONE)
		return ferror(tagd::TS_INTERNAL_ERR, "read snapshot %s failed: %s", sql, sqlite3_errmsg(_db));
	return tagd::TAGD_OK;
}

void sqlite::clear_cache() {
	_canonical.clear();
	_operation_refers.clear();
	_operation_refers_context.clear();
}

sqlite::cache_metrics sqlite::cache_status() const {
	return _canonical.cache_status();
}

void sqlite::configure_cache(size_t bytes, size_t entries, cache_clock::duration idle,
	std::function<cache_clock::time_point()> now) {
	clear_cache();
	_canonical.configure_cache(bytes, entries, idle, std::move(now));
}

sqlite::mutation_scope::mutation_scope(sqlite& owner) : _owner(owner) {
	++_owner._mutation_depth;
	_owner.clear_cache();
}

sqlite::mutation_scope::~mutation_scope() {
	// A failed write can change storage before returning or rolling back.
	_owner.clear_cache();
	--_owner._mutation_depth;
	if (_owner._mutation_depth == 0 && _owner._read_depth == 0) {
		// Completed write cursors must not hold a read lock against other connections.
		for (auto& [key, stmt] : _owner._stmts) {
			if (sqlite3_stmt_busy(stmt))
				sqlite3_reset(stmt);
		}
	}
}

tagd::code sqlite::begin_read() {
	if (_read_depth++ != 0)
		return tagd::TAGD_OK;

	_canonical.end();
	_operation_refers.clear();
	_operation_refers_context.clear();
	_cacheable = false;
	_read_transaction = false;

	if (!_db)
		return ferror(tagd::TS_MISUSE, "rank lookup requires open storage");

	// Internal mutation reads and externally owned transactions never publish cache entries.
	if (_mutation_depth || _doing_init || !sqlite3_get_autocommit(_db)) {
		clear_cache();
		return tagd::TAGD_OK;
	}

	for (auto& [key, stmt] : _stmts) {
		if (sqlite3_stmt_busy(stmt))
			sqlite3_reset(stmt);
	}

	const int version = data_version();
	if (version < 0)
		return ferror(tagd::TS_INTERNAL_ERR, "cannot read database version");

	const auto changes = sqlite3_total_changes64(_db);
	if (version != _cache_data_version || changes != _cache_changes)
		clear_cache();
	_cache_data_version = version;
	_cache_changes = changes;

	if (snapshot_statement(stmt_t::READ_BEGIN, "BEGIN") != tagd::TAGD_OK)
		return ferror(tagd::TS_INTERNAL_ERR, "cannot begin rank snapshot: %s", sqlite3_errmsg(_db));
	_read_transaction = true;

	/*
	 * Pin the database snapshot before admitting cached ranks. Checking data_version
	 * alone leaves a race between the check and the first predicate/rank read.
	 * Recheck after pinning and again after releasing this transaction.
	 */
	if (snapshot_statement(stmt_t::READ_PIN, "SELECT tag FROM tags LIMIT 1") != tagd::TAGD_OK)
		return ferror(tagd::TS_INTERNAL_ERR, "cannot pin rank snapshot: %s", sqlite3_errmsg(_db));
	_cacheable = data_version() == version;
	if (!_cacheable)
		clear_cache();

	_canonical.begin(_cacheable);
	_canonical.expire();

	return tagd::TAGD_OK;
}

void sqlite::end_read() {
	if (--_read_depth != 0)
		return;

	_canonical.end();
	_operation_refers.clear();
	_operation_refers_context.clear();
	if (_read_transaction) {
		for (auto& [key, stmt] : _stmts) {
			if (sqlite3_stmt_busy(stmt))
				sqlite3_reset(stmt);
		}

		// These transactions are read-only; rollback releases the snapshot on every return path.
		if (snapshot_statement(stmt_t::READ_ROLLBACK, "ROLLBACK") != tagd::TAGD_OK) {
			// Do not let a failed read cleanup turn later writes into part of this transaction.
			close();
			ferror(tagd::TS_INTERNAL_ERR, "cannot release read snapshot; storage closed");
		} else if (data_version() != _cache_data_version)
			clear_cache();
	}

	_read_transaction = false;
	_cacheable = false;
}

tagd::code sqlite::lookup_rank(tagd::rank& result, tagd::id_view id) {
	if (_read_depth == 0 && _mutation_depth == 0)
		reset(nullptr);

	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;

	return _canonical.rank(result, id, !_mutation_depth, [&](tagd::rank& value) {
		// Read only the identity rank; loading predicates here would recursively order whole tags.
		auto rc = prepare(stmt_t::RANK, "SELECT rank FROM tags WHERE tag = tid(?)", "rank lookup");
		if (rc != tagd::TAGD_OK)
			return rc;

		const std::string key(id);
		rc = bind_text(stmt_t::RANK, 1, key.c_str(), "rank id");
		if (rc != tagd::TAGD_OK)
			return rc;

		auto* stmt = get_stmt(stmt_t::RANK);
		const int step = sqlite3_step(stmt);
		if (step == SQLITE_ROW) {
			rc = value.init(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
		} else if (step == SQLITE_DONE) {
			rc = tagd::TS_NOT_FOUND;
		} else {
			rc = ferror(tagd::TS_INTERNAL_ERR, "rank lookup failed for %s: %s",
				key.c_str(), sqlite3_errmsg(_db));
		}

		sqlite3_reset(stmt);
		return rc;
	});
}

tagd::code sqlite::load_identity(canonical_identity& result, tagd::rank& rank,
	const tagd::id_string& id) {
	return _canonical.identity(result, rank, id, [&](auto& result, auto& rank) {
		auto rc = prepare(stmt_t::GET,
			"SELECT pos, idt(sub_relator), idt(super_object), rank FROM tags WHERE tag = tid(?)",
			"get_tag");
		if (rc != tagd::TAGD_OK)
			return rc;
		rc = bind_text(stmt_t::GET, 1, id.c_str(), "get_tag_id");
		if (rc != tagd::TAGD_OK)
			return rc;

		auto* stmt = get_stmt(stmt_t::GET);
		const auto step = sqlite3_step(stmt);
		if (step == SQLITE_ROW) {
			result = {reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
				reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
				static_cast<tagd::part_of_speech>(sqlite3_column_int(stmt, 0))};
			rc = rank.init(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
		} else if (step == SQLITE_DONE) {
			rc = tagd::TS_NOT_FOUND;
		} else {
			rc = ferror(tagd::TS_INTERNAL_ERR, "get_tag failed for %s: %s",
				id.c_str(), sqlite3_errmsg(_db));
		}

		sqlite3_reset(stmt);
		return rc;
	});
}

tagd::code sqlite::order_predicates(tagd::predicate_set& values) {
	if (_read_depth == 0 && _mutation_depth == 0)
		reset(nullptr);

	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	return values.order([this](tagd::rank& r, tagd::id_view id) {
		auto rc = lookup_rank(r, id);
		if (rc != tagd::TAGD_OK) {
			return ferror(rc, "cannot order predicate component: %.*s",
				static_cast<int>(id.size()), id.data());
		}
		return rc;
	});
}

} // namespace tagdb
