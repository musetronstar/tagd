#pragma once

#include "tagspace.h"
#include "tagd/hard-tags.h"

#include <initializer_list>

#include <cxxtest/TestSuite.h>

class TagspaceMemoryTester : public CxxTest::TestSuite {
public:
	static bool rank_equals_bytes(
		const tagd::rank& rank,
		std::initializer_list<unsigned char> bytes
	) {
		if (rank.size() != bytes.size())
			return false;

		const char* data = rank.c_str();
		size_t index = 0;
		for (unsigned char byte : bytes) {
			if (static_cast<unsigned char>(data[index]) != byte)
				return false;
			++index;
		}

		return true;
	}

	void test_memory_tagspace_round_trips_explicit_identity() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag type_of(
			"type_of",
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_SUB,
			tagd::POS_SUB_RELATOR
		);
		tagd::abstract_tag explicit_tag(
			"signal",
			"type_of",
			tagd::HARD_TAG_INTERROGATOR,
			tagd::POS_INTERROGATOR
		);
		tagd::abstract_tag loaded;

		TS_ASSERT_EQUALS(ts.put(type_of), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(explicit_tag), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(loaded, "signal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.id(), explicit_tag.id());
		TS_ASSERT_EQUALS(loaded.sub_relator(), explicit_tag.sub_relator());
		TS_ASSERT_EQUALS(loaded.super_object(), explicit_tag.super_object());
		TS_ASSERT_EQUALS(loaded.pos(), explicit_tag.pos());
		TS_ASSERT(!loaded.rank().empty());
	}

	void test_memory_tagspace_allows_children_of_root_entity() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag is_a(
			"is_a",
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_SUB,
			tagd::POS_SUB_RELATOR
		);
		tagd::abstract_tag
			animal("animal", "is_a", tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		tagd::abstract_tag loaded;

		TS_ASSERT_EQUALS(ts.put(is_a), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(loaded, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.super_object(), tagd::HARD_TAG_ENTITY);
		// User tags must not reuse ranks already assigned to hard tags.
		TS_ASSERT(!loaded.rank().empty());
		TS_ASSERT_DIFFERS(loaded.rank(), ts.lookup_rank(tagd::HARD_TAG_SUB));
		TS_ASSERT(ts.contains(tagd::HARD_TAG_ENTITY, "animal"));
	}

	void test_memory_tagspace_rejects_hard_tag_mutation() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag hard_tag(
			tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_RELATOR,
			tagd::POS_RELATOR
		);

		TS_ASSERT_EQUALS(ts.put(hard_tag), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.del(hard_tag), tagd::TS_MISUSE);
	}
};
