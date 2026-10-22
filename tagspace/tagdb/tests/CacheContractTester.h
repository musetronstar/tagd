#pragma once

#include <cxxtest/TestSuite.h>
#include "tagdb/canonical-cache.h"
#include "tagd/hard-tags.h"

class CacheContractTester : public CxxTest::TestSuite {
public:
	void test_components_expiry_and_snapshot_admission() {
		tagdb::canonical_cache cache;
		using clock = tagdb::canonical_cache::cache_clock;
		auto now = clock::time_point{};
		cache.configure_cache(4096, 2, std::chrono::seconds(10), [&] {
			return now;
		});
		size_t loads = 0;
		auto load = [&](tagd::rank& rank) {
			++loads;
			return rank.init("a");
		};

		cache.begin(true);
		tagd::rank first, next;
		TS_ASSERT_EQUALS(cache.rank(first, "subject", true, load), tagd::TAGD_OK);
		cache.end();
		cache.begin(true);
		TS_ASSERT_EQUALS(cache.rank(next, "subject", true, load), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loads, 1);

		cache.end();
		now += std::chrono::seconds(11);
		cache.begin(true);
		cache.expire();
		TS_ASSERT_EQUALS(cache.rank(next, "subject", true, load), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loads, 2);

		// A backend that cannot validate the snapshot must neither reuse nor retain data.
		cache.end();
		cache.begin(false);
		TS_ASSERT_EQUALS(cache.rank(next, "subject", false, load), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cache.cache_status().entries, 0);
		TS_ASSERT_EQUALS(first, next);
	}

	void test_failed_loads_and_partial_predicates_are_not_retained() {
		tagdb::canonical_cache cache;
		cache.begin(true);
		tagd::rank rank;
		TS_ASSERT_EQUALS(cache.rank(rank, "broken", true, [](auto&) {
			return tagd::TS_INTERNAL_ERR;
		}), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(cache.cache_status().entries, 0);

		tagdb::canonical_cache::canonical_identity identity;
		TS_ASSERT_EQUALS(cache.identity(identity, rank, "subject", [](auto& value, auto& r) {
			value = {std::string(HARD_TAG_SUB), std::string(HARD_TAG_ENTITY), tagd::POS_TAG};
			return r.init("a");
		}), tagd::TAGD_OK);
		size_t loads = 0;
		auto load = [&](auto&) {
			++loads;
			return tagd::TAGD_OK;
		};
		auto order = [](auto&) {
			return tagd::TAGD_OK;
		};
		tagd::predicate_set values;

		TS_ASSERT_EQUALS(cache.predicates(values, "subject", false, load, order), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cache.predicates(values, "subject", true, load, order), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cache.predicates(values, "subject", true, load, order), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loads, 2);
	}
};
