#pragma once

#include "Tester.h"

class TagdUrlTester : public CxxTest::TestSuite {
	public:

	void test_cmd_get_tag(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		// TODO implement this
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_get_tag(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	// mirrors Taster.h `test_subject(void)`, but uses a tagdurl subject
	void test_subject(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		// TODO TAGL will recognize CMD + tagdurl where the the preceeding '/' in the tagdurl comes directly after a CMD token 
		tagd::code tc = tagl.execute("<< /dog");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

};
