#pragma once
#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include "tagd/url.h"

class FilepileVocabularyTester : public CxxTest::TestSuite {
public:
	void test_stored_url_and_long_tag() {
		tagd::tagspace::memory space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		tagd::url stored("file:///tmp/content.bin", tagd::HARD_TAG_FILE_STORED);
		TS_ASSERT_EQUALS(space.put(stored), tagd::TAGD_OK);
		tagd::url fetched;
		TS_ASSERT_EQUALS(space.get(fetched, stored.id()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fetched.super_object(), tagd::HARD_TAG_FILE_STORED);
		TS_ASSERT_EQUALS(fetched.id(), stored.id());
		TS_ASSERT_EQUALS(space.put(tagd::abstract_tag(std::string(8192, 'a'),
			tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(tagd::abstract_tag(std::string(8193, 'b'),
			tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TS_ERR_MAX_TAG_LEN);
		TS_ASSERT_EQUALS(tagd::url("file:///" + std::string(2112, 'a')).code(), tagd::URL_MAX_LEN);
	}
};
