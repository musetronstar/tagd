#pragma once

#include "Tester.h"

class TagdUrlTester : public CxxTest::TestSuite {
	public:

	void test_type_query_mapping() {
		for (const auto* subject : {"mammal,quadruped", "mammal,quadruped,carnivore",
			",quadruped", ",quadruped,carnivore"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			const std::string path = std::string("/_/") + subject + "/legs?q=can+bark";
			TS_ASSERT_EQUALS(input.execute(path + ";"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.cmd(), TOK_CMD_QUERY);
			TS_ASSERT_EQUALS(input.tag().id(), HARD_TAG_INTERROGATOR);
			TS_ASSERT_EQUALS(input.tag().super_object(), subject[0] == ',' ? "" : "mammal");
			TS_ASSERT(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "quadruped"}));
			TS_ASSERT_EQUALS(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "carnivore"}),
				std::string(subject).find("carnivore") != std::string::npos);
			TS_ASSERT(input.tag().related("legs"));
			TS_ASSERT(input.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark"));
		}
	}

	void test_explicit_get_with_query_or_type_list() {
		for (const auto* command : {"<<", "??"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string(command) + " /_/animal,legs/;"),
				tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.cmd(), TOK_CMD_QUERY);
			TS_ASSERT(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "legs"}));
		}
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("<< /_/animal,legs;"), tagd::TS_MISUSE);
	}

	void test_type_paths_validate_syntax_before_command() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE}) {
			for (const auto* path : {"/mammal,/", "/,/", "/mammal,,quadruped/",
				"/,quadruped=x/", "/mammal,", "/*,quadruped/"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
			}
			for (const auto* path : {"/dog,quadruped", "/,quadruped"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TS_MISUSE);
			}
		}
	}

	void test_write_query_paths_are_rejected_before_callbacks() {
		class capture : public TAGL::callback {
		public:
			unsigned mutations = 0;
			void cmd_get(const tagd::abstract_tag&) override {
			}
			void cmd_query(const tagd::interrogator&) override {
			}
			void cmd_put(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_del(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_store(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_delete_storage(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_error() override {
			}
		};
		for (const auto* command : {">>", "!!", ">>+", "!!+"}) {
			for (const auto* path : {"/_/animal/", "/_/mammal,quadruped/", "/_/dog,quadruped"}) {
				tagspace_tester tsp;
				capture cb;
				TAGL::driver input(&tsp, &cb);
				TS_ASSERT_EQUALS(input.execute(std::string(command) + " " + path + ";"),
					tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(cb.mutations, 0u);
			}
		}
	}

	void test_context_lists_and_missing_contexts_for_every_command() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(command, "/dog?c=simple_english,japanese",
				TAGL::tagdurl_mode::constrain_body),
				tagd::TAGD_OK);
			TS_ASSERT(input.session_ptr());
			if (input.session_ptr()) {
				const tagd::id_vec expected{"simple_english", "japanese"};
				TS_ASSERT_EQUALS(input.session_ptr()->context(), expected);
			}
			input.finish();
			input.clear_context_levels();
			TS_ASSERT_EQUALS(input.scan_tagdurl(command, "/dog?c=simple_english,absent",
				TAGL::tagdurl_mode::constrain_body),
				tagd::TS_NOT_FOUND);
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_existing_option_syntax_and_put_slash_restriction() {
		for (const auto* path : {"/dog?unknown=value", "/dog?c=japanese&&q=text"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_GET, path,
				TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
		}
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_PUT, "/https://example.com/file",
			TAGL::tagdurl_mode::constrain_body),
			tagd::TAGL_ERR);
	}

	void test_empty_context_elements_are_syntax_errors_before_command_checks() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE,
			TOK_CMD_STORE, TOK_CMD_DELETE_STORAGE}) {
			for (const auto* path : {"/dog?c=,japanese", "/dog?c=japanese,,simple_english",
				"/dog?c=japanese,", "/animal/?c=japanese%2C%2Csimple_english"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
				if (input.session_ptr())
					TS_ASSERT(input.session_ptr()->context().empty());
			}
		}
	}

	void test_option_values_decode_once() {
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("/_/animal/?q=c%2B%2B+language;"), tagd::TAGD_OK);
		TS_ASSERT(input.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "c++ language"));
	}

	void test_empty_context_option_and_bare_options_remain_accepted() {
		for (const auto* path : {"/dog?c=", "/dog?bare&c=&", "/dog?c=japanese&c="}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_GET, path,
				TAGL::tagdurl_mode::constrain_body), tagd::TAGD_OK);
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_unknown_context_diagnostic_for_session_and_tagdurl() {
		tagspace_tester tsp;
		auto session = tsp.get_session();
		TS_ASSERT_EQUALS(session.push_context("absent"), tagd::TS_NOT_FOUND);
		std::ostringstream session_errors;
		session.print_errors(session_errors);
		TS_ASSERT_DIFFERS(session_errors.str().find("unknown context: absent"), std::string::npos);

		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("<< /_/dog?c=absent;"), tagd::TS_NOT_FOUND);
		std::ostringstream errors;
		input.print_errors(errors);
		TS_ASSERT_DIFFERS(errors.str().find("unknown context: absent"), std::string::npos);
	}

	void test_embedded_put_and_store_resolve_hduri_subject_with_context() {
		for (const auto* command : {">>", ">>+"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string(command)
				+ " /_/hd:org!wikipedia!en!/wiki/Dog!!!!!!https?c=japanese;"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.tag().id(), "https://en.wikipedia.org/wiki/Dog");
			TS_ASSERT_EQUALS(input.cmd(), std::string(command) == ">>" ? TOK_CMD_PUT : TOK_CMD_STORE);
			TS_ASSERT(input.constrain_tag_id.empty());
			TS_ASSERT(input.session_ptr());
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_put_hduri_with_context() {
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		const std::string id = "hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_PUT, "/" + id + "?c=japanese",
			TAGL::tagdurl_mode::constrain_body),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.constrain_tag_id, id);
	}

	void test_cmd_get_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog", TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_get_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_del_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DELETE, "/dog",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DELETE)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_del_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_DELETE, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DELETE)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_get_hduri_tag(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		const char *hduri = "/hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, hduri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), "https://en.wikipedia.org/wiki/Dog")
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_URL)
	}

	void test_cmd_get_event_error_uri_tag(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		const char *evuri = "/ev:2026-04-09T04:00:56.738Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!1!_event";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, evuri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), evuri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_EVENT)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_TAG)

		const char *erruri = "/err:2026-04-09T04:00:56.739Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!2!_error:ts_not_found";
		tc = tagl.scan_tagdurl(TOK_CMD_GET, erruri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), erruri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_ERROR_TS_NOT_FOUND)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_ERROR)
	}

	// mirrors Tester.h `test_subject(void)`, but uses a tagdurl subject
	void test_get_statement_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);
		tagd::code tc = tagl.execute("<< /_/dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_del_statement_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("!! /_/dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_DELETE )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/mammal/;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children_placeholder(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/*/legs,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT( tagl.tag().super_object().empty() )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/meow,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related("meow") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations_with_modifier(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/blood=warm,tail,bark;");
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
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/mammal/?q=can+bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_sub_search_terms(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/?q=warm+blood;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "warm blood") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_search_terms_without_trailing_slash(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/animal?q=warm+blood",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_root_search_terms(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "?q=can+bark",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_SEARCH )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_get_context_option(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog?c=simple_english",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "simple_english" )

		tagl.clear_context_levels();
	}

	void test_get_utf8_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ", TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
	}

	void test_get_context_option_japanese(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ?c=japanese",
			TAGL::tagdurl_mode::constrain_body);
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
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/doggy?c=simple_english",
			TAGL::tagdurl_mode::constrain_body);
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
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_slash_empty_tagdurl(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_trailing_slash_tagdurl(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog/", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_put_illegal_search(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/pigeon?q=oops",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_cmd_put_constrain_tag_id(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_http_put_constrain_tag_id(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_PUT, "/dog");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_del_illegal_search(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DELETE, "/bat?q=oops",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

};
