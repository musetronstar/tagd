#pragma once
#include "tagspace-config.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>

class CollectionTester : public CxxTest::TestSuite {
	std::filesystem::path root;
public:
	void setUp() {
		char pattern[] = "/tmp/tagspace-collection-XXXXXX";
		root = mkdtemp(pattern);
	}
	void tearDown() {
		std::filesystem::remove_all(root);
	}
	void test_descriptions_sources_and_borrowed_lifetime() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> first, second;
		TS_ASSERT_EQUALS(cfg.open_space("animals", "a", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_space("a", "", second), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.get(), second.get());
		TS_ASSERT_EQUALS(cfg.close_space("animals"), tagd::TS_MISUSE);
		first.reset();
		second.reset();
		TS_ASSERT_EQUALS(cfg.close_space("animals"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.close_space("animals"), tagd::TAGD_OK);
		TS_ASSERT(cfg.exists("animals"));
		TS_ASSERT_EQUALS(cfg.open_space("a", "", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(tagd::abstract_tag("animals")), tagd::TS_MISUSE);
	}
	void test_source_first_reuse_and_collisions() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		tagd::tagspace::persistent db;
		TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		auto path = root / "tagspaces/animals/animals.db";
		tagd::url source("file://" + path.string());
		TS_ASSERT_EQUALS(cfg.put(source), tagd::TAGD_OK);
		tagd::abstract_tag description("zoology", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(description), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_space(path.string(), "z", opened), tagd::TAGD_OK);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_space("z", id), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "zoology");
		tagd::abstract_tag duplicate("other", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(duplicate.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(duplicate), tagd::TS_MISUSE);
		TS_ASSERT(!cfg.exists("other"));
	}
	void test_reserved_targets_and_supporting_vocabulary() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> first, second;
		TS_ASSERT_EQUALS(cfg.open_space("_default", "", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_space("_config", "", second), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(second.get(), &cfg);
		TS_ASSERT_EQUALS(cfg.close_space("_default"), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.open_space("_default", "_bad", second), tagd::TS_MISUSE);
	}
};
