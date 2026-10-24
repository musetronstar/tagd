#pragma once
#include <cxxtest/TestSuite.h>
#include "tagd/datetime.h"

class DatetimeTester : public CxxTest::TestSuite {
public:
	void test_epoch_fraction_and_range() {
		std::string out;
		TS_ASSERT_EQUALS(tagd::format_datetime(out, 0, 0), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(out, "1970-01-01T00:00:00.000000000Z");
		TS_ASSERT_EQUALS(tagd::format_datetime(out, -1, 500000000), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(out, "1969-12-31T23:59:59.500000000Z");
		TS_ASSERT_EQUALS(tagd::format_datetime(out, -62135596800LL, 1), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(out, "0001-01-01T00:00:00.000000001Z");
		TS_ASSERT_EQUALS(tagd::format_datetime(out, 253402300799LL, 999999999), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(out, "9999-12-31T23:59:59.999999999Z");
		TS_ASSERT_EQUALS(tagd::format_datetime(out, 951782400, 0), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(out, "2000-02-29T00:00:00.000000000Z");
		for (auto seconds : {-62135596801LL, 253402300800LL}) {
			TS_ASSERT_EQUALS(tagd::format_datetime(out, seconds, 0), tagd::TS_ERR);
			TS_ASSERT(out.empty());
		}
		for (auto nanos : {-1, 1000000000}) {
			TS_ASSERT_EQUALS(tagd::format_datetime(out, 0, nanos), tagd::TS_ERR);
			TS_ASSERT(out.empty());
		}
	}
};
