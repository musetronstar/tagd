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
	void test_ordered_results_across_sessions_targets_and_storage_lifetimes() {
		tagd::abstract_tag retained;
		std::string serialized;
		{
			tagd::tagspace::memory first, second;
			TS_ASSERT_EQUALS(first.init(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(second.init(), tagd::TAGD_OK);
			for (auto* db : {&first, &second}) {
				const auto ids = db == &first ? std::vector{"z", "a"} : std::vector{"a", "z"};
				for (const auto id : ids) {
					TS_ASSERT_EQUALS(db->put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
						tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
				}

				tagd::abstract_tag subject("subject", tagd::HARD_TAG_SUB,
					tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
				TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS, "a"), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS, "z"), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(db->put(subject), tagd::TAGD_OK);
			}

			auto session = first.get_session();
			auto copy = session;
			TS_ASSERT_EQUALS(first.get(retained, "subject", &session), tagd::TAGD_OK);
			serialized = retained.str();

			tagd::abstract_tag loaded;
			TS_ASSERT_EQUALS(first.get(loaded, "subject", &copy), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(loaded.str(), serialized);
			session.membership([&second](tagd::id_view id) {
				return second.exists(id);
			});
			TS_ASSERT_EQUALS(second.get(loaded, "subject", &session), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(loaded.relations.begin()->object, "a");
			TS_ASSERT_EQUALS(first.get(loaded, "subject"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(loaded.relations.begin()->object, "z");

			tagd::predicate_set missing{{tagd::HARD_TAG_HAS, "missing"}};
			const auto before = missing;
			TS_ASSERT_EQUALS(first.order_predicates(missing), tagd::TS_NOT_FOUND);
			TS_ASSERT_EQUALS(first.code(), tagd::TS_NOT_FOUND);
			TS_ASSERT_EQUALS(missing, before);

			tagd::predicate_set root{{tagd::HARD_TAG_HAS, tagd::HARD_TAG_ENTITY}};
			TS_ASSERT_EQUALS(first.order_predicates(root), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(retained.str(), serialized);
		TS_ASSERT_EQUALS(retained.relations.begin()->object, "z");
	}

	void test_rank_order_is_transparent_and_snapshots_survive_moves() {
		tagd::tagspace::memory db;
		TS_ASSERT_EQUALS(db.init(), tagd::TAGD_OK);
		for (const auto id : {"z_rel", "a_rel", "z_object", "a_object", "subject", "context"}) {
			TS_ASSERT_EQUALS(db.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		}

		tagd::abstract_tag input("subject");
		TS_ASSERT_EQUALS(input.relation("a_rel", "a_object", "2"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.relation("z_rel", "a_object", "10"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.relation("z_rel", "z_object"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.put(input), tagd::TAGD_OK);

		tagd::abstract_tag old;
		TS_ASSERT_EQUALS(db.get(old, "subject"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(old.relations.begin()->relator, "z_rel");
		TS_ASSERT_EQUALS(old.relations.begin()->object, "z_object");
		TS_ASSERT_EQUALS(db.put(tagd::referent("zzz", "z_rel", "context")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.put(tagd::referent("aaa", "a_rel", "context")), tagd::TAGD_OK);

		auto ssn = db.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

		tagd::abstract_tag encoded;
		TS_ASSERT_EQUALS(db.get(encoded, "subject", &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(encoded.relations.begin()->relator, "zzz");
		TS_ASSERT_EQUALS(db.put(tagd::abstract_tag("z_rel", tagd::HARD_TAG_SUB,
			"a_rel", tagd::POS_TAG)), tagd::TAGD_OK);
		tagd::abstract_tag fresh;
		TS_ASSERT_EQUALS(db.get(fresh, "subject"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fresh.relations.begin()->relator, "a_rel");
		TS_ASSERT_EQUALS(old.relations.begin()->relator, "z_rel");
		TS_ASSERT_EQUALS(fresh, old);

		auto old_key = *old.relations.begin();
		auto found = fresh.relations.lower_bound(old_key);
		TS_ASSERT(found != fresh.relations.end());
		if (found != fresh.relations.end()) {
			TS_ASSERT_EQUALS(found->relator, old_key.relator);
			TS_ASSERT_EQUALS(found->object, old_key.object);
		}
		TS_ASSERT_EQUALS(db.put(tagd::abstract_tag("z_object", tagd::HARD_TAG_SUB,
			"a_object", tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.get(fresh, "subject"), tagd::TAGD_OK);

		auto it = fresh.relations.begin();
		TS_ASSERT_EQUALS((++it)->object, "a_object");
		TS_ASSERT_EQUALS(old.relations.begin()->object, "z_object");
	}

	void test_rank_lookup_observes_other_connections() {
		const auto file = (root / "ranks.db").string();
		tagd::tagspace::persistent first, second;
		TS_ASSERT_EQUALS(first.create_file(file), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.put(tagd::abstract_tag("z", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.put(tagd::abstract_tag("a", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(second.open_file(file), tagd::TAGD_OK);

		tagd::abstract_tag subject("subject", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS, "a"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS, "z"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.put(subject), tagd::TAGD_OK);

		tagd::abstract_tag old, fresh;
		TS_ASSERT_EQUALS(first.get(old, "subject"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(old.relations.begin()->object, "z");

		tagd::rank before, after;
		TS_ASSERT_EQUALS(first.lookup_rank(before, "z"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(second.put(tagd::abstract_tag("z", tagd::HARD_TAG_SUB,
			"a", tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.lookup_rank(after, "z"), tagd::TAGD_OK);
		TS_ASSERT(before != after);
		TS_ASSERT_EQUALS(first.get(fresh, "subject"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fresh.relations.begin()->object, "a");
		TS_ASSERT_EQUALS(old.relations.begin()->object, "z");
	}

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
		TS_ASSERT_EQUALS(stack.push_context("nonexistent_context"), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(stack.code(), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(stack.context().size(), 1u);
		TS_ASSERT_EQUALS(stack.context().back(), "zoo");
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

	void test_direct_creation_requires_existing_parent_and_is_exclusive() {
		const auto path = (root / "animals.db").string();
		{
			tagd::tagspace::persistent created;
			TS_ASSERT_EQUALS(created.create_file(path), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(created.put(tagd::abstract_tag("donkey", tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		}

		tagd::tagspace::persistent duplicate;
		TS_ASSERT_EQUALS(duplicate.create_file(path), tagd::TS_DUPLICATE);
		tagd::tagspace::persistent opened;
		TS_ASSERT_EQUALS(opened.open_file(path), tagd::TAGD_OK);
		TS_ASSERT(opened.exists("donkey"));

		tagd::tagspace::persistent missing;
		TS_ASSERT_DIFFERS(missing.create_file((root / "missing/animals.db").string()), tagd::TAGD_OK);
		TS_ASSERT(!std::filesystem::exists(root / "missing"));
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

	void test_reserved_names_cannot_create_or_open_persistent_storage() {
		for (const std::string name : {"_config", "_private", "_"}) {
			const auto home = root / name;
			tagd::tagspace::persistent created;
			TS_ASSERT_EQUALS(created.create(name, home.string()), tagd::TS_ERR);
			TS_ASSERT(created.has_errors());
			TS_ASSERT(!std::filesystem::exists(home));

			// Reserved selection must not reach a database even if one exists on disk.
			const auto dir = home / "tagspaces" / name;
			std::filesystem::create_directories(dir);
			std::ofstream(dir / (name + ".db")) << "not a database";
			tagd::tagspace::persistent opened;
			TS_ASSERT_EQUALS(opened.init(name, home.string()), tagd::TS_ERR);
			TS_ASSERT(opened.has_errors());
			std::ifstream input(dir / (name + ".db"));
			std::string contents;
			std::getline(input, contents);
			TS_ASSERT_EQUALS(contents, "not a database");
		}
	}

	void test_internal_underscore_remains_a_valid_persistent_name() {
		tagd::tagspace::persistent created;
		TS_ASSERT_EQUALS(created.create("my_config", root.string()), tagd::TAGD_OK);
		tagd::tagspace::persistent opened;
		TS_ASSERT_EQUALS(opened.init("my_config", root.string()), tagd::TAGD_OK);
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
