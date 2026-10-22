#pragma once

#include "TestCommon.h"
#include "tagdb/sqlite.h"
#include <filesystem>
#include <unistd.h>

class SqliteCacheTester : public CxxTest::TestSuite {
	class database : public tagdb::sqlite {
	public:
		using ::tagdb::sqlite::configure_cache;
		using ::tagdb::sqlite::cache_status;
		using ::tagdb::sqlite::exec;
		using ::tagdb::sqlite::get_stmt;

		void deny_rollback() {
			sqlite3_set_authorizer(_db, [](void*, int action, const char* operation,
				const char*, const char*, const char*) {
				return action == SQLITE_TRANSACTION && std::string_view(operation) == "ROLLBACK"
					? SQLITE_DENY : SQLITE_OK;
			}, nullptr);
		}

		bool open_storage() const {
			return _db != nullptr;
		}
	};

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
	void test_snapshot_statements_are_reused_and_hits_are_counted() {
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		define(db, "subject");
		tagd::abstract_tag result;
		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));
		const auto before = db.cache_status();
		auto* version = db.get_stmt(tagdb::stmt_t::DATA_VERSION);
		TS_ASSERT(version != nullptr);

		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));
		TS_ASSERT_EQUALS(db.get_stmt(tagdb::stmt_t::DATA_VERSION), version);
		TS_ASSERT_EQUALS(db.cache_status().identity_hits, before.identity_hits + 1);
		TS_ASSERT_EQUALS(db.cache_status().predicate_hits, before.predicate_hits + 1);
		for (const auto key : {tagdb::stmt_t::DATA_VERSION, tagdb::stmt_t::READ_BEGIN,
			tagdb::stmt_t::READ_PIN, tagdb::stmt_t::READ_ROLLBACK}) {
			TS_ASSERT(db.get_stmt(key) != nullptr);
			TS_ASSERT(!sqlite3_stmt_busy(db.get_stmt(key)));
		}
	}

	void test_failed_snapshot_cleanup_closes_storage_and_records_error() {
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		define(db, "subject");
		bool deny = false;
		db.configure_cache(4096, 10, std::chrono::minutes(1), [&] {
			// The clock is sampled after pinning, so denial targets cleanup of an owned read.
			if (deny) {
				deny = false;
				db.deny_rollback();
			}
			return std::chrono::steady_clock::now();
		});
		tagd::abstract_tag result;
		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));

		deny = true;
		(void)db.get(result, "subject", nullptr);
		TS_ASSERT_EQUALS(db.code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(!db.open_storage());
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_EQUALS(db.get(result, "subject", nullptr), tagd::TS_MISUSE);
	}

	void test_external_commit_preserves_current_snapshot_and_invalidates_next_read() {
		char pattern[] = "/tmp/tagd-canonical-cache-XXXXXX";
		auto* directory = mkdtemp(pattern);
		TS_ASSERT(directory != nullptr);
		if (!directory)
			return;
		struct cleanup {
			std::filesystem::path path;
			~cleanup() {
				std::filesystem::remove_all(path);
			}
		} remove{directory};
		const auto file = (remove.path / "tags.db").string();
		database first, second;
		TS_ASSERT_TAGD_OK(first.init(file));
		TS_ASSERT_TAGD_OK(first.exec("PRAGMA journal_mode=WAL"));
		define(first, "parent");
		define(first, "subject");
		relation(first, "subject", HARD_TAG_MESSAGE, "before");
		TS_ASSERT_TAGD_OK(second.init(file));

		bool mutate = false;
		// begin_read samples the clock after pinning; the external write must occur after that pin.
		first.configure_cache(65536, 10, std::chrono::minutes(1), [&] {
			if (mutate) {
				mutate = false;
				define(second, "subject", "parent");
				relation(second, "subject", HARD_TAG_MESSAGE, "after");
			}
			return std::chrono::steady_clock::now();
		});
		tagd::abstract_tag original, during, after;
		TS_ASSERT_TAGD_OK(first.get(original, "subject", nullptr));

		mutate = true;
		TS_ASSERT_TAGD_OK(first.get(during, "subject", nullptr));
		TS_ASSERT(!mutate);
		TS_ASSERT_EQUALS(during.str(), original.str());

		TS_ASSERT_TAGD_OK(first.get(after, "subject", nullptr));
		TS_ASSERT_EQUALS(after.super_object(), "parent");
		TS_ASSERT(after.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "after"));
		TS_ASSERT(original.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "before"));
		TS_ASSERT_TAGD_OK(second.del(tagd::abstract_tag("subject"), nullptr));
		TS_ASSERT_EQUALS(first.get(after, "subject", nullptr), tagd::TS_NOT_FOUND);
	}

	void test_failures_and_rollback_do_not_publish_predicates() {
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		define(db, "subject");
		relation(db, "subject", HARD_TAG_MESSAGE, "original");
		tagd::abstract_tag original, loaded;
		TS_ASSERT_TAGD_OK(db.get(original, "subject", nullptr));

		TS_ASSERT_TAGD_OK(db.exec("BEGIN"));
		TS_ASSERT_TAGD_OK(db.exec("DELETE FROM relations WHERE subject=tid('subject')"));
		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		TS_ASSERT(loaded.relations.empty());
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_TAGD_OK(db.exec("ROLLBACK"));
		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		TS_ASSERT_EQUALS(loaded.str(), original.str());

		TS_ASSERT_TAGD_OK(db.exec("ALTER TABLE relations RENAME TO hidden_relations"));
		TS_ASSERT_DIFFERS(db.get(loaded, "subject", nullptr), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(db.get(loaded, "subject", nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.str(), original.str());
		TS_ASSERT_TAGD_OK(db.exec("ALTER TABLE hidden_relations RENAME TO relations"));
		TS_ASSERT_TAGD_OK(db.get(loaded, "subject", nullptr));
		TS_ASSERT_EQUALS(loaded.str(), original.str());

		TS_ASSERT_TAGD_OK(db.del(tagd::abstract_tag("subject"), nullptr));
		TS_ASSERT_EQUALS(db.get(loaded, "subject", nullptr), tagd::TS_NOT_FOUND);
	}
};
