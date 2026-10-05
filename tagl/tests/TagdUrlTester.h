#pragma once

#include "Tester.h"

class TagdUrlTester : public CxxTest::TestSuite {
	public:

	void test_cmd_get_tag(void) {
		tagspace_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_get_tag(void) {
		tagspace_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_del_tag(void) {
		tagspace_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DEL, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DEL)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_del_tag(void) {
		tagspace_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_DELETE, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DEL)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_get_hduri_tag(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		const char *hduri = "/hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, hduri);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), "https://en.wikipedia.org/wiki/Dog")
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_URL)
	}

	void test_cmd_get_event_error_uri_tag(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		const char *evuri = "/ev:2026-04-09T04:00:56.738Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!1!_event";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, evuri);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), evuri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_EVENT)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_TAG)

		const char *erruri = "/err:2026-04-09T04:00:56.739Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!2!_error:ts_not_found";
		tc = tagl.scan_tagdurl(TOK_CMD_GET, erruri);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), erruri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_ERROR_TS_NOT_FOUND)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_ERROR)
	}

	// mirrors Tester.h `test_subject(void)`, but uses a tagdurl subject
	void test_get_statement_subject(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("<< /dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_del_statement_subject(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("!! /dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_DEL )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /mammal/;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children_placeholder(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /*/legs,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT( tagl.tag().super_object().empty() )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /animal/meow,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related("meow") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations_with_modifier(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /animal/blood=warm,tail,bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related("", "blood", "warm") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tagl.tag().related("bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_search_terms(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /mammal/?q=can+bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_sub_search_terms(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute("?? /animal/?q=warm+blood;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "warm blood") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_search_terms_without_trailing_slash(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/animal?q=warm+blood");
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_root_search_terms(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "?q=can+bark");
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_SEARCH )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_get_context_option(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog?c=simple_english");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "simple_english" )

		tagl.clear_context_levels();
	}

	void test_get_utf8_subject(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ");
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
	}

	void test_get_context_option_japanese(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ?c=japanese");
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "japanese" )

		tagl.clear_context_levels();
	}

	void test_get_context_option_simple_english(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/doggy?c=simple_english");
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "doggy" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "simple_english" )

		tagl.clear_context_levels();
	}

	void test_put_empty_tagdurl(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_slash_empty_tagdurl(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_trailing_slash_tagdurl(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog/");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_illegal_search(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/pigeon?q=oops");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_cmd_put_constrain_tag_id(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_http_put_constrain_tag_id(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_PUT, "/dog");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_del_illegal_search(void) {
		tagspace_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DEL, "/bat?q=oops");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

};
