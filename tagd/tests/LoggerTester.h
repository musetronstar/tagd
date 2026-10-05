// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>

#include "tagd.h"
#include "tagd/hard-tags.h"
#include "tagd/logger.h"
#include "tagd/event.h"

inline tagd::id_string owned_id(std::string_view sv) {
	return tagd::id_string(sv);
}

static bool test_log_role_validator(const std::string& role) {
	return role == HARD_TAG_ROLE_SYSTEM
		|| role == HARD_TAG_ROLE_SCANNER
		|| role == HARD_TAG_ROLE_PARSER
		|| role == HARD_TAG_ROLE_TAGDB
		|| role == HARD_TAG_ROLE_TAGSPACE
		|| role == HARD_TAG_ROLE_TAGSH
		|| role == HARD_TAG_ROLE_SECURITY;
}

class Tester : public CxxTest::TestSuite {
	public:

	void test_default_level(void) {
		std::stringstream ss;
		tagd::logger log(ss);

		TS_ASSERT_EQUALS(log.level(), tagd::log_level::INFO)
	}

	void test_syslog_threshold(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		log.level(tagd::log_level::WARNING);

		log.log(tagd::log_level::ERROR, "error visible");
		log.log(tagd::log_level::WARNING, "warning visible");
		log.log(tagd::log_level::NOTICE, "notice hidden");
		log.log(tagd::log_level::DEBUG, "debug hidden");

		TS_ASSERT_DIFFERS(ss.str().find("error visible"), std::string::npos)
		TS_ASSERT_DIFFERS(ss.str().find("warning visible"), std::string::npos)
		TS_ASSERT_EQUALS(ss.str().find("notice hidden"), std::string::npos)
		TS_ASSERT_EQUALS(ss.str().find("debug hidden"), std::string::npos)
	}

	void test_debug_level_logs_everything(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		log.level(tagd::log_level::DEBUG);

		log.log(tagd::log_level::EMERGENCY, "emergency visible");
		log.log(tagd::log_level::DEBUG, "debug visible");

		TS_ASSERT_DIFFERS(ss.str().find("emergency visible"), std::string::npos)
		TS_ASSERT_DIFFERS(ss.str().find("debug visible"), std::string::npos)
	}

	void test_log_level_name(void) {
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::EMERGENCY)), "emergency")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::ALERT)), "alert")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::CRITICAL)), "critical")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::ERROR)), "error")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::WARNING)), "warning")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::NOTICE)), "notice")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::INFO)), "info")
		TS_ASSERT_EQUALS(std::string(tagd::log_level_name(tagd::log_level::DEBUG)), "debug")
	}

	void test_parse_log_level(void) {
		tagd::log_level lvl = tagd::log_level::INFO;

		TS_ASSERT(tagd::parse_log_level("debug", lvl))
		TS_ASSERT_EQUALS(lvl, tagd::log_level::DEBUG)

		TS_ASSERT(tagd::parse_log_level("WARNING", lvl))
		TS_ASSERT_EQUALS(lvl, tagd::log_level::WARNING)

		TS_ASSERT(tagd::parse_log_level("Critical", lvl))
		TS_ASSERT_EQUALS(lvl, tagd::log_level::CRITICAL)
	}

	void test_parse_log_level_invalid(void) {
		tagd::log_level lvl = tagd::log_level::NOTICE;

		TS_ASSERT(!tagd::parse_log_level("chatty", lvl))
		TS_ASSERT_EQUALS(lvl, tagd::log_level::NOTICE)
	}

	void test_role_level_override(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		log.level(tagd::log_level::WARNING);
		log.level(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG);

		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_PARSER), tagd::log_level::WARNING)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_SCANNER), tagd::log_level::DEBUG)

		log.log(HARD_TAG_ROLE_PARSER, tagd::log_level::INFO, "parser hidden");
		log.log(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG, "scanner visible");

		TS_ASSERT_EQUALS(ss.str().find("parser hidden"), std::string::npos)
		TS_ASSERT_DIFFERS(ss.str().find("scanner visible"), std::string::npos)
	}

	void test_role_stream_override(void) {
		std::stringstream default_ss;
		std::stringstream scanner_ss;
		tagd::logger log(default_ss);

		log.stream(HARD_TAG_ROLE_SCANNER, scanner_ss);
		log.log(HARD_TAG_ROLE_SCANNER, tagd::log_level::INFO, "scanner visible");
		log.log(HARD_TAG_ROLE_PARSER, tagd::log_level::INFO, "parser visible");

		TS_ASSERT_DIFFERS(scanner_ss.str().find("scanner visible"), std::string::npos)
		TS_ASSERT_EQUALS(scanner_ss.str().find("parser visible"), std::string::npos)
		TS_ASSERT_EQUALS(default_ss.str().find("scanner visible"), std::string::npos)
		TS_ASSERT_DIFFERS(default_ss.str().find("parser visible"), std::string::npos)
	}

	void test_role_stream_override_respects_role_level(void) {
		std::stringstream default_ss;
		std::stringstream scanner_ss;
		tagd::logger log(default_ss);

		log.level(tagd::log_level::WARNING);
		log.level(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG);
		log.stream(HARD_TAG_ROLE_SCANNER, scanner_ss);

		log.log(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG, "scanner debug visible");
		log.log(HARD_TAG_ROLE_PARSER, tagd::log_level::INFO, "parser info hidden");
		log.log(HARD_TAG_ROLE_PARSER, tagd::log_level::ERROR, "parser error visible");

		TS_ASSERT_DIFFERS(scanner_ss.str().find("scanner debug visible"), std::string::npos)
		TS_ASSERT_EQUALS(scanner_ss.str().find("parser"), std::string::npos)
		TS_ASSERT_EQUALS(default_ss.str().find("scanner debug visible"), std::string::npos)
		TS_ASSERT_EQUALS(default_ss.str().find("parser info hidden"), std::string::npos)
		TS_ASSERT_DIFFERS(default_ss.str().find("parser error visible"), std::string::npos)
	}

	void test_parse_log_level_spec(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		tagd::set_log_role_validator(test_log_role_validator);

		TS_ASSERT(tagd::parse_log_level_spec("warning,_role:scanner:debug,_role:parser:info,_role:tagdb:emergency,_role:tagspace:info,_role:tagsh:notice", log))
		TS_ASSERT_EQUALS(log.level(), tagd::log_level::WARNING)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_SCANNER), tagd::log_level::DEBUG)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_PARSER), tagd::log_level::INFO)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_TAGDB), tagd::log_level::EMERGENCY)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_TAGSPACE), tagd::log_level::INFO)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_TAGSH), tagd::log_level::NOTICE)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_HTTAGD), tagd::log_level::WARNING)
	}

	void test_parse_log_level_spec_invalid(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		tagd::set_log_role_validator(test_log_role_validator);
		log.level(tagd::log_level::NOTICE);
		log.level(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG);

		TS_ASSERT(!tagd::parse_log_level_spec("warning,_role:scanner:chatty", log))
		TS_ASSERT_EQUALS(log.level(), tagd::log_level::NOTICE)
		TS_ASSERT_EQUALS(log.level(HARD_TAG_ROLE_SCANNER), tagd::log_level::DEBUG)

		TS_ASSERT(!tagd::parse_log_level_spec("_role:parser:debug:extra", log))
		TS_ASSERT(!tagd::parse_log_level_spec("bad role:debug", log))
		TS_ASSERT(!tagd::parse_log_level_spec("bananas:debug", log))
	}

	void test_valid_log_role(void) {
		tagd::set_log_role_validator(test_log_role_validator);

		TS_ASSERT(tagd::valid_log_role(owned_id(HARD_TAG_ROLE_SYSTEM)))
		TS_ASSERT(tagd::valid_log_role(owned_id(HARD_TAG_ROLE_SCANNER)))
		TS_ASSERT(tagd::valid_log_role(owned_id(HARD_TAG_ROLE_SECURITY)))
		TS_ASSERT(!tagd::valid_log_role("_role:bananas"))
	}

	void test_log_event(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		tagd::session ssn;
		tagd::event ev(ssn, "tagsh", owned_id(HARD_TAG_COMMAND_EVENT));

		log.log(tagd::log_level::NOTICE, ev);
		TS_ASSERT_DIFFERS(ss.str().find(ev.evuri()), std::string::npos)
	}

	void test_log_event_with_role_stream_override(void) {
		std::stringstream default_ss;
		std::stringstream tagdb_ss;
		tagd::logger log(default_ss);
		tagd::session ssn;
		tagd::event ev(ssn, "tagdb", owned_id(HARD_TAG_TAGDB_PUT_EVENT));

		log.stream(HARD_TAG_ROLE_TAGDB, tagdb_ss);
		log.log(HARD_TAG_ROLE_TAGDB, tagd::log_level::NOTICE, ev);

		TS_ASSERT_DIFFERS(tagdb_ss.str().find(ev.evuri()), std::string::npos)
		TS_ASSERT_EQUALS(default_ss.str().find(ev.evuri()), std::string::npos)
	}

	void test_log_event_filtered(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		log.level(tagd::log_level::WARNING);
		tagd::session ssn;
		tagd::event ev(ssn, "tagsh", owned_id(HARD_TAG_COMMAND_EVENT));

		log.log(tagd::log_level::NOTICE, ev);
		TS_ASSERT_EQUALS(ss.str().find(ev.evuri()), std::string::npos)
	}

	void test_log_errorable(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		tagd::errorable e;
		e.ferror(tagd::TS_NOT_FOUND, "no such tag: %s", "foo");

		log.log(tagd::log_level::ERROR, e);
		TS_ASSERT_DIFFERS(ss.str().find(HARD_TAG_ERROR_TS_NOT_FOUND), std::string::npos)
	}

	void test_log_errorable_filtered(void) {
		std::stringstream ss;
		tagd::logger log(ss);
		log.level(tagd::log_level::CRITICAL);
		tagd::errorable e;
		e.ferror(tagd::TS_NOT_FOUND, "no such tag: %s", "foo");

		log.log(tagd::log_level::ERROR, e);
		TS_ASSERT_EQUALS(ss.str().find(HARD_TAG_ERROR_TS_NOT_FOUND), std::string::npos)
	}
};
