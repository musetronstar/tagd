// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>

#include "tagsh.h"

class Tester : public CxxTest::TestSuite {
	public:

	void test_notice_file_logging_in_process(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)

		tagsh shell(&tdb);
		cmd_args args;
		args.opt_noshell = true;
		args.opt_logger.level(tagd::log_level::EMERGENCY);
		args.opt_logger.level(HARD_TAG_ROLE_TAGSH, tagd::log_level::NOTICE);
		args.opt_logger.stream(log_ss);
		args.tagl_statements.push_back("f:../bootstrap.tagl");

		TS_ASSERT_EQUALS(args.interpret(shell), 0)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagsh load file=../bootstrap.tagl"), std::string::npos)
		TS_ASSERT_EQUALS(log_ss.str().find("-- tagsh code=TAGD_OK"), std::string::npos)
	}

	void test_error_command_logging_in_process(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)

		tagsh shell(&tdb);
		cmd_args args;
		args.opt_noshell = true;
		args.opt_logger.level(tagd::log_level::EMERGENCY);
		args.opt_logger.level(HARD_TAG_ROLE_TAGSH, tagd::log_level::ERROR);
		args.opt_logger.stream(log_ss);
		args.tagl_statements.push_back("t:.bogus");

		TS_ASSERT_EQUALS(args.interpret(shell), 0)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- no such command: .bogus"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagsh code=TAGD_ERR"), std::string::npos)
	}

	void test_invalid_log_level_parse_error(void) {
		cmd_args args;
		const char *argv[] = {"tagsh", "--log-level", "chatty"};

		args.parse(3, const_cast<char **>(argv));

		TS_ASSERT(args.has_errors())
		TS_ASSERT_EQUALS(args.code(), tagd::TAGD_ERR)
		TS_ASSERT_DIFFERS(args.last_error().message().find("invalid log level: chatty"), std::string::npos)
	}

	void test_valid_log_level_parse_configures_tagdb_debug_logging(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)
		tagdb::hard_tag::install_logger_validator();

		tagsh shell(&tdb);
		cmd_args args;
		const char *argv[] = {
			"tagsh",
			"--log-level", "emergency,_role:tagdb:debug",
			"-t", "<< dog;",
			"-n"
		};

		args.parse(6, const_cast<char **>(argv));
		args.opt_logger.stream(log_ss);

		TS_ASSERT(!args.has_errors())
		TS_ASSERT_EQUALS(args.code(), tagd::TAGD_OK)
		TS_ASSERT_EQUALS(args.interpret(shell), tagd::TS_NOT_FOUND)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb get subject=dog"), std::string::npos)
	}

	void test_driver_debug_logging_in_process(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)

		tagsh shell(&tdb);
		cmd_args args;
		args.opt_noshell = true;
		args.opt_logger.level(tagd::log_level::EMERGENCY);
		args.opt_logger.level(HARD_TAG_ROLE_DRIVER, tagd::log_level::DEBUG);
		args.opt_logger.stream(log_ss);
		args.tagl_statements.push_back("t:<< dog;");

		TS_ASSERT_EQUALS(args.interpret(shell), tagd::TS_NOT_FOUND)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- driver statement command=CMD_GET subject=dog"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- driver callback=cmd_get subject=dog"), std::string::npos)
	}

	void test_tagdb_notice_put_logging_in_process(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)

		tagsh shell(&tdb);
		cmd_args args;
		args.opt_noshell = true;
		args.opt_logger.level(tagd::log_level::EMERGENCY);
		args.opt_logger.level(HARD_TAG_ROLE_TAGDB, tagd::log_level::NOTICE);
		args.opt_logger.stream(log_ss);
		args.tagl_statements.push_back("f:../bootstrap.tagl");
		args.tagl_statements.push_back("t:>> otter kind_of mammal;");

		TS_ASSERT_EQUALS(args.interpret(shell), 0)
		TS_ASSERT_DIFFERS(log_ss.str().find(">> otter kind_of mammal"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb code=TAGD_OK"), std::string::npos)
	}

	void test_tagdb_error_put_logging_in_process(void) {
		std::stringstream log_ss;
		tagdb_type tdb;

		TS_ASSERT_EQUALS(tdb.init(":memory:"), tagd::TAGD_OK)

		tagsh shell(&tdb);
		cmd_args args;
		args.opt_noshell = true;
		args.opt_logger.level(tagd::log_level::EMERGENCY);
		args.opt_logger.level(HARD_TAG_ROLE_TAGDB, tagd::log_level::ERROR);
		args.opt_logger.stream(log_ss);
		args.tagl_statements.push_back("f:../bootstrap.tagl");
		args.tagl_statements.push_back("t:>> dog kind_of mammal;");

		TS_ASSERT_EQUALS(args.interpret(shell), tagd::TS_DUPLICATE)
		TS_ASSERT_DIFFERS(log_ss.str().find(">> dog kind_of mammal"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb code=TS_DUPLICATE"), std::string::npos)
	}
};
