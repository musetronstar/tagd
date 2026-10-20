// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>

#include "tagspace.h"
#include "tagsh.h"

typedef tagd::tagspace::memory tagspace_type;

class Tester : public CxxTest::TestSuite {
	public:
	void tearDown() {
		uninstall_loggers();
	}

	void test_notice_file_logging_in_process(void) {
		std::stringstream log_ss;
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		std::vector<startup_action> actions;
		tagd::tagspace_install_logger_validator();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "emergency,_role:tagsh:notice"), tagd::TAGD_OK);
		cfg.log().stream(log_ss);
		actions.push_back({startup_action::kind::file, "bootstrap.tagl"});

		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagsh load file=bootstrap.tagl"), std::string::npos)
		TS_ASSERT_EQUALS(log_ss.str().find("-- tagsh code=TAGD_OK"), std::string::npos)
	}

	void test_error_command_logging_in_process(void) {
		std::stringstream log_ss;
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		std::vector<startup_action> actions;
		tagd::tagspace_install_logger_validator();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "emergency,_role:tagsh:error"), tagd::TAGD_OK);
		cfg.log().stream(log_ss);
		actions.push_back({startup_action::kind::tagl, ".bogus"});

		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0)
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
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)
		tagd::tagspace_install_logger_validator();

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		cmd_args args;
		const char *argv[] = {
			"tagsh",
			"--log-level", "emergency,_role:tagdb:debug",
			"-t", "<< dog;",
			"-n"
		};

		args.parse(6, const_cast<char **>(argv));
		for (const auto& [id, value] : args.overrides()) {
			TS_ASSERT_EQUALS(cfg.set(id, value), tagd::TAGD_OK);
		}
		const auto& actions = args.actions();
		cfg.log().stream(log_ss);

		TS_ASSERT(!args.has_errors())
		TS_ASSERT_EQUALS(args.code(), tagd::TAGD_OK)
		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), tagd::TS_NOT_FOUND)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb get subject=dog"), std::string::npos)
	}

	void test_driver_debug_logging_in_process(void) {
		std::stringstream log_ss;
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		std::vector<startup_action> actions;
		tagd::tagspace_install_logger_validator();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "emergency,_role:driver:debug"), tagd::TAGD_OK);
		cfg.log().stream(log_ss);
		actions.push_back({startup_action::kind::tagl, "<< dog;"});

		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), tagd::TS_NOT_FOUND)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- driver statement command=CMD_GET subject=dog"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- driver callback=cmd_get subject=dog"), std::string::npos)
	}

	void test_tagdb_notice_put_logging_in_process(void) {
		std::stringstream log_ss;
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		std::vector<startup_action> actions;
		tagd::tagspace_install_logger_validator();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "emergency,_role:tagdb:notice"), tagd::TAGD_OK);
		cfg.log().stream(log_ss);
		actions.push_back({startup_action::kind::file, "bootstrap.tagl"});
		actions.push_back({startup_action::kind::tagl, ">> otter kind_of mammal;"});

		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0)
		TS_ASSERT_DIFFERS(log_ss.str().find(">> otter kind_of mammal"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb code=TAGD_OK"), std::string::npos)
	}

	void test_tagdb_error_put_logging_in_process(void) {
		std::stringstream log_ss;
		tagspace_type tsp;

		TS_ASSERT_EQUALS(tsp.init(), tagd::TAGD_OK)

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		tagsh shell(&tsp, cfg);
		std::vector<startup_action> actions;
		tagd::tagspace_install_logger_validator();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "emergency,_role:tagdb:error"), tagd::TAGD_OK);
		cfg.log().stream(log_ss);
		actions.push_back({startup_action::kind::file, "bootstrap.tagl"});
		actions.push_back({startup_action::kind::tagl, ">> dog kind_of mammal;"});

		install_loggers(cfg);
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), tagd::TS_DUPLICATE)
		TS_ASSERT_DIFFERS(log_ss.str().find(">> dog kind_of mammal"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- tagdb code=TS_DUPLICATE"), std::string::npos)
	}
};
