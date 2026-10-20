#pragma once
#include "tagsh.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

class ConfigStartupTester : public CxxTest::TestSuite {
public:
	void test_fixture_profile_selection_and_cli_precedence() {
		for (const auto subject : {"development", "production"}) {
			const char* argv[] = {"tagsh", "--config-id", subject, "--config", "config.tagl",
				"--log-level", "debug"};
			cmd_args args;
			args.parse(7, const_cast<char**>(argv));
			TS_ASSERT(!args.has_errors());
			tagd::config cfg;
			TS_ASSERT_EQUALS(load_config(cfg, args), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(cfg.subject_id(), subject);
			TS_ASSERT_EQUALS(cfg.bind_port(), std::string_view(subject) == "development" ? 9090 : 7070);
			TS_ASSERT_EQUALS(cfg.log_level(), "debug");
			TS_ASSERT_EQUALS(cfg.prompt(), std::string_view(subject) == "development"
				? "development> " : "tagd> ");
			tagd::abstract_tag defaults;
			TS_ASSERT_EQUALS(cfg.get(defaults, tagd::config::CONFIG_SUBJECT), tagd::TAGD_OK);
			TS_ASSERT(defaults.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_CONFIG_LOG_LEVEL, "warning"));
		}
	}

	void test_alternate_file_cli_precedence_and_live_actions() {
		char pattern[] = "/tmp/tagd-config-test-XXXXXX";
		const std::filesystem::path home = mkdtemp(pattern);
		std::filesystem::create_directories(home / "_config");
		std::ofstream(home / "_config/_config.tagl") << "this is invalid TAGL;";
		const auto alternate = (home / "alternate.tagl").string();
		const auto home_string = home.string();
		std::ofstream(alternate) << ">> default_config _has _config:bind_port = 8080;";
		const char* shared[] = {"tagsh", "--home", home_string.c_str(), "--config",
			alternate.c_str(), "--log-level", "debug"};
		cmd_args args;
		args.parse(7, const_cast<char**>(shared));
		TS_ASSERT(!args.has_errors());
		tagd::config cfg;
		TS_ASSERT_EQUALS(load_config(cfg, args), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(cfg.log_level(), "debug");
		tagsh shell(&cfg, cfg);
		std::vector<startup_action> actions{{startup_action::kind::tagl,
			">> default_config _has _config:prompt = \"live> \";"}};
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0);
		TS_ASSERT_EQUALS(shell.prompt(), "live> ");
		TS_ASSERT(!shell.batch());
		std::ifstream source(alternate);
		std::string contents;
		std::getline(source, contents);
		TS_ASSERT_EQUALS(contents, ">> default_config _has _config:bind_port = 8080;");
		std::filesystem::remove_all(home);
	}

	void test_boolean_setting_uses_hard_boolean_values() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		tagsh shell(&cfg, cfg);
		std::vector<startup_action> actions{{startup_action::kind::tagl,
			">> default_config _has _config:echo_result_code = _false;"}};
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0);
		TS_ASSERT(!cfg.echo_result_code());
	}
};
