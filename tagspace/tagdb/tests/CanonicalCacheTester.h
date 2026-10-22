#pragma once

#include "TestCommon.h"
#include <filesystem>
#include <unistd.h>

class CanonicalCacheTester : public CxxTest::TestSuite {
	using database = tagdb::tagdb;

	void define(database& db, tagd::id_view id, tagd::id_view parent = HARD_TAG_ENTITY) {
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB, parent,
			tagd::POS_TAG), nullptr));
	}

	void relation(database& db, tagd::id_view id, tagd::id_view object, const std::string& value) {
		tagd::abstract_tag tag(id);
		TS_ASSERT_TAGD_OK(tag.relation(HARD_TAG_HAS, object, value));
		TS_ASSERT_TAGD_OK(db.put(tag, nullptr));
	}

public:
	void test_canonical_identity_cache_loads_on_demand() {
		auto owner = tagdb::make_backend();
		auto& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("subject", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));

		const auto initial = db.cache_status();
		tagd::rank rank;
		TS_ASSERT_TAGD_OK(db.lookup_rank(rank, "subject"));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, initial.identity_reads);
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, initial.predicate_reads);

		tagd::abstract_tag first, second;
		TS_ASSERT_TAGD_OK(db.get(first, "subject", nullptr));
		const auto loaded = db.cache_status();
		TS_ASSERT_EQUALS(loaded.identity_reads, initial.identity_reads + 1);
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, loaded.identity_reads);
		TS_ASSERT_EQUALS(first.str(), second.str());
		TS_ASSERT_EQUALS(first.rank(), rank);

		db.configure_cache(0, 0, std::chrono::minutes(1), std::chrono::steady_clock::now);
		const auto disabled = db.cache_status().identity_reads;
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, disabled + 2);
		TS_ASSERT_EQUALS(first.str(), second.str());
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
	}

	void test_expiry_preserves_recently_touched_entries() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		define(db, "old");
		define(db, "recent");
		auto now = std::chrono::steady_clock::time_point{};
		db.configure_cache(4096, 10, std::chrono::seconds(10), [&] {
			return now;
		});
		tagd::abstract_tag result;
		TS_ASSERT_TAGD_OK(db.get(result, "old", nullptr));
		TS_ASSERT_TAGD_OK(db.get(result, "recent", nullptr));
		now += std::chrono::seconds(5);
		TS_ASSERT_TAGD_OK(db.get(result, "recent", nullptr));
		now += std::chrono::seconds(5);
		const auto before = db.cache_status();

		TS_ASSERT_TAGD_OK(db.get(result, "recent", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().entries, 1);
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, before.identity_reads);
		TS_ASSERT_TAGD_OK(db.get(result, "old", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, before.identity_reads + 1);
	}

	void test_referent_memo_is_scoped_to_one_read_and_context() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		define(db, "subject");
		define(db, "context");
		define(db, "other_context");
		relation(db, "subject", HARD_TAG_MESSAGE, "literal");
		relation(db, "subject", HARD_TAG_LINE_NUMBER, "2");
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("has", HARD_TAG_HAS, "context"), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("owns", HARD_TAG_HAS, "other_context"), nullptr));
		auto first = test_session(db);
		auto second = test_session(db);
		TS_ASSERT_TAGD_OK(first.push_context("context"));
		TS_ASSERT_TAGD_OK(second.push_context("other_context"));

		tagd::abstract_tag result;
		for (auto* ssn : {&first, &first, &second}) {
			const auto before = db.cache_status();
			TS_ASSERT_TAGD_OK(db.get(result, "subject", ssn));
			TS_ASSERT_EQUALS(db.cache_status().referent_reads, before.referent_reads + 6);
			TS_ASSERT_EQUALS(db.cache_status().referent_hits, before.referent_hits + 1);
			TS_ASSERT(result.related(ssn == &first ? "has" : "owns", HARD_TAG_MESSAGE, "literal"));
		}

		TS_ASSERT_TAGD_OK(db.del(tagd::referent("has", HARD_TAG_HAS, "context"), nullptr));
		TS_ASSERT_TAGD_OK(db.get(result, "subject", &first));
		TS_ASSERT(result.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "literal"));
	}

	void test_complete_direct_and_empty_reads_are_reused() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		define(db, "subject");
		define(db, "empty");
		relation(db, "subject", HARD_TAG_MESSAGE, "literal");

		tagd::abstract_tag first, second;
		for (const auto id : {"subject", "empty"}) {
			TS_ASSERT_TAGD_OK(db.get(first, id, nullptr));
			const auto loaded = db.cache_status();
			TS_ASSERT_TAGD_OK(db.get(second, id, nullptr));
			TS_ASSERT_EQUALS(db.cache_status().predicate_reads, loaded.predicate_reads);
			TS_ASSERT_EQUALS(db.cache_status().identity_reads, loaded.identity_reads);
			TS_ASSERT_EQUALS(first.str(), second.str());
		}

		const auto loaded = db.cache_status();
		TS_ASSERT_EQUALS(db.get(second, "missing", nullptr, tagdb::F_NO_NOT_FOUND_ERROR),
			tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.get(second, "missing", nullptr, tagdb::F_NO_NOT_FOUND_ERROR),
			tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, loaded.predicate_reads);

		db.configure_cache(0, 0, std::chrono::minutes(1), std::chrono::steady_clock::now);
		TS_ASSERT_TAGD_OK(db.get(first, "subject", nullptr));
		const auto disabled = db.cache_status();
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, disabled.predicate_reads + 1);
		TS_ASSERT_EQUALS(first.str(), second.str());
	}

	void test_inherited_and_query_results_do_not_supply_direct_entries() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		define(db, "parent");
		define(db, "child", "parent");
		relation(db, "parent", HARD_TAG_MESSAGE, "ancestor");
		relation(db, "parent", HARD_TAG_LINE_NUMBER, "10");
		relation(db, "child", HARD_TAG_MESSAGE, "");
		relation(db, "child", tagd::HARD_TAG_FILE, "local");

		for (bool warm : {false, true}) {
			tagd::abstract_tag direct, inherited;
			if (warm) {
				TS_ASSERT_TAGD_OK(db.get(direct, "child", nullptr));
			}
			TS_ASSERT_TAGD_OK(db.get(inherited, "child", nullptr,
				tagdb::F_INCLUDE_INHERITED_PREDICATES));
			TS_ASSERT_EQUALS(inherited.relations.size(), 3);
			TS_ASSERT(inherited.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, ""));
			TS_ASSERT(inherited.related(HARD_TAG_HAS, HARD_TAG_LINE_NUMBER, "10"));

			tagd::interrogator query(HARD_TAG_INTERROGATOR);
			TS_ASSERT_TAGD_OK(query.relation(HARD_TAG_HAS, tagd::HARD_TAG_FILE, "local"));
			tagd::tag_set selected;
			TS_ASSERT_TAGD_OK(db.query(selected, query, nullptr));
			TS_ASSERT_EQUALS(selected.size(), 1);
			if (!selected.empty()) {
				TS_ASSERT_EQUALS(selected.begin()->relations.size(), 1);
			}

			TS_ASSERT_TAGD_OK(db.get(direct, "child", nullptr));
			TS_ASSERT_EQUALS(direct.relations.size(), 2);
			TS_ASSERT(!direct.related(HARD_TAG_HAS, HARD_TAG_LINE_NUMBER));
		}
	}

	void test_contexts_sessions_and_url_reconstruction_on_hits() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		for (const auto id : {"subject", "object", "first_context", "second_context"}) {
			define(db, id);
		}
		relation(db, "subject", "object", "alias");
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("alias", "object", "first_context"), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("selected", "subject", "first_context"), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("other", "object", "second_context"), nullptr));

		auto first = test_session(db);
		auto second = test_session(db);
		TS_ASSERT_TAGD_OK(first.push_context("first_context"));
		TS_ASSERT_TAGD_OK(second.push_context("second_context"));
		auto copy = first;
		tagd::abstract_tag loaded;

		TS_ASSERT_TAGD_OK(db.get(loaded, "selected", &first));
		const auto reads = db.cache_status().predicate_reads;

		TS_ASSERT_TAGD_OK(db.get(loaded, "selected", &copy));
		TS_ASSERT(loaded.related(HARD_TAG_REFERS_TO, "subject"));
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "alias", "alias"));

		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", &second));
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "other", "alias"));

		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "object", "alias"));
		TS_ASSERT(!loaded.related(HARD_TAG_REFERS_TO, "subject"));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, reads);

		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", &first, tagdb::F_NO_TRANSFORM_REFERENTS));
		TS_ASSERT_EQUALS(loaded.id(), "subject");
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "object", "alias"));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, reads);

		TS_ASSERT_EQUALS(db.del(tagd::abstract_tag("object"), nullptr),
			tagd::TS_RELATION_DEPENDENCY);
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);

		TS_ASSERT_TAGD_OK(db.get(loaded, "selected", &first));
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "alias", "alias"));
		TS_ASSERT_TAGD_OK(db.del(tagd::referent("alias", "object", "first_context"), nullptr));

		TS_ASSERT_TAGD_OK(db.get(loaded, "selected", &first));
		TS_ASSERT(loaded.related(HARD_TAG_HAS, "object", "alias"));

		tagd::url url("https://example.com/path?q=one");
		TS_ASSERT_TAGD_OK(url.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, "page"));
		TS_ASSERT_TAGD_OK(db.put(url, nullptr));
		tagd::url fetched;

		TS_ASSERT_TAGD_OK(db.get(fetched, url.id(), nullptr));
		const auto url_reads = db.cache_status().predicate_reads;

		TS_ASSERT_TAGD_OK(db.get(fetched, url.id(), nullptr));
		TS_ASSERT_EQUALS(fetched.id(), url.id());
		TS_ASSERT_EQUALS(fetched.hduri(), url.hduri());
		TS_ASSERT(fetched.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "page"));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, url_reads);
	}

	void test_entry_growth_budget_expiry_and_retained_snapshots() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		define(db, "small");
		define(db, "large");
		relation(db, "small", HARD_TAG_MESSAGE, "small");
		relation(db, "large", HARD_TAG_MESSAGE, std::string(8192, 'x'));
		auto now = std::chrono::steady_clock::time_point{};
		db.configure_cache(4096, 2, std::chrono::seconds(10), [&now] {
			return now;
		});

		tagd::abstract_tag small, large, result;

		TS_ASSERT_TAGD_OK(db.get(small, "small", nullptr));
		const auto warm = db.cache_status().predicate_reads;

		TS_ASSERT_TAGD_OK(db.get(result, "small", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, warm);

		TS_ASSERT_TAGD_OK(db.get(large, "large", nullptr));
		const auto oversized = db.cache_status().predicate_reads;

		TS_ASSERT_TAGD_OK(db.get(result, "large", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, oversized + 1);
		TS_ASSERT(db.cache_status().bytes <= 4096);
		TS_ASSERT(db.cache_status().entries <= 2);

		now += std::chrono::seconds(11);

		TS_ASSERT_TAGD_OK(db.get(result, "small", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, oversized + 2);

		db.close();
		TS_ASSERT_EQUALS(result.str(), small.str());
		TS_ASSERT_EQUALS(large.relations.begin()->modifier.size(), 8192);
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		TS_ASSERT_EQUALS(db.get(result, "small", nullptr), tagd::TS_NOT_FOUND);
	}

	void test_oversized_identity_does_not_discard_loaded_empty_predicates() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		const std::string parent(1800, 'p');
		define(db, parent);
		define(db, "subject", parent);
		tagd::rank rank;
		TS_ASSERT_TAGD_OK(db.lookup_rank(rank, "subject"));
		const auto rank_bytes = db.cache_status().bytes;
		db.configure_cache(rank_bytes + 64, 10, std::chrono::minutes(1),
			std::chrono::steady_clock::now);
		TS_ASSERT_TAGD_OK(db.lookup_rank(rank, "subject"));

		tagd::abstract_tag loaded;
		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		const auto first = db.cache_status();
		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		TS_ASSERT_EQUALS(loaded.super_object(), parent);
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, first.identity_reads + 1);
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, first.predicate_reads);
		TS_ASSERT(db.cache_status().bytes <= rank_bytes + 64);
	}

	void test_growing_direct_entries_evict_least_recently_used_data() {
		auto owner = tagdb::make_backend();
		database& db = *owner;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		for (const auto id : {"one", "two"}) {
			define(db, id);
			relation(db, id, HARD_TAG_MESSAGE, std::string(1024, 'v'));
		}
		db.configure_cache(65536, 10, std::chrono::minutes(1), std::chrono::steady_clock::now);
		tagd::abstract_tag loaded;
		TS_ASSERT_TAGD_OK(db.get(loaded, "one", nullptr));
		const auto full_bytes = db.cache_status().bytes;
		const auto budget = full_bytes + full_bytes / 2;
		db.configure_cache(budget, 10, std::chrono::minutes(1), std::chrono::steady_clock::now);

		TS_ASSERT_TAGD_OK(db.get(loaded, "one", nullptr));
		TS_ASSERT_TAGD_OK(db.get(loaded, "two", nullptr));
		TS_ASSERT(db.cache_status().bytes <= budget);
		const auto reads = db.cache_status().predicate_reads;
		TS_ASSERT_TAGD_OK(db.get(loaded, "two", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, reads);
		TS_ASSERT_TAGD_OK(db.get(loaded, "one", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, reads + 1);
		TS_ASSERT(db.cache_status().bytes <= budget);
	}

};
