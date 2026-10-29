#pragma once
#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include "tagd/url.h"

class FilepileVocabularyTester : public CxxTest::TestSuite {
public:
	void test_stored_url_and_long_tag() {
		tagd::tagspace::memory space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		tagd::url stored("file:///tmp/content.bin");
		TS_ASSERT_EQUALS(stored.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_FILE_STORED), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(stored), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(std::string(tagd::code_str(tagd::TS_NOT_AVAILABLE)), "TS_NOT_AVAILABLE");
		TS_ASSERT_EQUALS(std::string(tagd::code_error_tag(tagd::TS_NOT_AVAILABLE)), tagd::HARD_TAG_ERROR_TS_NOT_AVAILABLE);
		TS_ASSERT_EQUALS(std::string(tagd::code_error_tag(tagd::TS_DEPENDENCY)), tagd::HARD_TAG_ERROR_TS_DEPENDENCY);
		TS_ASSERT_EQUALS(space.put(tagd::abstract_tag(std::string(8192, 'a'),
			tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(tagd::abstract_tag(std::string(8193, 'b'),
			tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TS_ERR_MAX_TAG_LEN);
		TS_ASSERT_EQUALS(tagd::url("file:///" + std::string(2112, 'a')).code(), tagd::URL_MAX_LEN);
	}
};
