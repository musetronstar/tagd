#pragma once

#include "tagd.h"
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <optional>

namespace tagdb {

// One cache belongs to one opened backend; callers receive owned values, never entry references.
class canonical_cache {
public:
	using cache_clock = std::chrono::steady_clock;

	struct canonical_identity {
		tagd::id_string sub_relator, super_object;
		tagd::part_of_speech pos = tagd::POS_UNKNOWN;
	};

	// Stored columns exclude caller-specific labels, container indexes, and ordering keys.
	struct canonical_predicate {
		tagd::id_string relator, object, modifier;
		tagd::operator_t opr8r = tagd::OP_EQ;
		tagd::data_t modifier_type = tagd::TYPE_STRING;
	};

private:
	struct cache_entry {
		tagd::id_string id;
		tagd::rank value;
		tagd::code status = tagd::TS_NOT_FOUND;
		size_t bytes = 0;
		cache_clock::time_point used;
		std::optional<canonical_identity> identity = std::nullopt;
		// Disengaged means unloaded; an engaged empty vector is a complete empty result.
		std::optional<std::vector<canonical_predicate>> predicates = std::nullopt;
	};

	// A tagspace and its sessions require serialized access, including reads that mutate this cache.
	std::list<cache_entry> _cache;
	// Views refer to stable list-node IDs and are erased before the corresponding nodes.
	std::map<tagd::id_view, std::list<cache_entry>::iterator, std::less<>> _cache_index;
	size_t _cache_bytes = 0;

	static constexpr size_t DEFAULT_CACHE_BYTES = 256 * 1024;
	static constexpr size_t DEFAULT_CACHE_ENTRIES = 1024;
	static constexpr auto DEFAULT_CACHE_IDLE = std::chrono::minutes(5);

	size_t _cache_byte_limit = DEFAULT_CACHE_BYTES;
	size_t _cache_entry_limit = DEFAULT_CACHE_ENTRIES;
	cache_clock::duration _cache_idle = DEFAULT_CACHE_IDLE;
	std::function<cache_clock::time_point()> _cache_now = cache_clock::now;

	size_t _rank_reads = 0, _rank_hits = 0, _predicate_reads = 0, _identity_reads = 0;
	size_t _identity_hits = 0, _predicate_hits = 0;
	size_t _referent_reads = 0, _referent_hits = 0;

	bool _cacheable = false;
	std::map<tagd::id_string, std::pair<tagd::code, tagd::rank>, std::less<>> _operation_ranks;
	void retain(cache_entry);

public:
	canonical_cache() = default;
	canonical_cache(const canonical_cache&) = delete;
	canonical_cache& operator=(const canonical_cache&) = delete;
	canonical_cache(canonical_cache&&) = delete;
	canonical_cache& operator=(canonical_cache&&) = delete;

	struct cache_metrics {
		size_t entries, bytes, rank_reads, rank_hits, predicate_reads, identity_reads;
		size_t identity_hits, predicate_hits, referent_reads, referent_hits;
	};
	cache_metrics cache_status() const;

	void record_predicate_read() {
		++_predicate_reads;
	}

	void record_referent_read() {
		++_referent_reads;
	}

	void record_referent_hit() {
		++_referent_hits;
	}
	void configure_cache(size_t, size_t, cache_clock::duration,
		std::function<cache_clock::time_point()>);
	void clear();

	// Enable reuse only after the backend has pinned and validated its read snapshot.
	void begin(bool valid);
	void end();
	void expire();

	using rank_loader = std::function<tagd::code(tagd::rank&)>;
	using identity_loader = std::function<tagd::code(canonical_identity&, tagd::rank&)>;
	using predicate_loader = std::function<tagd::code(std::vector<canonical_predicate>&)>;
	tagd::code rank(tagd::rank&, tagd::id_view, bool memoize, const rank_loader&);
	tagd::code identity(canonical_identity&, tagd::rank&, const tagd::id_string&,
		const identity_loader&);
	tagd::code predicates(tagd::predicate_set&, const tagd::id_string&, bool direct,
		const predicate_loader&, const std::function<tagd::code(tagd::predicate_set&)>&);
};

} // namespace tagdb
