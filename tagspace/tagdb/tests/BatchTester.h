#pragma once
#include "TestCommon.h"
#include "tagdb/sqlite.h"
#include <array>
#include <filesystem>
#include <unistd.h>

class BatchTester : public CxxTest::TestSuite {
	class database : public tagdb::sqlite {
	public:
		using ::tagdb::sqlite::exec;
		int synchronous() {
			sqlite3_stmt* statement = nullptr;
			sqlite3_prepare_v2(_db, "PRAGMA synchronous", -1, &statement, nullptr);
			const int result = sqlite3_step(statement) == SQLITE_ROW
				? sqlite3_column_int(statement, 0) : -1;
			sqlite3_finalize(statement);
			return result;
		}
		void deny_finish() {
			sqlite3_set_authorizer(_db, [](void*, int action, const char* name,
				const char*, const char*, const char*) {
				return action == SQLITE_TRANSACTION && std::string_view(name) != "BEGIN"
					? SQLITE_DENY : SQLITE_OK;
			}, nullptr);
		}
	};
public:
	void test_type_relation_batch_validates_the_final_affected_descriptions() {
		database db;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		for (const auto id : {"legs", "source", "dog"}) {
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}
		tagd::abstract_tag source("source"), dog("dog");
		TS_ASSERT_TAGD_OK(source.relation(HARD_TAG_HAS, "legs", "four"));
		TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "legs", "four"));
		TS_ASSERT_TAGD_OK(dog.type_relation(tagd::HARD_TAG_TYPE_OF, "source"));
		TS_ASSERT_TAGD_OK(db.put(source, nullptr));
		TS_ASSERT_TAGD_OK(db.put(dog, nullptr));
		source.relations.clear();
		dog.relations.clear();
		dog.type_relations.clear();
		TS_ASSERT_TAGD_OK(source.relation(HARD_TAG_HAS, "legs", "two"));
		TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "legs", "two"));
		TS_ASSERT_EQUALS(db.put(source, nullptr), tagd::TS_CONTRADICTION);
		std::array<const tagd::abstract_tag*, 2> compatible{&source, &dog};
		TS_ASSERT_TAGD_OK(db.put_batch(compatible, nullptr));
		tagd::abstract_tag read;
		TS_ASSERT_TAGD_OK(db.get(read, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES));
		TS_ASSERT_EQUALS(read.relations.size(), 1u);
		TS_ASSERT(read.related(HARD_TAG_HAS, "legs", "two"));
		std::ostringstream before, after;
		TS_ASSERT_TAGD_OK(db.dump(before));
		source.relations.clear();
		TS_ASSERT_TAGD_OK(source.relation(HARD_TAG_HAS, "legs", "three"));
		TS_ASSERT_EQUALS(db.put_batch(compatible, nullptr, tagd::F_IGNORE_DUPLICATES),
			tagd::TS_CONTRADICTION);
		TS_ASSERT_TAGD_OK(db.dump(after));
		TS_ASSERT_EQUALS(before.str(), after.str());
	}

	void test_integrated_order_matches_structural_and_inherited_type_paths() {
		database db;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		for (const auto id : {"kind", "other"})
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB, HARD_TAG_ENTITY,
				tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("specific", HARD_TAG_SUB, "kind", tagd::POS_TAG), nullptr));
		tagd::abstract_tag parent("parent", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_TAGD_OK(parent.type_relation(tagd::HARD_TAG_TYPE_OF, "specific"));
		TS_ASSERT_TAGD_OK(db.put(parent, nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("child", HARD_TAG_SUB, "parent", tagd::POS_TAG), nullptr));
		tagd::abstract_tag leaf("leaf", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_TAGD_OK(leaf.type_relation(tagd::HARD_TAG_TYPE_OF, "child"));
		TS_ASSERT_TAGD_OK(db.put(leaf, nullptr));
		const std::array<tagd::id_view, 5> requested{"leaf", "kind", "_entity", "other", "missing"};
		std::set<tagd::id_string> inherited;
		TS_ASSERT_TAGD_OK(db.inherited_sources(inherited, "leaf", requested));
		TS_ASSERT_EQUALS(inherited, (std::set<tagd::id_string>{"leaf", "kind", "_entity"}));
		TS_ASSERT_TAGD_OK(db.inherited_sources(inherited, "missing", requested));
		TS_ASSERT(inherited.empty());
		for (const auto object : {"leaf", "child", "parent", "specific", "kind", "_entity", "other", "missing"}) {
			bool matches = false;
			TS_ASSERT_TAGD_OK(db.inherits(matches, "leaf", object));
			TS_ASSERT_EQUALS(matches, std::string(object) != "other" && std::string(object) != "missing");
			if (std::string(object) == "missing")
				continue;
			tagd::interrogator query(tagd::HARD_TAG_WHAT);
			TS_ASSERT_TAGD_OK(query.type_relation(tagd::HARD_TAG_TYPE_OF, object));
			tagd::tag_set found;
			TS_ASSERT_TAGD_OK(db.query(found, query, nullptr));
			TS_ASSERT_EQUALS(std::any_of(found.begin(), found.end(), [](const auto& tag) {
				return tag.id() == "leaf";
			}), matches);
		}
		bool matches = true;
		TS_ASSERT_TAGD_OK(db.inherits(matches, "missing", "missing"));
		TS_ASSERT(!matches);
		TS_ASSERT_TAGD_OK(db.inherits(matches, "_entity", "_entity"));
		TS_ASSERT(matches);
		tagd::abstract_tag change("parent");
		TS_ASSERT_TAGD_OK(change.type_relation(tagd::HARD_TAG_TYPE_OF, "other"));
		TS_ASSERT_TAGD_OK(db.put(change, nullptr));
		TS_ASSERT_TAGD_OK(db.inherits(matches, "leaf", "other"));
		TS_ASSERT(matches);
		TS_ASSERT_TAGD_OK(db.del(change, nullptr));
		TS_ASSERT_TAGD_OK(db.inherits(matches, "leaf", "other"));
		TS_ASSERT(!matches);
	}

	void test_check_put_does_not_commit_referents() {
		database db;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("target", HARD_TAG_SUB, HARD_TAG_ENTITY,
			tagd::POS_TAG), nullptr));
		tagd::referent alias("alias", "target", "target");
		std::ostringstream before, after;
		TS_ASSERT_TAGD_OK(db.dump(before));
		TS_ASSERT_EQUALS(db.check_put(alias, 0), tagd::TS_MISUSE);
		TS_ASSERT_TAGD_OK(db.dump(after));
		TS_ASSERT_EQUALS(after.str(), before.str());
	}

	void test_durable_store_preflight() {
		char name[] = "/tmp/tagd-batch-XXXXXX";
		const auto root = std::filesystem::path(mkdtemp(name));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		database db;
		TS_ASSERT_TAGD_OK(db.init((root / "test.db").string()));
		TS_ASSERT_TAGD_OK(db.exec("PRAGMA synchronous=OFF"));
		TS_ASSERT_TAGD_OK(db.prepare_store());
		TS_ASSERT_EQUALS(db.synchronous(), 3);
		TS_ASSERT_TAGD_OK(db.exec("PRAGMA journal_mode=MEMORY"));
		TS_ASSERT_EQUALS(db.prepare_store(), tagd::TS_MISUSE);
	}

	void test_order_rollback_and_duplicate() {
		database db;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		TS_ASSERT(db.supports_atomic_put());
		tagd::abstract_tag parent("parent", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		tagd::abstract_tag child("child", HARD_TAG_SUB, "parent", tagd::POS_TAG);
		tagd::abstract_tag invalid("invalid", HARD_TAG_SUB, "missing", tagd::POS_TAG);
		std::array<const tagd::abstract_tag*, 3> failed{&parent, &child, &invalid};
		TS_ASSERT_EQUALS(db.put_batch(failed, nullptr), tagd::TS_SUB_UNK);
		db.clear_errors();
		TS_ASSERT(!db.exists("parent"));
		TS_ASSERT(!db.exists("child"));
		std::array<const tagd::abstract_tag*, 2> success{&parent, &child};
		TS_ASSERT_TAGD_OK(db.put_batch(success, nullptr));
		TS_ASSERT_EQUALS(db.put_batch(success, nullptr), tagd::TS_DUPLICATE);
		db.clear_errors();
		TS_ASSERT_TAGD_OK(db.put_batch(success, nullptr, tagd::F_IGNORE_DUPLICATES));
		tagd::url u("file:///tmp/batch");
		TS_ASSERT_EQUALS(u.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_FILE_STORED), tagd::TAGD_OK);
		std::array<const tagd::abstract_tag*, 1> urls{&u};
		TS_ASSERT_TAGD_OK(db.put_batch(urls, nullptr));
		tagd::url got;
		TS_ASSERT_TAGD_OK(db.get(got, u.id(), nullptr));
		TS_ASSERT_EQUALS(got.super_object(), tagd::HARD_TAG_FILE);
		TS_ASSERT(got.type_relations.contains({tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_FILE_STORED}));
	}

	void test_indeterminate_finish_blocks_writes() {
		database db;
		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		db.deny_finish();
		tagd::abstract_tag tag("uncertain", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		std::array<const tagd::abstract_tag*, 1> batch{&tag};
		TS_ASSERT_EQUALS(db.put_batch(batch, nullptr), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_DIFFERS(db.last_error().id().size(), 0u);
		db.clear_errors();
		TS_ASSERT_EQUALS(db.put(tag, nullptr), tagd::TS_INTERNAL_ERR);
	}
};
