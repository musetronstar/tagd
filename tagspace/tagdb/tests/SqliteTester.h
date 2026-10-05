#pragma once

#include "TestCommon.h"

class BooleanOccurrenceDatabase : public tagdb::sqlite {
public:
	using ::tagdb::sqlite::term_pos_occurence;
};

class SqliteTester : public CxxTest::TestSuite {
public:
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
			tagd::tagspace_session ssn([&tdb](tagd::id_view term) { return tdb.exists(term); });
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
