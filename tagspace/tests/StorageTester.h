#pragma once

#include "tagspace.h"
#include "tagd/hard-tagspace.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <csignal>

class StorageTester : public CxxTest::TestSuite {
	std::filesystem::path root;

public:
	void test_tagspace_creates_stack_and_heap_sessions() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(
			ts.put(tagd::abstract_tag(
				"zoo",
				tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY,
				tagd::POS_TAG
			)),
			tagd::TAGD_OK
		);
		auto stack = ts.get_session();
		auto* heap = ts.new_session();
		TS_ASSERT_DIFFERS(stack.id(), heap->id());
		TS_ASSERT_EQUALS(stack.push_context("zoo"), tagd::TAGD_OK);
		TS_ASSERT(heap->context().empty());
		TS_ASSERT_EQUALS(heap->push_context("zoo"), tagd::TAGD_OK);
		ts.release_session(heap);
	}

	void setUp() {
		char pattern[] = "/tmp/tagspace-contract-XXXXXX";
		root = mkdtemp(pattern);
	}
	void tearDown() {
		std::filesystem::remove_all(root);
	}

	void test_create_open_persist_and_duplicate() {
		/*
		 * Use a UTF-8 name with a space to check that filenames preserve it.
		 * Destroy the first tagspace before reopening to check that data was saved.
		 */
		const auto home = root / "new-home";
		{
			tagd::tagspace::persistent ts;
			TS_ASSERT_EQUALS(
				ts.init("動物 Zoo", home.string()),
				tagd::TS_NOT_FOUND
			);
			TS_ASSERT(!std::filesystem::exists(home));
		}
		{
			tagd::tagspace::persistent ts;
			TS_ASSERT_EQUALS(
				ts.create("動物 Zoo", home.string()),
				tagd::TAGD_OK
			);
			TS_ASSERT_EQUALS(
				ts.put(tagd::abstract_tag(
					"donkey",
					tagd::HARD_TAG_SUB,
					tagd::HARD_TAG_ENTITY,
					tagd::POS_TAG
				)),
				tagd::TAGD_OK
			);
		}
		TS_ASSERT(
			std::filesystem::exists(home / "tagspaces/動物 Zoo/動物 Zoo.db")
		);
		tagd::tagspace::persistent reopened;
		TS_ASSERT_EQUALS(
			reopened.init("動物 Zoo", home.string()),
			tagd::TAGD_OK
		);
		TS_ASSERT(reopened.exists("donkey"));
		tagd::tagspace::persistent duplicate;
		TS_ASSERT_EQUALS(
			duplicate.create("動物 Zoo", home.string()),
			tagd::TS_DUPLICATE
		);
		TS_ASSERT(duplicate.has_errors());
		TS_ASSERT(reopened.exists("donkey"));
	}

	void test_recreate_missing_database_preserves_directory_contents() {
		const auto dir = root / "tagspaces/zoo";
		std::filesystem::create_directories(dir);
		std::ofstream(dir / "keep.txt") << "keep";
		{
			tagd::tagspace::persistent ts;
			TS_ASSERT_EQUALS(ts.create("zoo", root.string()), tagd::TAGD_OK);
		}
		std::filesystem::remove(dir / "zoo.db");
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("zoo", root.string()), tagd::TAGD_OK);
		TS_ASSERT(std::filesystem::exists(dir / "keep.txt"));
	}

	void test_invalid_names_have_no_filesystem_effects() {
		/*
		 * Include malformed UTF-8 (overlong slash, surrogate) and Unicode edge
		 * whitespace: byte-safe paths alone do not establish valid Unicode names.
		 */
		std::vector<std::string> names{
			"",
			".",
			"..",
			"../escape",
			"a/b",
			"a\\b",
			" leading",
			"trailing ",
			"a\nb",
			std::string("a\0b", 3),
			std::string("\xc0\xaf", 2),
			std::string("\xed\xa0\x80", 3),
			"\xc2\xa0name",
			"name\xe3\x80\x80",
			std::string(256, 'a')
		};
		for (const auto& name : names) {
			tagd::tagspace::persistent ts;
			TS_ASSERT_EQUALS(
				ts.create(name, (root / "absent").string()),
				tagd::TS_ERR
			);
			TS_ASSERT(ts.has_errors());
			TS_ASSERT(!std::filesystem::exists(root / "absent"));
		}
	}

	void test_existing_invalid_database_is_not_overwritten() {
		auto dir = root / "tagspaces/broken";
		std::filesystem::create_directories(dir);
		std::ofstream(dir / "broken.db") << "not a database";
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(
			ts.create("broken", root.string()),
			tagd::TS_DUPLICATE
		);
		tagd::tagspace::persistent opened;
		TS_ASSERT_DIFFERS(opened.init("broken", root.string()), tagd::TAGD_OK);
		std::ifstream input(dir / "broken.db");
		std::string contents;
		std::getline(input, contents);
		TS_ASSERT_EQUALS(contents, "not a database");
	}

	void test_failed_creation_does_not_remove_existing_contents() {
		auto dir = root / "tagspaces/broken";
		std::filesystem::create_directories(dir / "broken.db-journal");
		std::ofstream(dir / "keep.txt") << "keep";
		tagd::tagspace::persistent ts;
		TS_ASSERT_DIFFERS(ts.create("broken", root.string()), tagd::TAGD_OK);
		TS_ASSERT(!std::filesystem::exists(dir / "broken.db"));
		TS_ASSERT(std::filesystem::is_directory(dir / "broken.db-journal"));
		TS_ASSERT(std::filesystem::exists(dir / "keep.txt"));
	}

	void test_write_failure_rolls_back_new_storage() {
		auto home = root / "failed-home";
		const pid_t pid = fork();
		TS_ASSERT_DIFFERS(pid, -1);
		if (pid == 0) {
			/*
			 * Force a real bootstrap write failure after exclusive reservation.
			 * The limit is child-local so the test runner's output is unaffected.
			 */
			std::signal(SIGXFSZ, SIG_IGN);
			struct rlimit limit {
				0, 0
			};
			if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
				_exit(2);
			tagd::tagspace::persistent ts;
			auto rc = ts.create("broken", home.string());
			_exit(rc == tagd::TAGD_OK ? 1 : 0);
		}
		if (pid < 0)
			return;
		int status = 0;
		TS_ASSERT_EQUALS(waitpid(pid, &status, 0), pid);
		TS_ASSERT(WIFEXITED(status));
		TS_ASSERT_EQUALS(WEXITSTATUS(status), 0);
		TS_ASSERT(!std::filesystem::exists(home));
	}

	void test_memory_is_isolated_and_obeys_rank_topology() {
		tagd::tagspace::memory a, b;
		TS_ASSERT_EQUALS(a.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(b.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(
			a.put(tagd::abstract_tag(
				"animal",
				tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY,
				tagd::POS_TAG
			)),
			tagd::TAGD_OK
		);
		TS_ASSERT_EQUALS(
			a.put(tagd::abstract_tag(
				"donkey",
				tagd::HARD_TAG_SUB,
				"animal",
				tagd::POS_TAG
			)),
			tagd::TAGD_OK
		);
		TS_ASSERT(!b.exists("animal"));
		TS_ASSERT(a.contains(tagd::HARD_TAG_ENTITY, "donkey"));
		TS_ASSERT(a.contains("animal", "donkey"));
		TS_ASSERT(!a.contains("donkey", "animal"));
		TS_ASSERT(!a.contains("absent", "donkey"));
		TS_ASSERT(!a.contains("absent", "absent"));
		tagd::hard_tagspace hard;
		TS_ASSERT_EQUALS(
			a.lookup_rank(tagd::HARD_TAG_HAS),
			hard.lookup_rank(tagd::HARD_TAG_HAS)
		);
		TS_ASSERT_DIFFERS(
			a.lookup_rank("animal"),
			a.lookup_rank(tagd::HARD_TAG_SUB)
		);
	}

	void test_sessions_errors_and_full_text_search_cross_public_interface() {
		tagd::tagspace::memory storage;
		TS_ASSERT_EQUALS(storage.init(), tagd::TAGD_OK);
		tagd::tagspace& ts = storage;
		TS_ASSERT_EQUALS(
			ts.put(tagd::abstract_tag(
				"zebra",
				tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY,
				tagd::POS_TAG
			)),
			tagd::TAGD_OK
		);
		auto ssn = ts.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("zebra"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ssn.context().back(), "zebra");
		TS_ASSERT_EQUALS(ssn.clear_context(), tagd::TAGD_OK);
		tagd::tag_set matches;
		tagd::interrogator q;
		TS_ASSERT_EQUALS(
			q.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_TERMS, "zebra"),
			tagd::TAGD_OK
		);
		TS_ASSERT_EQUALS(ts.query(matches, q, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(matches.size(), 1U);
		tagd::errorable observer;
		/*
		 * Errors must reach the newly shared list even after initialization.
		 * Suppressing an error object must still return TS_NOT_FOUND.
		 */
		observer.share_errors(ts);
		ts.share_errors(ssn);
		tagd::abstract_tag missing;
		TS_ASSERT_EQUALS(ts.get(missing, "missing", &ssn), tagd::TS_NOT_FOUND);
		TS_ASSERT(observer.has_errors());
		ts.clear_errors();
		TS_ASSERT_EQUALS(
			ts.get(missing, "missing", &ssn, tagd::F_NO_NOT_FOUND_ERROR),
			tagd::TS_NOT_FOUND
		);
		TS_ASSERT_EQUALS(observer.size(), 0U);
	}
};
