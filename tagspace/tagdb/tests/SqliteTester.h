#pragma once

#include "TestCommon.h"
#include "tagdb/sqlite.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<tagdb::sqlite>, "tagdb::sqlite must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<tagdb::sqlite>,    "tagdb::sqlite must not be copy-assignable");
static_assert(!std::is_move_constructible_v<tagdb::sqlite>, "tagdb::sqlite must not be move-constructible");
static_assert(!std::is_move_assignable_v<tagdb::sqlite>,    "tagdb::sqlite must not be move-assignable");

#include <filesystem>
#include <unistd.h>

class BooleanOccurrenceDatabase : public tagdb::sqlite {
public:
	using ::tagdb::sqlite::term_pos_occurence;
};

class SqliteTester : public CxxTest::TestSuite {
public:
	void test_replace_url_move_preserves_other_ranks() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::insert;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("animal", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("dog", HARD_TAG_SUB, "animal",
			tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("parent", HARD_TAG_SUB,
			HARD_TAG_URL, tagd::POS_TAG), nullptr));
		const tagd::url original("https://example.com/a");
		TS_ASSERT_TAGD_OK(db.put(original, nullptr));
		tagd::abstract_tag before;
		TS_ASSERT_TAGD_OK(db.get(before, original.hduri(), nullptr));
		// insert() takes stored identity; GET displays URL subjects in URL form.
		tagd::abstract_tag identity(original.hduri(), before.sub_relator(),
			before.super_object(), before.pos());
		std::vector<tagd::abstract_tag> descendants;
		for (const auto* id : {"https://example.com/child1", "https://example.com/child2"}) {
			const tagd::url child(id);
			TS_ASSERT_TAGD_OK(db.insert(tagd::abstract_tag(child.hduri(), HARD_TAG_SUB,
				original.hduri(), tagd::POS_URL), tagd::abstract_tag(identity, before.rank())));
			tagd::abstract_tag stored;
			TS_ASSERT_TAGD_OK(db.get(stored, child.hduri(), nullptr));
			descendants.push_back(stored);
		}
		std::vector<tagd::abstract_tag> unrelated;
		const tagd::id_view unrelated_ids[]{HARD_TAG_ENTITY, HARD_TAG_URL,
			tagd::HARD_TAG_FILE, "animal", "dog", "parent"};
		for (const auto id : unrelated_ids) {
			tagd::abstract_tag tag;
			TS_ASSERT_TAGD_OK(db.get(tag, id, nullptr));
			unrelated.push_back(tag);
		}

		tagd::replace_outcome outcome{};
		TS_ASSERT_TAGD_OK(db.replace(outcome, tagd::url(original.id(), HARD_TAG_SUB, "parent"), nullptr));
		TS_ASSERT_EQUALS(outcome, tagd::replace_outcome::replaced);
		tagd::abstract_tag moved;
		TS_ASSERT_TAGD_OK(db.get(moved, original.hduri(), nullptr));
		TS_ASSERT_EQUALS(moved.super_object(), "parent");
		TS_ASSERT_DIFFERS(moved.rank(), before.rank());
		TS_ASSERT(unrelated.back().rank().contains(moved.rank()));
		for (const auto& prior : unrelated) {
			tagd::abstract_tag after;
			TS_ASSERT_TAGD_OK(db.get(after, prior.id(), nullptr));
			TS_ASSERT_EQUALS(after.rank(), prior.rank());
		}
		for (const auto& prior : descendants) {
			tagd::abstract_tag after;
			TS_ASSERT_TAGD_OK(db.get(after, tagd::canonical_url_id(prior.id()), nullptr));
			TS_ASSERT_EQUALS(after.rank().str(),
				moved.rank().str() + prior.rank().str().substr(before.rank().size()));
		}
	}

	void test_update_rejects_an_empty_source_rank() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::update;
			sqlite3_int64 changes() { return sqlite3_total_changes64(_db); }
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		tagd::abstract_tag subject("subject", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_TAGD_OK(db.put(subject, nullptr));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("parent", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		tagd::abstract_tag destination;
		TS_ASSERT_TAGD_OK(db.get(destination, "parent", nullptr));
		const auto before = db.changes();
		TS_ASSERT_EQUALS(db.update(subject, destination), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(db.changes(), before);
	}

	void test_replace_requires_an_idle_backend() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::exec;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.exec("BEGIN IMMEDIATE"));
		tagd::replace_outcome outcome{};
		TS_ASSERT_EQUALS(db.replace(outcome, tagd::abstract_tag("new", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr), tagd::TS_MISUSE);
		TS_ASSERT_TAGD_OK(db.exec("ROLLBACK"));
		TS_ASSERT(!db.exists("new"));
	}

	void test_replace_duplicate_reads_storage_and_performs_no_write() {
		class database : public tagdb::sqlite {
		public:
			sqlite3_int64 changes() { return sqlite3_total_changes64(_db); }
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		tagd::replace_outcome outcome;
		tagd::url value("https://example.com/a");
		TS_ASSERT_TAGD_OK(value.relation(tagd::predicate(HARD_TAG_HAS, HARD_TAG_MESSAGE,
			"4", tagd::OP_EQ, tagd::TYPE_INTEGER)));
		TS_ASSERT_TAGD_OK(db.replace(outcome, value, nullptr));
		const auto before = db.changes();
		tagd::HDURI encoded(value.hduri());
		encoded.relations = value.relations;
		encoded.type_relations.emplace(tagd::HARD_TAG_TYPE_OF, encoded.hduri());
		TS_ASSERT_EQUALS(db.replace(outcome, encoded, nullptr), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(outcome, tagd::replace_outcome::unchanged);
		TS_ASSERT_EQUALS(db.changes(), before);
		db.clear_errors();
		encoded.relations.clear();
		TS_ASSERT_TAGD_OK(encoded.relation(tagd::predicate(HARD_TAG_HAS, HARD_TAG_MESSAGE,
			"4.0", tagd::OP_EQ, tagd::TYPE_FLOAT)));
		TS_ASSERT_TAGD_OK(db.replace(outcome, encoded, nullptr));
		TS_ASSERT_EQUALS(outcome, tagd::replace_outcome::replaced);
		TS_ASSERT_EQUALS(db.replace(outcome, encoded, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(outcome, tagd::replace_outcome::unchanged);

		tagd::abstract_tag axiomatic("axiomatic", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_TAGD_OK(db.replace(outcome, axiomatic, nullptr));
		axiomatic.type_relations.emplace(tagd::HARD_TAG_TYPE_OF, axiomatic.id());
		axiomatic.type_relations.emplace(tagd::HARD_TAG_TYPE_OF, HARD_TAG_ENTITY);
		const auto before_axioms = db.changes();
		for (const tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_IGNORE_DUPLICATES)}) {
			TS_ASSERT_TAGD_OK(db.replace(outcome, axiomatic, nullptr, flags));
			TS_ASSERT_EQUALS(outcome, tagd::replace_outcome::unchanged);
			TS_ASSERT_EQUALS(db.changes(), before_axioms);
		}
	}

	void test_tag_id_syntax_validation() {
		for (const auto& [id, expected] : std::vector<std::pair<std::string, tagd::code>>{
			{"", tagd::TS_MISUSE}, {":foo", tagd::TS_MISUSE},
			{std::string(tagd::MAX_TAG_LEN + 1, 'a'), tagd::TS_ERR_MAX_TAG_LEN},
			{"ordinary:tag", tagd::TAGD_OK}}) {
			tagdb::sqlite db;
			TS_ASSERT_TAGD_OK(db.init(":memory:"));
			TS_ASSERT_EQUALS(db.put(tagd::abstract_tag(id, HARD_TAG_SUB, HARD_TAG_ENTITY,
				tagd::POS_TAG), nullptr), expected);
		}
	}

	void test_broad_query_reads_each_source_declaration_at_most_once() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::cache_status;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		constexpr unsigned subjects = 128;
		for (unsigned i = 0; i != subjects; ++i) {
			tagd::abstract_tag t("g" + std::to_string(i), HARD_TAG_SUB,
				i ? "g" + std::to_string((i - 1) / 10) : std::string(HARD_TAG_ENTITY), tagd::POS_TAG);
			TS_ASSERT_TAGD_OK(t.relation(HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "shared"));
			TS_ASSERT_TAGD_OK(db.put(t, nullptr));
		}

		db.configure_cache(0, 0, std::chrono::seconds(1), std::chrono::steady_clock::now);
		tagd::interrogator q(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_TAGD_OK(q.relation(HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "shared"));
		for (const tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_INCLUDE_INHERITED_PREDICATES)}) {
			const auto before = db.cache_status().predicate_reads;
			tagd::tag_set found;
			TS_ASSERT_TAGD_OK(db.query(found, q, nullptr, flags));
			TS_ASSERT_EQUALS(found.size(), subjects);
			// The operation already resolved these subjects and their shared root source.
			TS_ASSERT_LESS_THAN_EQUALS(db.cache_status().predicate_reads - before, subjects + 1);
		}
	}

	void test_unrelated_tags_do_not_scale_leaf_validation_reads_or_queries() {
		class database : public tagdb::sqlite {
		public:
			size_t steps = 0;
			void measure() {
				steps = 0;
				for (auto* stmt = sqlite3_next_stmt(_db, nullptr); stmt; stmt = sqlite3_next_stmt(_db, stmt)) {
					sqlite3_stmt_status(stmt, SQLITE_STMTSTATUS_VM_STEP, 1);
				}
				sqlite3_trace_v2(_db, SQLITE_TRACE_PROFILE,
					[](unsigned, void* context, void* statement, void*) {
						auto& db = *static_cast<database*>(context);
						db.steps += sqlite3_stmt_status(static_cast<sqlite3_stmt*>(statement),
							SQLITE_STMTSTATUS_VM_STEP, 1);
						return 0;
					}, this);
			}
			void stop() {
				sqlite3_trace_v2(_db, 0, nullptr, nullptr);
			}
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("leaf", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_TAGD_OK(query.relation(HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE));

		auto work = [&](const char* value) {
			db.measure();
			tagd::abstract_tag leaf("leaf"), read;
			TS_ASSERT_TAGD_OK(leaf.relation(HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, value));
			TS_ASSERT_TAGD_OK(db.put(leaf, nullptr));
			const auto put = db.steps;
			db.steps = 0;
			TS_ASSERT_TAGD_OK(db.get(read, "leaf", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES));
			const auto get = db.steps;
			db.steps = 0;
			tagd::tag_set result;
			TS_ASSERT_TAGD_OK(db.query(result, query, nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES));
			TS_ASSERT_EQUALS(result.size(), 1u);
			const auto select = db.steps;
			db.stop();
			return std::vector<size_t>{put, get, select};
		};
		(void)work("warmup");
		const auto before = work("before");
		for (unsigned i = 0; i != 400; ++i) {
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("unrelated" + std::to_string(i),
				HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}
		const auto after = work("after");

		// Count executed SQL instructions, avoiding machine-speed and wall-clock assumptions.
		for (size_t i = 0; i != before.size(); ++i) {
			TS_ASSERT_LESS_THAN(after[i], before[i] * 2);
		}
	}

	void test_reparenting_preserves_stored_hard_tag_roles() {
		class database : public tagdb::sqlite {
		public:
			std::vector<std::pair<int, int>> hard_roles() {
				std::vector<std::pair<int, int>> roles;
				sqlite3_stmt* stmt = nullptr;
				TS_ASSERT_EQUALS(sqlite3_prepare_v2(_db,
					"SELECT pos,term_pos FROM tags JOIN terms ON tag=terms.ROWID "
					"WHERE substr(term,1,1)='_' ORDER BY term", -1, &stmt, nullptr), SQLITE_OK);
				while (sqlite3_step(stmt) == SQLITE_ROW) {
					roles.emplace_back(sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1));
				}
				sqlite3_finalize(stmt);
				return roles;
			}
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("moving", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		const auto before = db.hard_roles();
		TS_ASSERT(!before.empty());

		for (unsigned i = 0; i != 70; ++i) {
			const auto parent = "parent" + std::to_string(i);
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(parent, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
			tagd::rank rank;
			TS_ASSERT_TAGD_OK(db.lookup_rank(rank, parent));
			if (rank.back() != 42 && rank.back() != 63 && rank.back() != 91) {
				continue;
			}

			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("moving", HARD_TAG_SUB,
				parent, tagd::POS_TAG), nullptr));
			TS_ASSERT_EQUALS(db.hard_roles(), before);
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("moving", HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
			TS_ASSERT_EQUALS(db.hard_roles(), before);
		}
	}

	void test_type_relations_reopen_and_reject_incompatible_storage_without_rewriting_it() {
		char pattern[] = "/tmp/tagd-type-relations-XXXXXX";
		auto* directory = mkdtemp(pattern);
		TS_ASSERT(directory);
		if (!directory) {
			return;
		}
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{directory};
		const auto path = (remove.path / "types.db").string();
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::exec;
		};
		{
			database db;
			TS_ASSERT_TAGD_OK(db.init(path));
			for (const auto id : {"legs", "quadruped", "dog"}) {
				TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB,
					HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
			}
			tagd::abstract_tag source("quadruped"), dog("dog");
			TS_ASSERT_TAGD_OK(source.relation(HARD_TAG_HAS, "legs", "4.0", tagd::OP_EQ, tagd::TYPE_FLOAT));
			TS_ASSERT_TAGD_OK(dog.type_relation(tagd::HARD_TAG_TYPE_OF, "quadruped"));
			TS_ASSERT_TAGD_OK(db.put(source, nullptr));
			TS_ASSERT_TAGD_OK(db.put(dog, nullptr));
			dog.type_relations.clear();
			TS_ASSERT_TAGD_OK(dog.type_relation(tagd::HARD_TAG_TYPE_OF, "dog"));
			TS_ASSERT_TAGD_OK(dog.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_ENTITY));
			TS_ASSERT_TAGD_OK(db.put(dog, nullptr));
		}
		{
			database db;
			TS_ASSERT_TAGD_OK(db.init(path));
			tagd::abstract_tag dog;
			TS_ASSERT_TAGD_OK(db.get(dog, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES));
			TS_ASSERT_EQUALS(dog.type_relations.size(), 1u);
			TS_ASSERT_EQUALS(dog.relations.size(), 1u);
			if (!dog.relations.empty()) {
				TS_ASSERT_EQUALS(dog.relations.begin()->modifier_type, tagd::TYPE_FLOAT);
			}
			TS_ASSERT_TAGD_OK(db.exec("PRAGMA user_version=2"));
		}
		{
			database incompatible;
			TS_ASSERT_EQUALS(incompatible.init(path), tagd::TS_MISUSE);
		}
		sqlite3* raw = nullptr;
		TS_ASSERT_EQUALS(sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK);
		sqlite3_stmt* stmt = nullptr;
		TS_ASSERT_EQUALS(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM type_relations", -1,
			&stmt, nullptr), SQLITE_OK);
		TS_ASSERT_EQUALS(sqlite3_step(stmt), SQLITE_ROW);
		TS_ASSERT_EQUALS(sqlite3_column_int(stmt, 0), 1);
		sqlite3_finalize(stmt);
		TS_ASSERT_EQUALS(sqlite3_prepare_v2(raw, "PRAGMA user_version", -1, &stmt, nullptr), SQLITE_OK);
		TS_ASSERT_EQUALS(sqlite3_step(stmt), SQLITE_ROW);
		TS_ASSERT_EQUALS(sqlite3_column_int(stmt, 0), 2);
		sqlite3_finalize(stmt);
		sqlite3_close(raw);
	}

	void test_dump_rejects_invalid_stored_hduri() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::exec;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.put(tagd::url("http://example.com/a%20b"), nullptr));

		// Bypass URL validation to represent a persisted identity with incompatible escaping.
		const std::string invalid = "hd:com!example!!/a%20b!!!!!!http";
		const std::string sql = "UPDATE terms SET term='" + invalid +
			"' WHERE term='hd:com!example!!/a%2520b!!!!!!http'";
		TS_ASSERT_TAGD_OK(db.exec(sql.c_str()));
		std::stringstream output;
		TS_ASSERT_EQUALS(db.dump(output), tagd::HDURI_ERR);
		TS_ASSERT(output.str().empty());
		TS_ASSERT(db.has_errors());
		std::stringstream errors;
		db.print_errors(errors);
		TS_ASSERT_DIFFERS(errors.str().find(invalid), std::string::npos);
	}

	void test_factory_locations_preserve_open_storage() {
		auto owner = tagdb::make_backend();
		auto& db = *owner;
		TS_ASSERT(tagdb::accepts_storage_file("test.db"));
		TS_ASSERT(!tagdb::accepts_storage_file("test.tagl"));
		TS_ASSERT_EQUALS(tagdb::storage_filename("test"), "test.db");
		TS_ASSERT_EQUALS(db.open({tagdb::storage_kind::remote, "service/database"}),
			tagd::TS_NOT_IMPLEMENTED);

		TS_ASSERT_TAGD_OK(db.open({tagdb::storage_kind::memory}));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("retained", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		TS_ASSERT_EQUALS(db.open({tagdb::storage_kind::memory}), tagd::TS_MISUSE);
		TS_ASSERT(db.exists("retained"));
	}

	void test_search(void) {
		tagdb::sqlite tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		populate_tags(tdb);

		std::string terms = "mammal can swim";
		tagd::tag_set S;
		tagd::code tc = tdb.search(S, terms);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(S.size(), 1);
		TS_ASSERT(tag_set_exists(S, "whale"));

		S.clear();
		terms = "has fangs";
		tc = tdb.search(S, terms);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(S.size(), 2);
		TS_ASSERT(tag_set_exists(S, "spider"));
		TS_ASSERT(tag_set_exists(S, "snake"));

		S.clear();
		terms = "has teeth";
		tc = tdb.search(S, terms);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(S.size(), 1);
		TS_ASSERT(tag_set_exists(S, "mammal"));
		// fangs are teeth, but don't match FTS for teeth
		TS_ASSERT(!tag_set_exists(S, "spider"));
		TS_ASSERT(!tag_set_exists(S, "snake"));

		S.clear();
		terms = "mammal has can";
		tc = tdb.search(S, terms);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(S.size(), 4);
		TS_ASSERT(tag_set_exists(S, "dog"));
		TS_ASSERT(tag_set_exists(S, "cat"));
		TS_ASSERT(tag_set_exists(S, "whale"));
		TS_ASSERT(tag_set_exists(S, "bat"));
	}


	void test_canonical_identity_cache_invalidation_and_expiry() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::cache_status;
			using ::tagdb::sqlite::exec;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		for (const auto id : {"parent", "subject"}) {
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}
		auto now = std::chrono::steady_clock::time_point{};
		db.configure_cache(4096, 1, std::chrono::seconds(10), [&now] {
			return now;
		});

		tagd::abstract_tag original, result;
		TS_ASSERT_TAGD_OK(db.get(original, "subject", nullptr));
		TS_ASSERT_TAGD_OK(db.get(result, "parent", nullptr));
		const auto evicted = db.cache_status().identity_reads;
		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, evicted + 1);
		now += std::chrono::seconds(11);
		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, evicted + 2);

		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("subject", HARD_TAG_SUB,
			"parent", tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.get(result, "subject", nullptr));
		TS_ASSERT_EQUALS(result.super_object(), "parent");
		TS_ASSERT_DIFFERS(result.rank(), original.rank());
		TS_ASSERT_EQUALS(original.super_object(), HARD_TAG_ENTITY);

		// Failed reads must not establish a reusable identity or alter the caller's snapshot.
		TS_ASSERT_TAGD_OK(db.exec("UPDATE tags SET rank=x'FF' WHERE tag=tid('subject')"));
		const auto before_error = db.cache_status().identity_reads;
		const auto saved = result.str();
		TS_ASSERT_EQUALS(db.get(result, "subject", nullptr), tagd::RANK_ERR);
		TS_ASSERT_EQUALS(db.get(result, "subject", nullptr), tagd::RANK_ERR);
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, before_error + 2);
		TS_ASSERT_EQUALS(result.str(), saved);
	}

	void test_rank_ordering_pins_snapshot_across_external_commit() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::exec;
		};
		char pattern[] = "/tmp/tagd-rank-snapshot-XXXXXX";
		auto* directory = mkdtemp(pattern);
		TS_ASSERT(directory != nullptr);
		if (!directory)
			return;
		struct cleanup {
			std::filesystem::path path;
			~cleanup() {
				std::filesystem::remove_all(path);
			}
		} remove{directory};

		const auto path = (remove.path / "ranks.db").string();
		database first, second;
		TS_ASSERT_TAGD_OK(first.init(path));
		TS_ASSERT_TAGD_OK(first.exec("PRAGMA journal_mode=WAL"));
		for (const auto id : {"z", "a"}) {
			TS_ASSERT_TAGD_OK(first.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}
		TS_ASSERT_TAGD_OK(second.init(path));
		bool mutate = false;
		// begin_read samples the clock after pinning, so this write cannot change the active snapshot.
		first.configure_cache(4096, 10, std::chrono::minutes(1), [&] {
			if (mutate) {
				mutate = false;
				TS_ASSERT_TAGD_OK(second.put(tagd::abstract_tag("z", HARD_TAG_SUB,
					"a", tagd::POS_TAG), nullptr));
			}
			return std::chrono::steady_clock::now();
		});

		tagd::predicate_set values{{HARD_TAG_HAS, "z"}, {HARD_TAG_HAS, "a"}};
		TS_ASSERT_TAGD_OK(first.order_predicates(values));
		TS_ASSERT_EQUALS(values.begin()->object, "z");
		mutate = true;
		TS_ASSERT_TAGD_OK(first.order_predicates(values));
		TS_ASSERT(!mutate);
		TS_ASSERT_EQUALS(values.begin()->object, "z");

		const auto retained = values;
		TS_ASSERT_TAGD_OK(first.order_predicates(values));
		TS_ASSERT_EQUALS(values.begin()->object, "a");
		first.close();
		TS_ASSERT_EQUALS(retained.begin()->object, "z");
	}

	void test_rank_cache_disabled_errors_and_mutation_boundaries() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::cache_status;
			using ::tagdb::sqlite::exec;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		for (const auto id : {"z", "a", "subject", "context"}) {
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}

		tagd::predicate_set values{{"z", "a", "10"}, {"z", "a", "2"}, {"a", "z"}};
		const auto predicate_reads = db.cache_status().predicate_reads;
		db.configure_cache(0, 0, std::chrono::seconds(1), std::chrono::steady_clock::now);

		const auto reads = db.cache_status().rank_reads;
		TS_ASSERT_TAGD_OK(db.order_predicates(values));
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, reads + 2);
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, predicate_reads);
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);

		auto uncached = values;
		db.configure_cache(4096, 10, std::chrono::seconds(60), std::chrono::steady_clock::now);
		TS_ASSERT_TAGD_OK(db.order_predicates(values));

		const auto warm = db.cache_status().rank_reads;
		TS_ASSERT_TAGD_OK(db.order_predicates(values));
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, warm);
		TS_ASSERT(std::equal(values.begin(), values.end(), uncached.begin(), uncached.end()));
		TS_ASSERT(db.cache_status().bytes <= 4096);

		tagd::rank r;
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "z"));

		// DDL on this connection does not increment SQLite's total_changes counter.
		TS_ASSERT_TAGD_OK(db.exec("ALTER TABLE tags RENAME TO hidden_tags"));
		TS_ASSERT_DIFFERS(db.lookup_rank(r, "z"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_TAGD_OK(db.exec("ALTER TABLE hidden_tags RENAME TO tags"));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "z"));

		tagd::abstract_tag subject("subject");
		TS_ASSERT_TAGD_OK(subject.relation("z", "a", "literal"));
		TS_ASSERT_TAGD_OK(db.put(subject, nullptr));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "z"));
		TS_ASSERT_EQUALS(db.del(tagd::abstract_tag("z"), nullptr), tagd::TS_DEPENDENCY);
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "z"));
		TS_ASSERT_TAGD_OK(db.put(tagd::referent("alias", "z", "context"), nullptr));
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "z"));
		TS_ASSERT_TAGD_OK(db.del(tagd::referent("alias", "z", "context"), nullptr));
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_EQUALS(db.lookup_rank(r, "later"), tagd::TS_NOT_FOUND);
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("later", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "later"));
		TS_ASSERT_TAGD_OK(db.del(tagd::abstract_tag("later"), nullptr));
		TS_ASSERT_EQUALS(db.lookup_rank(r, "later"), tagd::TS_NOT_FOUND);

		tagd::abstract_tag snapshot;
		TS_ASSERT_TAGD_OK(db.get(snapshot, "subject", nullptr));

		const auto text = snapshot.str();
		TS_ASSERT_TAGD_OK(db.exec("UPDATE tags SET rank=x'FF' WHERE tag=tid('a')"));

		const auto before_error = db.cache_status().rank_reads;
		const auto invalid = db.lookup_rank(r, "a");
		TS_ASSERT_DIFFERS(invalid, tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(invalid, tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.lookup_rank(r, "a"), invalid);
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, before_error + 2);
		TS_ASSERT_EQUALS(db.get(snapshot, "subject", nullptr), invalid);
		TS_ASSERT_EQUALS(snapshot.str(), text);

		auto ssn = test_session(db);
		TS_ASSERT_EQUALS(db.get(snapshot, "subject", &ssn), invalid);
		TS_ASSERT_EQUALS(ssn.code(), invalid);
	}

	void test_rank_lookup_cache_bounds_and_invalidation() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::cache_status;
			using ::tagdb::sqlite::exec;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		for (const auto id : {"z", "a", "child"}) {
			TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				id == std::string_view("child") ? "z" : HARD_TAG_ENTITY,
				tagd::POS_TAG), nullptr));
		}

		auto now = std::chrono::steady_clock::time_point{};
		db.configure_cache(4096, 2, std::chrono::seconds(10), [&now] {
			return now;
		});

		tagd::rank r, original;
		const auto reads = db.cache_status().rank_reads;
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, HARD_TAG_ENTITY));
		TS_ASSERT(r.empty());
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, reads);
		TS_ASSERT_TAGD_OK(db.lookup_rank(original, "child"));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, reads + 1);
		TS_ASSERT_EQUALS(db.lookup_rank(r, "missing"), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.lookup_rank(r, "missing"), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, reads + 2);
		TS_ASSERT_EQUALS(db.cache_status().entries, 2);
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "a"));

		const auto after_eviction = db.cache_status().rank_reads;
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, after_eviction);
		TS_ASSERT_EQUALS(db.lookup_rank(r, "missing"), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(db.cache_status().rank_reads, after_eviction + 1);
		now += std::chrono::seconds(11);
		TS_ASSERT(db.exists("child"));
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_EQUALS(db.cache_status().entries, 1);
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("z", HARD_TAG_SUB, "a",
			tagd::POS_TAG), nullptr));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT(r != original);

		const auto moved = r;
		TS_ASSERT_TAGD_OK(db.exec("BEGIN"));
		TS_ASSERT_TAGD_OK(db.exec("UPDATE tags SET rank='temporary' WHERE tag=tid('child')"));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT(r != moved);
		TS_ASSERT_TAGD_OK(db.exec("ROLLBACK"));
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_EQUALS(r, moved);
		db.configure_cache(1, 2, std::chrono::seconds(10), [&now] {
			return now;
		});
		TS_ASSERT_TAGD_OK(db.lookup_rank(r, "child"));
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
		TS_ASSERT_EQUALS(db.cache_status().bytes, 0);
		db.close();
		TS_ASSERT_EQUALS(db.lookup_rank(r, "child"), tagd::TS_MISUSE);
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_EQUALS(db.lookup_rank(r, "child"), tagd::TS_NOT_FOUND);
	}

	void test_referent_helpers_preserve_modifier_values_and_types() {
		class referent_database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::encode_referents;
			using ::tagdb::sqlite::decode_referents;
		};
		referent_database tdb;
		TS_ASSERT_TAGD_OK(tdb.init(":memory:"));
		for (const auto id : {"context", "10"}) {
			TS_ASSERT_TAGD_OK(tdb.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));
		}
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("ten", "10", "context"), nullptr));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("text", HARD_TAG_MESSAGE, "context"), nullptr));

		auto ssn = test_session(tdb);
		TS_ASSERT_TAGD_OK(ssn.push_context("context"));

		for (const auto type : {tagd::TYPE_INTEGER, tagd::TYPE_FLOAT, tagd::TYPE_STRING}) {
			tagd::predicate_set canonical, encoded, decoded;
			canonical.emplace(HARD_TAG_HAS, HARD_TAG_MESSAGE, "10", tagd::OP_GT, type);
			tdb.encode_referents(encoded, canonical, &ssn);
			TS_ASSERT_EQUALS(encoded.size(), 1);
			for (const auto& p : encoded) {
				TS_ASSERT_EQUALS(p.object, "text");
				TS_ASSERT_EQUALS(p.modifier, "10");
				TS_ASSERT_EQUALS(p.opr8r, tagd::OP_GT);
				TS_ASSERT_EQUALS(p.modifier_type, type);
			}
			tdb.decode_referents(decoded, encoded, &ssn);
			TS_ASSERT_EQUALS(decoded, canonical);
		}

		tagd::predicate_set literal, decoded;
		literal.emplace(HARD_TAG_HAS, "text", "ten");
		tdb.decode_referents(decoded, literal, &ssn);
		TS_ASSERT_EQUALS(decoded.size(), 1);
		for (const auto& p : decoded) {
			TS_ASSERT_EQUALS(p.object, HARD_TAG_MESSAGE);
			TS_ASSERT_EQUALS(p.modifier, "ten");
		}

		for (const auto value : {"2", "10"}) {
			tagd::abstract_tag subject(value == std::string_view("2") ? "small" : "large",
				HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
			TS_ASSERT_TAGD_OK(subject.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, value));
			TS_ASSERT_TAGD_OK(tdb.put(subject, nullptr));
		}

		const tagd::predicate numeric(HARD_TAG_HAS, "text", "2",
			tagd::OP_GT, tagd::TYPE_INTEGER);
		tagd::tag_set results;
		TS_ASSERT_TAGD_OK(tdb.related(results, numeric, &ssn));
		TS_ASSERT_EQUALS(results.size(), 1);
		for (const auto& result : results) {
			TS_ASSERT_EQUALS(result.id(), "large");
			TS_ASSERT(result.related(HARD_TAG_HAS, "text", "10"));
		}
	}

	void test_boolean_occurrence_preserves_declared_pos() {
		BooleanOccurrenceDatabase tdb;
		TS_ASSERT_TAGD_OK(tdb.init(":memory:"));
		for (auto parent : {HARD_TAG_TRUE, HARD_TAG_FALSE}) {
			const bool truth = parent == HARD_TAG_TRUE;
			const auto id = truth ? "yes" : "no";
			const auto child = truth ? "certainly" : "never";
			const auto pos = truth ? tagd::POS_TRUE : tagd::POS_FALSE;
			TS_ASSERT_TAGD_OK(tdb.put(tagd::abstract_tag(id, HARD_TAG_SUB, parent, tagd::POS_UNKNOWN), nullptr));
			TS_ASSERT_EQUALS(tdb.term_pos_occurence(id, nullptr, false), pos);
			TS_ASSERT_TAGD_OK(tdb.put(tagd::abstract_tag(child, HARD_TAG_SUB, id, tagd::POS_UNKNOWN), nullptr));
			tagd::tagspace_session ssn([&tdb](tagd::id_view term) {
				return tdb.exists(term);
			});
			const auto occurrence = tdb.term_pos_occurence(id, &ssn, true);
			TS_ASSERT(occurrence & pos);
			TS_ASSERT(occurrence & tagd::POS_SUB_OBJECT);
			TS_ASSERT_EQUALS(ssn.code(), tagd::TS_DEPENDENCY);
		}
	}


};

/*
 * Exposes protected stmt_t cache interface for white-box contract tests.
 * Uses explicit wrappers and ::tagdb:: global-scope qualifier to avoid the injected
 * base-class name 'tagdb' shadowing the tagdb namespace inside the class body.
 */
class TagdbSqliteStmtTest : public tagdb::sqlite {
public:
	TagdbSqliteStmtTest() = default;
	tagd::code expose_prepare(::tagdb::stmt_t k, const char *sql, const char *label = nullptr) {
		return prepare(k, sql, label);
	}
	sqlite3_stmt* expose_get_stmt(::tagdb::stmt_t k) { return get_stmt(k); }
	tagd::code expose_bind_text(::tagdb::stmt_t k, int i, const char *v, const char *l = nullptr) {
		return bind_text(k, i, v, l);
	}
	tagd::code expose_bind_int(::tagdb::stmt_t k, int i, int v, const char *l = nullptr) {
		return bind_int(k, i, v, l);
	}
	tagd::code expose_bind_null(::tagdb::stmt_t k, int i, const char *l = nullptr) {
		return bind_null(k, i, l);
	}
};

class StmtCacheTester : public CxxTest::TestSuite {
public:
	// SQL for stmt_t::GET; must match sqlite.cc
	static constexpr const char* GET_SQL =
		"SELECT idt(tag), pos, idt(sub_relator), idt(super_object), rank "
		"FROM tags WHERE tag = tid(?)";

	// get_stmt() on a never-prepared key must return nullptr, not a garbage pointer
	void test_get_stmt_returns_null_when_unprepared() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		TS_ASSERT_EQUALS(tdb.expose_get_stmt(tagdb::stmt_t::GET), nullptr);
	}

	// prepare() must return TAGD_OK and produce a non-null cached pointer
	void test_prepare_returns_ok_and_caches_stmt() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		tagd::code tc = tdb.expose_prepare(tagdb::stmt_t::GET, GET_SQL, "get_tag");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_DIFFERS(tdb.expose_get_stmt(tagdb::stmt_t::GET), nullptr);
	}

	// second prepare() must reset+clear, not re-prepare — same pointer, TAGD_OK
	void test_prepare_idempotent_same_pointer() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));

		TS_ASSERT_TAGD_OK(tdb.expose_prepare(tagdb::stmt_t::GET, GET_SQL, "get_tag"));
		sqlite3_stmt *first = tdb.expose_get_stmt(tagdb::stmt_t::GET);
		TS_ASSERT_DIFFERS(first, nullptr);

		tagd::code tc = tdb.expose_prepare(tagdb::stmt_t::GET, GET_SQL, "get_tag");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		sqlite3_stmt *second = tdb.expose_get_stmt(tagdb::stmt_t::GET);
		TS_ASSERT_EQUALS(first, second);  // same cached handle, not a new allocation
	}

	// get_stmt() must be a pure lookup with no side effects — same pointer every call
	void test_get_stmt_stable_no_side_effects() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		TS_ASSERT_TAGD_OK(tdb.expose_prepare(tagdb::stmt_t::GET, GET_SQL, "get_tag"));

		sqlite3_stmt *p1 = tdb.expose_get_stmt(tagdb::stmt_t::GET);
		sqlite3_stmt *p2 = tdb.expose_get_stmt(tagdb::stmt_t::GET);
		sqlite3_stmt *p3 = tdb.expose_get_stmt(tagdb::stmt_t::GET);
		TS_ASSERT_DIFFERS(p1, nullptr);
		TS_ASSERT_EQUALS(p1, p2);
		TS_ASSERT_EQUALS(p2, p3);
	}

	// bind_* without prior prepare() must return TS_INTERNAL_ERR, not crash or UB
	void test_bind_text_null_guard() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		tagd::code tc = tdb.expose_bind_text(tagdb::stmt_t::GET, 1, "foo", "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	void test_bind_int_null_guard() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		tagd::code tc = tdb.expose_bind_int(tagdb::stmt_t::GET, 1, 42, "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	void test_bind_null_null_guard() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		tagd::code tc = tdb.expose_bind_null(tagdb::stmt_t::GET, 1, "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	// public API: repeated get() calls must return consistent results (exercises idempotent prepare path)
	void test_repeated_get_correct_results() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.open({tagdb::storage_kind::memory}));
		populate_tags(tdb);
		tagdb::session ssn = test_session(tdb);

		// "unicorn" is not in populate_tags — both lookups must return TS_NOT_FOUND
		tagd::abstract_tag t1, t2;
		tagd::code tc1 = tdb.get(t1, "unicorn", &ssn);
		tagd::code tc2 = tdb.get(t2, "unicorn", &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc1), "TS_NOT_FOUND");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc2), "TS_NOT_FOUND");

		// "dog" IS in populate_tags — both lookups must return the same correct result
		tagd::abstract_tag t3, t4;
		tc1 = tdb.get(t3, "dog", &ssn);
		tc2 = tdb.get(t4, "dog", &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc1), "TAGD_OK");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc2), "TAGD_OK");
		TS_ASSERT_EQUALS(t3.super_object(), "mammal");
		TS_ASSERT_EQUALS(t4.super_object(), "mammal");
	}
};
