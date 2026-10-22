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
	_cache_index.clear();
	_cache.clear();
	_cache_bytes = 0;
	_operation_ranks.clear();
	_operation_refers.clear();
	_operation_refers_context.clear();
}

sqlite::cache_metrics sqlite::cache_status() const {
	return {_cache.size(), _cache_bytes, _rank_reads, _rank_hits, _predicate_reads, _identity_reads,
		_identity_hits, _predicate_hits, _referent_reads, _referent_hits};
}

void sqlite::configure_cache(size_t bytes, size_t entries, cache_clock::duration idle,
	std::function<cache_clock::time_point()> now) {
	clear_cache();
	_cache_byte_limit = bytes;
	_cache_entry_limit = entries;
	_cache_idle = idle;
	_cache_now = std::move(now);
}

sqlite::read_scope::read_scope(sqlite& owner)
	: _owner(owner), status(owner.begin_read()) {
}

sqlite::read_scope::~read_scope() {
	_owner.end_read();
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

	_operation_ranks.clear();
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

	const auto now = _cache_now();
	// Every touch moves an entry to the front with a monotonic timestamp.
	while (!_cache.empty() && now - _cache.back().used >= _cache_idle) {
		_cache_bytes -= _cache.back().bytes;
		_cache_index.erase(_cache.back().id);
		_cache.pop_back();
	}

	return tagd::TAGD_OK;
}

void sqlite::end_read() {
	if (--_read_depth != 0)
		return;

	_operation_ranks.clear();
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

	if (const auto* axiom = tagd::hard_tag_axiom_for(id)) {
		result = tagd::rank(axiom->packed_rank);
		return tagd::TAGD_OK;
	}

	if (!_mutation_depth) {
		if (auto it = _operation_ranks.find(id); it != _operation_ranks.end()) {
			if (it->second.first == tagd::TAGD_OK)
				result = it->second.second;
			return it->second.first;
		}
	}

	if (_cacheable) {
		if (auto found = _cache_index.find(id); found != _cache_index.end()) {
			auto it = found->second;
			++_rank_hits;
			it->used = _cache_now();
			const auto rc = it->status;
			if (rc == tagd::TAGD_OK)
				result = it->value;
			_operation_ranks.emplace(id, std::pair(rc, it->value));
			_cache.splice(_cache.begin(), _cache, it);
			return rc;
		}
	}

	// Read only the identity rank; loading predicates here would recursively order whole tags.
	auto rc = prepare(stmt_t::RANK, "SELECT rank FROM tags WHERE tag = tid(?)", "rank lookup");
	if (rc != tagd::TAGD_OK)
		return rc;
	const std::string key(id);
	rc = bind_text(stmt_t::RANK, 1, key.c_str(), "rank id");
	if (rc != tagd::TAGD_OK)
		return rc;
	auto* stmt = get_stmt(stmt_t::RANK);
	++_rank_reads;
	const int step = sqlite3_step(stmt);
	tagd::rank value;
	if (step == SQLITE_ROW) {
		rc = value.init(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	} else if (step == SQLITE_DONE) {
		rc = tagd::TS_NOT_FOUND;
	} else {
		rc = ferror(tagd::TS_INTERNAL_ERR, "rank lookup failed for %s: %s",
			key.c_str(), sqlite3_errmsg(_db));
	}
	sqlite3_reset(stmt);
	if (rc != tagd::TAGD_OK && rc != tagd::TS_NOT_FOUND)
		return rc;
	if (rc == tagd::TAGD_OK)
		result = value;
	if (!_mutation_depth)
		_operation_ranks.emplace(id, std::pair(rc, value));
	retain(cache_entry{.id = key, .value = value, .status = rc, .used = _cache_now()});

	return rc;
}

void sqlite::retain(cache_entry entry) {
	if (!_cacheable || !_cache_entry_limit)
		return;

	// Missing tags cannot own successfully loaded identity fields or direct predicates.
	if (entry.status != tagd::TAGD_OK) {
		entry.identity.reset();
		entry.predicates.reset();
	}

	// Components loaded within this snapshot remain available when another component grows.
	if (auto found = _cache_index.find(entry.id); found != _cache_index.end()) {
		const auto& previous = *found->second;
		if (entry.status == tagd::TAGD_OK && previous.status == tagd::TAGD_OK) {
			if (!entry.identity)
				entry.identity = previous.identity;
			if (!entry.predicates)
				entry.predicates = previous.predicates;
		}
	}

	// Include owned capacities and tree/list links; allocator overhead varies.
	entry.bytes = sizeof(cache_entry) + 2 * sizeof(void*) + entry.id.capacity() + 1
		+ entry.value.capacity() + 1
		+ sizeof(decltype(_cache_index)::value_type) + 4 * sizeof(void*);
	if (entry.identity) {
		entry.bytes += entry.identity->sub_relator.capacity() + 1
			+ entry.identity->super_object.capacity() + 1;
	}
	if (entry.predicates) {
		/*
		 * Retain only raw canonical rows, not predicate_set indexes or shared rank keys.
		 * Returned sets own their indexes and resolve ranks within their read snapshot.
		 */
		entry.bytes += entry.predicates->capacity() * sizeof(canonical_predicate);
		for (const auto& p : *entry.predicates) {
			entry.bytes += p.relator.capacity() + p.object.capacity() + p.modifier.capacity() + 3;
		}
	}
	if (entry.bytes > _cache_byte_limit)
		return;

	// Publish the replacement only after both owning node and index allocation succeed.
	_cache.push_front(std::move(entry));
	decltype(_cache_index)::iterator found;
	bool inserted;
	try {
		std::tie(found, inserted) = _cache_index.emplace(_cache.front().id, _cache.begin());
	} catch (...) {
		_cache.pop_front();
		throw;
	}
	if (!inserted) {
		auto old = found->second;
		_cache_bytes -= old->bytes;
		auto node = _cache_index.extract(found);
		node.key() = _cache.front().id;
		node.mapped() = _cache.begin();
		_cache_index.insert(std::move(node));
		_cache.erase(old);
	}
	_cache_bytes += _cache.front().bytes;
	while (_cache.size() > _cache_entry_limit || _cache_bytes > _cache_byte_limit) {
		_cache_bytes -= _cache.back().bytes;
		_cache_index.erase(_cache.back().id);
		_cache.pop_back();
	}
}

tagd::code sqlite::load_identity(canonical_identity& result, tagd::rank& rank,
	const tagd::id_string& id) {
	if (_cacheable) {
		if (auto found = _cache_index.find(id); found != _cache_index.end()) {
			auto it = found->second;
			if (it->identity) {
				++_identity_hits;
				result = *it->identity;
				rank = it->value;
				it->used = _cache_now();
				_cache.splice(_cache.begin(), _cache, it);
				return tagd::TAGD_OK;
			}
			if (it->status == tagd::TS_NOT_FOUND)
				return tagd::TS_NOT_FOUND;
		}
	}

	auto rc = prepare(stmt_t::GET,
		"SELECT pos, idt(sub_relator), idt(super_object), rank FROM tags WHERE tag = tid(?)",
		"get_tag");
	if (rc != tagd::TAGD_OK)
		return rc;
	rc = bind_text(stmt_t::GET, 1, id.c_str(), "get_tag_id");
	if (rc != tagd::TAGD_OK)
		return rc;

	auto* stmt = get_stmt(stmt_t::GET);
	++_identity_reads;
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
	if (rc == tagd::TAGD_OK) {
		retain(cache_entry{.id = id, .value = rank, .status = rc, .used = _cache_now(),
			.identity = result});
	} else if (rc == tagd::TS_NOT_FOUND) {
		retain(cache_entry{.id = id, .value = {}, .status = rc, .used = _cache_now()});
	}
	return rc;
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
