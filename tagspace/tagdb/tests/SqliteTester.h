#pragma once

#include "TestCommon.h"
#include <filesystem>
#include <unistd.h>

class BooleanOccurrenceDatabase : public tagdb::sqlite {
public:
	using ::tagdb::sqlite::term_pos_occurence;
};

class SqliteTester : public CxxTest::TestSuite {
public:
	void test_canonical_identity_cache_loads_on_demand() {
		class database : public tagdb::sqlite {
		public:
			using ::tagdb::sqlite::configure_cache;
			using ::tagdb::sqlite::cache_status;
		};
		database db;
		TS_ASSERT_TAGD_OK(db.init(":memory:"));
		TS_ASSERT_TAGD_OK(db.put(tagd::abstract_tag("subject", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG), nullptr));

		const auto initial = db.cache_status();
		tagd::rank rank;
		TS_ASSERT_TAGD_OK(db.lookup_rank(rank, "subject"));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, initial.identity_reads);
		TS_ASSERT_EQUALS(db.cache_status().predicate_reads, initial.predicate_reads);

		tagd::abstract_tag first, second;
		TS_ASSERT_TAGD_OK(db.get(first, "subject", nullptr));
		const auto loaded = db.cache_status();
		TS_ASSERT_EQUALS(loaded.identity_reads, initial.identity_reads + 1);
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, loaded.identity_reads);
		TS_ASSERT_EQUALS(first.str(), second.str());
		TS_ASSERT_EQUALS(first.rank(), rank);

		db.configure_cache(0, 0, std::chrono::minutes(1), std::chrono::steady_clock::now);
		const auto disabled = db.cache_status().identity_reads;
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_TAGD_OK(db.get(second, "subject", nullptr));
		TS_ASSERT_EQUALS(db.cache_status().identity_reads, disabled + 2);
		TS_ASSERT_EQUALS(first.str(), second.str());
		TS_ASSERT_EQUALS(db.cache_status().entries, 0);
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
		TS_ASSERT_EQUALS(db.del(tagd::abstract_tag("z"), nullptr), tagd::TS_RELATION_DEPENDENCY);
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
			TS_ASSERT_EQUALS(ssn.code(), tagd::TS_RELATION_DEPENDENCY);
		}
	}

	// Contract 2: tagdb::session copy preserves context and sequence state (inherited from tagd::session)
	void test_session_copy_preserves_state(void) {
		TDB_CONS_INIT();

		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("thing", "physical_object", "simple_english"), &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));
		ssn.next_sequence();
		ssn.next_sequence();  // sequence == 2

		tagdb::session ssn2 = ssn;  // copy
		TS_ASSERT_EQUALS(ssn2.context().size(), ssn.context().size());
		TS_ASSERT_EQUALS(ssn2.context()[0], "simple_english");
		TS_ASSERT_EQUALS(ssn2.sequence(), ssn.sequence());

		tagdb::session ssn3(ssn);  // copy ctor
		TS_ASSERT_EQUALS(ssn3.context()[0], "simple_english");
		TS_ASSERT_EQUALS(ssn3.sequence(), ssn.sequence());
	}

	// Contract 3: url/referent overloads on tagdb::tagdb dispatch through an abstract pointer to the sqlite implementation
	void test_url_referent_overloads_dispatch_through_abstract_pointer(void) {
		TDB_CONS_INIT();
		tagdb::tagdb *abstract_tdb = &tdb;

		// url put through abstract pointer
		tagd::url u("http://www.example.com/page");
		TS_ASSERT_TAGD_OK(u.relation("about", "computer"));
		TS_ASSERT_TAGD_OK(abstract_tdb->put(u, &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		// url get through abstract pointer
		tagd::url retrieved;
		TS_ASSERT_TAGD_OK(abstract_tdb->get(retrieved, u.id(), &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
		TS_ASSERT_EQUALS(retrieved.id(), u.id());

		// url del through abstract pointer
		TS_ASSERT_TAGD_OK(abstract_tdb->del(u, &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		// referent put and del through abstract pointer
		tagd::referent r("having", HARD_TAG_HAS, "simple_english");
		TS_ASSERT_TAGD_OK(abstract_tdb->put(r, &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		TS_ASSERT_TAGD_OK(abstract_tdb->del(r, &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
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
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		TS_ASSERT_EQUALS(tdb.expose_get_stmt(tagdb::stmt_t::GET), nullptr);
	}

	// prepare() must return TAGD_OK and produce a non-null cached pointer
	void test_prepare_returns_ok_and_caches_stmt() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		tagd::code tc = tdb.expose_prepare(tagdb::stmt_t::GET, GET_SQL, "get_tag");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_DIFFERS(tdb.expose_get_stmt(tagdb::stmt_t::GET), nullptr);
	}

	// second prepare() must reset+clear, not re-prepare — same pointer, TAGD_OK
	void test_prepare_idempotent_same_pointer() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));

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
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
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
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		tagd::code tc = tdb.expose_bind_text(tagdb::stmt_t::GET, 1, "foo", "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	void test_bind_int_null_guard() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		tagd::code tc = tdb.expose_bind_int(tagdb::stmt_t::GET, 1, 42, "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	void test_bind_null_null_guard() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		tagd::code tc = tdb.expose_bind_null(tagdb::stmt_t::GET, 1, "test");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_INTERNAL_ERR");
	}

	// public API: repeated get() calls must return consistent results (exercises idempotent prepare path)
	void test_repeated_get_correct_results() {
		TagdbSqliteStmtTest tdb;
		TS_ASSERT_TAGD_OK(tdb.init(db_fname));
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
