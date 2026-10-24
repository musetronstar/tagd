#pragma once
#include "tagsh.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

class FilepileInvocationTester : public CxxTest::TestSuite {
	std::filesystem::path home;
	std::string source;
public:
	void setUp() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagsh-store-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		if (!directory)
			throw std::runtime_error("cannot create test directory");
		home = directory;
		std::ofstream(home / "source") << "filepile invocation\n";
		source = "file://" + (home / "source").string();
	}
	void tearDown() {
		std::filesystem::remove_all(home);
	}
	void test_ordered_actions_flags_and_explicit_subject() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(home.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> persistent;
		TS_ASSERT_EQUALS(cfg.create_tagspace("test", persistent), tagd::TAGD_OK);
		tagsh shell(persistent.get(), cfg);
		std::ostringstream out;
		shell.set_output(out);

		std::vector<startup_action> actions{
			{startup_action::kind::store, source},
			{startup_action::kind::tagl, ">> " + source + " _has _message = explicit;"},
			{startup_action::kind::tagl, "%% _ignore_duplicates _true;"},
			{startup_action::kind::store, source}
		};
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0);

		const auto link = "link:" + tagd::url(source).hduri() + "#001\n";
		TS_ASSERT_EQUALS(out.str(), link + link);

		tagd::url a;
		TS_ASSERT_EQUALS(persistent->get(a, source), tagd::TAGD_OK);
		TS_ASSERT(a.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "explicit"));

		TS_ASSERT_EQUALS(execute_startup_actions(shell, {
			{startup_action::kind::tagl, "%% _ignore_duplicates _false;"},
			{startup_action::kind::store, source},
			{startup_action::kind::tagl, ">> must_not_exist _sub _entity;"}
		}), tagd::TS_DUPLICATE);
		TS_ASSERT(!persistent->exists("must_not_exist"));
	}
	void test_script_store_is_rejected_and_put_failure_preserves_store() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(home.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> persistent;
		TS_ASSERT_EQUALS(cfg.create_tagspace("test", persistent), tagd::TAGD_OK);
		tagsh shell(persistent.get(), cfg);
		shell.batch(true);

		TS_ASSERT_EQUALS(shell.interpret(".store " + source), tagd::TS_MISUSE);

		std::istringstream stream(".store " + source + "\n");
		TS_ASSERT_EQUALS(shell.interpret(stream), tagd::TS_MISUSE);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));

		std::ofstream(home / "script.tagl") << ".store " << source;
		TS_ASSERT_DIFFERS(shell.interpret_fname((home / "script.tagl").string()), 0);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));

		tagsh fresh(persistent.get(), cfg);
		TS_ASSERT_DIFFERS(execute_startup_actions(fresh, {
			{startup_action::kind::store, source},
			{startup_action::kind::tagl, ">> fail _sub unknown_parent;"}
		}), 0);
		TS_ASSERT(persistent->exists("link:" + tagd::url(source).hduri() + "#001"));
	}
	void test_cli_option_order() {
		const char* argv[] = {"tagsh", "--store", source.c_str(), "--tagl", "<< _entity;",
			"--store", source.c_str(), "--noshell"};
		cmd_args args;
		args.parse(8, const_cast<char**>(argv));
		TS_ASSERT(!args.has_errors());
		TS_ASSERT_EQUALS(args.actions().size(), 3U);
		TS_ASSERT_EQUALS(args.actions().at(0).type, startup_action::kind::store);
		TS_ASSERT_EQUALS(args.actions().at(1).type, startup_action::kind::tagl);
		TS_ASSERT_EQUALS(args.actions().at(2).type, startup_action::kind::store);
		TS_ASSERT(args.noshell());
	}
};
