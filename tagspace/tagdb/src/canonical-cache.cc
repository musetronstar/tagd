#include "tagdb/canonical-cache.h"
#include "tagd/hard-tags.h"

namespace tagdb {

void canonical_cache::clear() {
	_cache_index.clear();
	_cache.clear();
	_cache_bytes = 0;
	_operation_ranks.clear();
}

void canonical_cache::begin(bool valid) {
	_operation_ranks.clear();
	_cacheable = valid;
	if (!valid)
		clear();
}

void canonical_cache::end() {
	_operation_ranks.clear();
	_cacheable = false;
}

void canonical_cache::expire() {
	const auto now = _cache_now();
	// Last-use order lets expiry stop at the first entry that remains fresh.
	while (!_cache.empty() && now - _cache.back().used >= _cache_idle) {
		_cache_bytes -= _cache.back().bytes;
		_cache_index.erase(_cache.back().id);
		_cache.pop_back();
	}
}

canonical_cache::cache_metrics canonical_cache::cache_status() const {
	return {_cache.size(), _cache_bytes, _rank_reads, _rank_hits, _predicate_reads, _identity_reads,
		_identity_hits, _predicate_hits, _referent_reads, _referent_hits};
}

void canonical_cache::configure_cache(size_t bytes, size_t entries, cache_clock::duration idle,
	std::function<cache_clock::time_point()> now) {
	clear();
	_cache_byte_limit = bytes;
	_cache_entry_limit = entries;
	_cache_idle = idle;
	_cache_now = std::move(now);
}

tagd::code canonical_cache::rank(tagd::rank& result, tagd::id_view id,
	bool memoize, const rank_loader& load) {
	if (const auto* axiom = tagd::hard_tag_axiom_for(id)) {
		result = tagd::rank(axiom->packed_rank);
		return tagd::TAGD_OK;
	}

	if (memoize) {
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

	tagd::rank value;
	++_rank_reads;
	auto rc = load(value);
	const std::string key(id);
	if (rc != tagd::TAGD_OK && rc != tagd::TS_NOT_FOUND)
		return rc;
	if (rc == tagd::TAGD_OK)
		result = value;
	if (memoize)
		_operation_ranks.emplace(id, std::pair(rc, value));
	retain(cache_entry{.id = key, .value = value, .status = rc, .used = _cache_now()});

	return rc;
}

void canonical_cache::retain(cache_entry entry) {
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
	const auto [found, inserted] = _cache_index.emplace(_cache.front().id, _cache.begin());
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

tagd::code canonical_cache::identity(canonical_identity& result, tagd::rank& rank,
	const tagd::id_string& id, const identity_loader& load) {
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

	++_identity_reads;
	auto rc = load(result, rank);
	if (rc == tagd::TAGD_OK) {
		retain(cache_entry{.id = id, .value = rank, .status = rc, .used = _cache_now(),
			.identity = result});
	} else if (rc == tagd::TS_NOT_FOUND) {
		retain(cache_entry{.id = id, .value = {}, .status = rc, .used = _cache_now()});
	}
	return rc;
}

tagd::code canonical_cache::predicates(tagd::predicate_set& P, const tagd::id_string& id,
	bool direct, const predicate_loader& load,
	const std::function<tagd::code(tagd::predicate_set&)>& order) {
	std::optional<cache_entry> entry;
	std::vector<canonical_predicate> rows;
	bool loaded = false;

	// If no rank/identity entry fits the budget, direct rows bypass admission too.
	if (direct && _cacheable) {
		if (auto found = _cache_index.find(id); found != _cache_index.end()) {
			auto it = found->second;
			if (it->predicates) {
				++_predicate_hits;
				for (const auto& p : *it->predicates) {
					P.emplace(p.relator, p.object, p.modifier, p.opr8r, p.modifier_type);
				}
				it->used = _cache_now();
				_cache.splice(_cache.begin(), _cache, it);
				loaded = true;
			} else if (it->status == tagd::TAGD_OK) {
				// Rank resolution can evict this entry; retain an owned admission candidate.
				entry = *it;
			}
		}
	}

	if (!loaded) {
		auto rc = load(rows);
		if (rc != tagd::TAGD_OK)
			return rc;
		for (const auto& p : rows) {
			P.emplace(p.relator, p.object, p.modifier, p.opr8r, p.modifier_type);
		}
	}

	if (!P.empty()) {
		auto rc = order(P);
		if (rc != tagd::TAGD_OK)
			return rc;
	}

	if (entry) {
		// Publish only complete direct rows after successful loading and rank resolution.
		entry->predicates = std::move(rows);
		entry->used = _cache_now();
		retain(std::move(*entry));
	}

	return tagd::TAGD_OK;
}

} // namespace tagdb
