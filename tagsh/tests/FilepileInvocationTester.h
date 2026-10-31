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
	void test_store_path_uses_collection_file_url_spelling() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(home.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> persistent;
		TS_ASSERT_EQUALS(cfg.create_tagspace("test", persistent), tagd::TAGD_OK);
		const auto path = home / "a-b.txt";
		std::ofstream(path) << "shared file identity\n";
		tagsh shell(persistent.get(), cfg);
		TS_ASSERT_EQUALS(shell.interpret_store(path.string(), ""), 0);
		const tagd::url expected("file://" + path.string());
		TS_ASSERT(persistent->exists(expected.hduri()));
		TS_ASSERT(persistent->exists("link:" + expected.hduri() + "#001"));
	}

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
			{startup_action::kind::tagl, ">>+ " + source + ";"},
			{startup_action::kind::tagl, ">> " + source + " _has _message = explicit;"},
			{startup_action::kind::tagl, "%% _ignore_duplicates _true;"},
			{startup_action::kind::tagl, ">>+ " + source + ";"}
		};
		TS_ASSERT_EQUALS(execute_startup_actions(shell, actions), 0);

		const auto link = "link:" + tagd::url(source).hduri() + "#001\n";
		TS_ASSERT_EQUALS(out.str(), link + link);

		tagd::url a;
		TS_ASSERT_EQUALS(persistent->get(a, source), tagd::TAGD_OK);
		TS_ASSERT(a.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "explicit"));

		TS_ASSERT_EQUALS(execute_startup_actions(shell, {
			{startup_action::kind::tagl, "%% _ignore_duplicates _false;"},
			{startup_action::kind::tagl, ">>+ " + source + ";"},
			{startup_action::kind::tagl, ">> must_not_exist _sub _entity;"}
		}), tagd::TS_DUPLICATE);
		TS_ASSERT(!persistent->exists("must_not_exist"));
	}
	void test_removed_store_and_tagl_file_invocation() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(home.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> persistent;
		TS_ASSERT_EQUALS(cfg.create_tagspace("test", persistent), tagd::TAGD_OK);
		tagsh shell(persistent.get(), cfg);
		shell.batch(true);
		TS_ASSERT_DIFFERS(shell.interpret(".store " + source), 0);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));

		std::ofstream(home / "script.tagl") << ">>+ " << source << " _has _message = script;";
		tagsh fresh(persistent.get(), cfg);
		TS_ASSERT_EQUALS(fresh.interpret_fname((home / "script.tagl").string()), 0);
		TS_ASSERT(persistent->exists("link:" + tagd::url(source).hduri() + "#001"));
	}

	void test_store_groups_and_tagl_actions_keep_their_order() {
		const char* argv[] = {"tagsh", "--tagl", "<< _entity;", "--store", source.c_str(),
			"holiday photo.jpg", "--with", "_has _message = caption", "--store", "other.jpg", "--noshell"};
		cmd_args args;
		args.parse(11, const_cast<char**>(argv));
		TS_ASSERT(!args.has_errors());
		TS_ASSERT_EQUALS(args.actions().size(), 4u);
		TS_ASSERT_EQUALS(args.actions().at(0).type, startup_action::kind::tagl);
		for (size_t i : {1u, 2u}) {
			TS_ASSERT_EQUALS(args.actions().at(i).type, startup_action::kind::store);
			TS_ASSERT_EQUALS(args.actions().at(i).predicates, "_has _message = caption");
		}
		TS_ASSERT_EQUALS(args.actions().at(1).input, source);
		TS_ASSERT_EQUALS(args.actions().at(2).input, "holiday photo.jpg");
		TS_ASSERT(args.actions().at(3).predicates.empty());
		TS_ASSERT(args.noshell());
	}

	void test_with_requires_a_value_before_the_next_option() {
		const char* argv[] = {"tagsh", "--store", source.c_str(), "--with", "--noshell"};
		cmd_args args;
		args.parse(5, const_cast<char**>(argv));
		TS_ASSERT(args.has_errors());
	}
};
