#pragma once

#include "TestCommon.h"
#include <type_traits>
#include <utility>

// Contract: [[nodiscard]] is a C++17 attribute; this build requires it
static_assert(__has_cpp_attribute(nodiscard), "[[nodiscard]] required for tagdb method contracts");

// Contract 1: tagdb::tagdb and tagdb::sqlite are execution-context types — not copyable or movable
static_assert(!std::is_copy_constructible_v<tagdb::tagdb>,  "tagdb::tagdb must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<tagdb::tagdb>,     "tagdb::tagdb must not be copy-assignable");
static_assert(!std::is_move_constructible_v<tagdb::tagdb>,  "tagdb::tagdb must not be move-constructible");
static_assert(!std::is_move_assignable_v<tagdb::tagdb>,     "tagdb::tagdb must not be move-assignable");

static_assert(!std::is_copy_constructible_v<tagdb::sqlite>, "tagdb::sqlite must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<tagdb::sqlite>,    "tagdb::sqlite must not be copy-assignable");
static_assert(!std::is_move_constructible_v<tagdb::sqlite>, "tagdb::sqlite must not be move-constructible");
static_assert(!std::is_move_assignable_v<tagdb::sqlite>,    "tagdb::sqlite must not be move-assignable");
static_assert(!std::is_assignable_v<decltype(std::declval<tagd::abstract_tag>().id()), tagd::id_string>,
	"abstract_tag identity must remain constructor-only");

#define TS_ASSERT_TAGD_OK(EXPR) TS_ASSERT_EQUALS((EXPR), tagd::TAGD_OK)

class Tester : public CxxTest::TestSuite {
    public:

	void test_hard_tag_pos(void) {
		std::string id(HARD_TAG_ENTITY);
		tagd::part_of_speech pos = tagdb::hard_tag::pos(id);
		TS_ASSERT_EQUALS(pos, tagd::POS_TAG);

		id = "haha";
		pos = tagdb::hard_tag::pos(id);
		TS_ASSERT_EQUALS(pos, tagd::POS_UNKNOWN);
	}

	void test_hard_tag_get(void) {
		std::string id(HARD_TAG_ENTITY);
		tagd::abstract_tag t;
		tagd::code tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_ENTITY);
		TS_ASSERT(t.rank().empty());
 
		t.clear();
		id = HARD_TAG_IS_A;
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_IS_A);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.rank().dotted_str(), "1.1");

		t.clear();
		id = HARD_TAG_HAS;
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_HAS);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_RELATOR);

		t.clear();
		id = HARD_TAG_ERROR;
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_ERROR);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_EVENT);

		t.clear();
		id = HARD_TAG_ERROR_TS_NOT_FOUND;
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_ERROR_TS_NOT_FOUND);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_ERROR);

		t.clear();
		id = HARD_TAG_TAGDB_PUT_EVENT;
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_TAGDB_PUT_EVENT);
		TS_ASSERT_EQUALS(t.sub_relator(), HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_TAGDB_EVENT);

		t.clear();
		id = "haha";
		tc = tagdb::hard_tag::get(t, id);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
	}

	void test_http_event_hard_tags(void) {
		// _event:http_request and _event:http_response are children of _event:http
		tagd::abstract_tag t;
		tagd::code tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_HTTP_REQUEST_EVENT);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_RESPONSE_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.id(), HARD_TAG_HTTP_RESPONSE_EVENT);
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_EVENT);

		// method events are children of _event:http_request
		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_GET_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_REQUEST_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_HEAD_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_REQUEST_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_PUT_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_REQUEST_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_POST_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_REQUEST_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_REQUEST_DELETE_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_REQUEST_EVENT);

		// response status events are children of _event:http_response
		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_RESPONSE_OK_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_RESPONSE_EVENT);

		t.clear();
		tc = tagdb::hard_tag::get(t, HARD_TAG_HTTP_RESPONSE_NOT_FOUND_EVENT);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_HTTP_RESPONSE_EVENT);
	}

    void test_init(void) {
        tagdb_type tdb;
        tagd::code tc = tdb.init(db_fname);

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag t;
        tc = tdb.get(t, HARD_TAG_ENTITY, nullptr);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t.id(), HARD_TAG_ENTITY);
        TS_ASSERT_EQUALS(t.super_object(), HARD_TAG_ENTITY);
    }

	void test_put_emits_consumable_tagdb_event(void) {
		std::stringstream log_ss;
		tagd::logger log(log_ss);
		log.level(tagd::log_level::EMERGENCY);
		log.level(HARD_TAG_ROLE_TAGDB, tagd::log_level::NOTICE);
		TAGDB_SET_LOGGER(&log);

		tagdb_type tdb;
		TS_ASSERT_EQUALS(tdb.init(db_fname), tagd::TAGD_OK)

		tagdb::session ssn = tdb.get_session();
		tagd::tag t("physical_object", HARD_TAG_ENTITY);

		TS_ASSERT_EQUALS(tdb.put(t, &ssn), tagd::TAGD_OK)

		const std::string logged = log_ss.str();
		const std::string::size_type ev_pos = logged.find(tagd::EVURI_SCHEME);
		TS_ASSERT_DIFFERS(ev_pos, std::string::npos)
		const std::string::size_type newline = logged.find('\n', ev_pos);
		TS_ASSERT_DIFFERS(newline, std::string::npos)

		tagd::event ev(logged.substr(ev_pos, newline - ev_pos));
		TS_ASSERT_EQUALS(ev.code(), tagd::TAGD_OK)
		TS_ASSERT_EQUALS(ev.event_type_tag(), HARD_TAG_TAGDB_PUT_EVENT)
		TS_ASSERT_EQUALS(ev.super_object(), HARD_TAG_TAGDB_PUT_EVENT)
		TS_ASSERT_EQUALS(ev.program(), "tagdb")
		TS_ASSERT_EQUALS(ev.session_id(), ssn.id())

		TAGDB_SET_LOGGER(nullptr);
	}

    void test_put_get_rank(void) {
        tagdb_type tdb;
        TS_ASSERT_TAGD_OK(tdb.init(db_fname));

        tagd::tag a("physical_object", HARD_TAG_ENTITY);
        tagd::code tc = tdb.put(a, nullptr);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b;
        tc = tdb.get(b, "physical_object", nullptr);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(b.id(), "physical_object");
        TS_ASSERT_EQUALS(b.super_object(), HARD_TAG_ENTITY);

		std::string r_dotted = b.rank().dotted_str();
		// living_thing will be first child
		r_dotted.append(".1");

        tagd::tag c("living_thing", "physical_object");
        tc = tdb.put(c, nullptr); // rank not updated
        TS_ASSERT_EQUALS(tc, tagd::TAGD_OK);

        tc = tdb.get(c, c.id(), nullptr);  // get rank
        TS_ASSERT_EQUALS(tc, tagd::TAGD_OK);
		
        TS_ASSERT_EQUALS(c.rank().dotted_str(), r_dotted);
    }

	void test_put_id_equals_super_object(void) {
        TDB_CONS_INIT();

        tagd::tag a("dog", "dog");
		tagd::code tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");   // system error
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");         // user error
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_MISUSE"); // user error
    }

    void test_get(void) {
        TDB_CONS_INIT();

		tagd::tag dog("dog", "mammal");
		TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "legs", "4"))
		TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "tail"))
		TS_ASSERT_TAGD_OK(dog.relation("can", "bark"))
		tagd::abstract_tag t1;
		tagd::code tc;

        tc = tdb.get(t1, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT(!ssn.has_errors());
		TS_ASSERT_EQUALS(pos_str(t1.pos()) , "POS_TAG");
		// compare against the ranked identity returned by tagdb
		tagd::abstract_tag ranked_dog(dog, t1.rank());
	        TS_ASSERT(t1 == ranked_dog);

		tagd::abstract_tag t2;

        tc = tdb.get(t2, "haha", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_NOT_FOUND");
        TS_ASSERT(ssn.has_errors());
        TS_ASSERT(t2.empty());

		ssn.clear_errors();
		tc = tdb.get(t2, "doodoo", &ssn, tagdb::F_NO_NOT_FOUND_ERROR);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT(!ssn.has_errors());
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT(t2.empty());

		// not-found are session errors, not system errors
        TS_ASSERT(!tdb.has_errors());
    }

    void test_put_referent(void) {
        TDB_CONS_INIT();

        tagd::referent a("thing", "physical_object", "simple_english");
        tagd::code tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_DUPLICATE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		// same thing cannot refer to different tag in same context
        tagd::referent a1("thing", "animal", "simple_english");
        tc = tdb.put(a1, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_DUPLICATE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_DUPLICATE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

        tagd::referent b("doodoo", "haha", "simple_english");  // unknown refers_to
        tc = tdb.put(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_REFERS_TO_UNK");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

        tagd::referent c("pictures", "movie", "oops");  // unknown context
        tc = tdb.put(c, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_CONTEXT_UNK");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::referent d("pictures", "movie", "event");
		tc = tdb.put(d, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::tag photography("photography", "visual_art");
		TS_ASSERT_TAGD_OK(tdb.put(photography, &ssn));

		// same thing cannot refer to different tag in same context
		tagd::referent d1("pictures", "photography", "event");
		tc = tdb.put(d1, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_DUPLICATE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
    }

	void test_put_refers_to_self(void) {
        TDB_CONS_INIT();

		// thing not a tag
        tagd::referent a("thing", "thing", "simple_english");
        auto tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_MISUSE");

		// dog a tag
        tagd::referent b("dog", "dog", "simple_english");
        tc = tdb.put(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_MISUSE");
    }

	void test_delete_tag(void) {
        TDB_CONS_INIT();

        tagd::tag a;
        tagd::code tc = tdb.get(a, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b("dog");
        tc = tdb.del(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag c;
        tc = tdb.get(c, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		ssn.clear_errors();

		// delete tag that is a super_object and relations.object
		tc = tdb.del(tagd::tag("teeth"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_RELATION_DEPENDENCY");
		TS_ASSERT_EQUALS(ssn.errors().size() , 2);
		auto errs = ssn.errors();
		if (errs.size() == 2) {
			auto it = errs.begin();
			TS_ASSERT(it->related(HARD_TAG_CAUSED_BY, "super_object", "teeth"));
			it++;
			TS_ASSERT(it->related(HARD_TAG_CAUSED_BY, "object", "teeth"));
		}
		ssn.clear_errors();

		// delete failed, so tag still exists
		tagd::tag d;
		tc = tdb.get(d, "teeth", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// delete tag that is a super_object and has relations
		tc = tdb.del(tagd::tag("mammal"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_RELATION_DEPENDENCY");

		// delete failed, so tag and relations still exists
		tagd::tag e;
		tc = tdb.get(e, "mammal", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(e.super_object() , "vertibrate");
		TS_ASSERT(e.related(HARD_TAG_HAS, "blood", "warm"));
		TS_ASSERT(e.related(HARD_TAG_HAS, "teeth"));

		// delete non-existing tag
		tagd::tag noexists("haha");
		ssn.clear_errors();
        tc = tdb.del(noexists, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		TS_ASSERT(ssn.has_errors());

		// delete non-existing - can't override not-found error
		ssn.clear_errors();
        tc = tdb.del(noexists, &ssn, tagdb::F_NO_NOT_FOUND_ERROR);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		TS_ASSERT(!ssn.has_errors());

		// all errors were user errors
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
    }

	void test_delete_relations(void) {
        TDB_CONS_INIT();

        tagd::tag a;
        tagd::code tc = tdb.get(a, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b("dog");
		TS_ASSERT_TAGD_OK(b.relation(HARD_TAG_HAS, "tail"))
		TS_ASSERT_TAGD_OK(b.relation("can", "bark"))
        tc = tdb.del(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		tdb.clear_errors();

        tagd::tag c;
        tc = tdb.get(c, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT(c.related(HARD_TAG_HAS, "legs", "4"));
		TS_ASSERT(!c.related(HARD_TAG_HAS, "tail"));
		TS_ASSERT(!c.related("can", "bark"));

		// delete relation it doesn't have
		tagd::tag d("dog");
		TS_ASSERT_TAGD_OK(d.relation("can", "meow"))
        tc = tdb.del(d, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		TS_ASSERT_EQUALS(std::string("cannot delete non-existent relation: dog can meow"), tdb.last_error().message());
		tdb.clear_errors();

		// delete unknown tag
		tagd::tag e("snarf");
        tc = tdb.del(e, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
    }

	void test_get_referent(void) {
        TDB_CONS_INIT();

        tagd::referent thing("thing", "physical_object", "simple_english");
        TS_ASSERT_TAGD_OK(tdb.put(thing, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::tag t;
		tagd::code tc;
		tc = tdb.get(t, "thing", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_AMBIGUOUS");

		tc = ssn.push_context("simple_english");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tc = tdb.get(t, "thing", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t.id(), "thing");
        TS_ASSERT(t.related(HARD_TAG_REFERS_TO, "physical_object"));
	}

	void test_illegal_referent_context(void) {
		TDB_CONS_INIT();

		tagd::code tc;

		tc = tdb.put(tagd::referent("physical_object", "physical_object", "substance"), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_REFERS_TO));

		tc = tdb.put(tagd::referent("", "physical_object", "substance"), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_EMPTY));

		tc = tdb.put(tagd::referent(HARD_TAG_ENTITY, "physical_object", "substance"), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_ENTITY));

		tc = tdb.put(tagd::referent("thing", "", "substance"), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS_TO, HARD_TAG_EMPTY));

		tc = tdb.put(tagd::referent("thing", HARD_TAG_ENTITY, "substance"), &ssn);  // _refers_to _entity -- OK
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tc = tdb.put(tagd::referent("thing", "physical_object", ""), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_EMPTY));

		tc = tdb.put(tagd::referent("thing", "physical_object", HARD_TAG_ENTITY), &ssn);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		TS_ASSERT(ssn.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_ENTITY));

		tc = ssn.push_context("");
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");

		tc = ssn.push_context(HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

    void test_ambiguous_referent(void) {
        TDB_CONS_INIT();

		tagd::referent thing2("thing", "animal", "living_thing");
        TS_ASSERT_TAGD_OK(tdb.put(thing2, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::tag t;
		auto tc = tdb.get(t, "thing", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_AMBIGUOUS");

		TS_ASSERT_TAGD_OK(ssn.push_context("living_thing"));
		tc = tdb.get(t, "thing", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t.id(), "thing");
        TS_ASSERT(t.related(HARD_TAG_REFERS_TO, "animal"));

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

    void test_referent_override(void) {
        TDB_CONS_INIT();

		tagd::code tc;
		tagd::tag prg("cat_program", "program");
		TS_ASSERT_TAGD_OK(tdb.put(prg, &ssn));

        tagd::referent cat("cat", "cat_program", "computer");
		tc = tdb.put(cat, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::tag t1;
		tc = tdb.get(t1, "cat", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t1.id(), "cat");
        TS_ASSERT_EQUALS(t1.super_object(), "mammal");

		TS_ASSERT_TAGD_OK(ssn.push_context("computer"));
		tagd::tag t2;
		tc = tdb.get(t2, "cat", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t2.id(), "cat");
        TS_ASSERT_EQUALS(t2.super_object(), "program");
        TS_ASSERT(t2.related(HARD_TAG_REFERS_TO, "cat_program"));

		// ssn.push_context("animal"); // this might be intuitive but contexts only decode referents
		ssn.pop_context();
		tagd::tag t3;
		tc = tdb.get(t3, "cat", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t3.id(), "cat");
        TS_ASSERT_EQUALS(t3.super_object(), "mammal");

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

    void test_query_referents(void) {
        tagdb_type tdb;
        TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		auto ssn = tdb.get_session();
        size_t num_referents = populate_tags(tdb);

        tagd::referent thing_physical_object("thing", "physical_object", "simple_english");
        auto tc = tdb.put(thing_physical_object, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::referent thing_animal("thing", "animal", "living_thing");
        tc = tdb.put(thing_animal, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::referent creature("creature", "animal", "living_thing");
        tc = tdb.put(creature, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag_set S;
		// refers.empty() && refers_to.empty() && context.empty()
		// all referents
        tagd::interrogator q_referent(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        tc = tdb.query(S, q_referent, &ssn);
		//tagd::print_tags(S);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), (3+num_referents));
        TS_ASSERT(tag_set_exists(S, thing_physical_object));
        TS_ASSERT(tag_set_exists(S, thing_animal));
        TS_ASSERT(tag_set_exists(S, creature));

		// refers.empty() && refers_to.empty() && !context.empty()
        tagd::interrogator q_context_living_thing(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_context_living_thing.relation(HARD_TAG_CONTEXT, "living_thing"))
        S.clear();
        tc = tdb.query(S, q_context_living_thing, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, thing_animal));
        TS_ASSERT(tag_set_exists(S, creature));

		// refers.empty() && !refers_to.empty() && context.empty()
        tagd::interrogator q_refers_to(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_to.relation(HARD_TAG_REFERS_TO, "animal"))
        S.clear();
        tc = tdb.query(S, q_refers_to, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, thing_animal));
        TS_ASSERT(tag_set_exists(S, creature));

		// refers.empty() && !refers_to.empty() && !context.empty()
        tagd::interrogator q_refers_to_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_to_context.relation(HARD_TAG_REFERS_TO, "animal"))
        TS_ASSERT_TAGD_OK(q_refers_to_context.relation(HARD_TAG_CONTEXT, "physical_object"))
		// living_thing _is_a physical_object
        S.clear();
        tc = tdb.query(S, q_refers_to_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, thing_animal));
        TS_ASSERT(tag_set_exists(S, creature));

		// !refers.empty() && refers_to.empty() && context.empty()
        tagd::interrogator q_refers(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers.relation(HARD_TAG_REFERS, "thing"))
        S.clear();
        tc = tdb.query(S, q_refers, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, thing_physical_object));
        TS_ASSERT(tag_set_exists(S, thing_animal));

		// !refers.empty() && refers_to.empty() && !context.empty()
        tagd::interrogator q_refers_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_context.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_context.relation(HARD_TAG_CONTEXT, "physical_object"))
        S.clear();
        tc = tdb.query(S, q_refers_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, thing_animal));

		// !refers.empty() && !refers_to.empty() && context.empty()
        tagd::interrogator q_refers_refers_to(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_refers_to.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to.relation(HARD_TAG_REFERS_TO, "animal"))
        S.clear();
        tc = tdb.query(S, q_refers_refers_to, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, thing_animal));

		// !refers.empty() && !refers_to.empty() && !context.empty()
        tagd::interrogator q_refers_refers_to_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_REFERS_TO, "animal"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_CONTEXT, "physical_object"))
        S.clear();
        tc = tdb.query(S, q_refers_refers_to_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, thing_animal));

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

    void test_insert_referent_relations(void) {
        TDB_CONS_INIT();

		TS_ASSERT_TAGD_OK(tdb.put(tagd::tag("bite", "action"), &ssn));
		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));
        tagd::tag a("dog");
		// has _refers_to _has _context simple_english
        TS_ASSERT_TAGD_OK(a.relation("has", "teeth"));  // new relations
        TS_ASSERT_TAGD_OK(a.relation("can", "bite"))
        tagd::code rc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(rc), "TAGD_OK");

		tagd::tag b;
		rc = tdb.get(b, "dog", &ssn);
		TS_ASSERT(b.related("has", "legs", "4"))  // has _refers_to _has _context simple_english
		TS_ASSERT(b.related("has", "tail"));
		TS_ASSERT(b.related("can", "bite"));
		ssn.pop_context();

		// Universal context
		tagd::tag c;
		rc = tdb.get(c, "dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(rc), "TAGD_OK");
		TS_ASSERT(!c.related("has", "legs", "4"));
		TS_ASSERT(!c.related("has", "tail"));
		TS_ASSERT(c.related(HARD_TAG_HAS, "legs", "4"));
		TS_ASSERT(c.related(HARD_TAG_HAS, "tail"));
		TS_ASSERT(c.related("can", "bite"));

		tagd::tag penguin("penguin", "bird");
		TS_ASSERT_TAGD_OK(penguin.relation("can", "swim"))
		rc = tdb.put(penguin, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(rc), "TAGD_OK");

		rc = tdb.put(tagd::referent("flippers", "wings", "penguin"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(rc), "TAGD_OK");

		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));  // has
		TS_ASSERT_TAGD_OK(ssn.push_context("penguin"));  // flippers
		tagd::tag_set S;
		tagd::interrogator q(HARD_TAG_WHAT);
		TS_ASSERT_TAGD_OK(q.relation("can", "swim"))
		TS_ASSERT_TAGD_OK(q.relation("has", "flippers"))
		rc = tdb.query(S, q, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(rc), "TAGD_OK");
        TS_ASSERT(S.size() == 1);
		tagd::abstract_tag r;
		for (auto s : S) {
			r = s;
		}
        TS_ASSERT_EQUALS(r.id(), "penguin");
		TS_ASSERT(r.related("can", "swim"));
		// `has flippers` decoded to `_has wings` matches {bat, bird}
		// `can swim` matches {whale, penguin}
		// containership merge: {bat, bird} ∪ {whale, penguin(_is_a bird)} => {penguin}
		// since bird `_has wings` and {penguin} result, the following are not present:
		TS_ASSERT(!r.related("has", "wings"));
		TS_ASSERT(!r.related("has", "flippers"));

		tagd::tag t1;
		rc = tdb.get(t1, "bird", &ssn);  // context: simple_english, penguin
		TS_ASSERT_EQUALS(t1.id(), "bird");
		TS_ASSERT(t1.related("has", "flippers"));

		ssn.pop_context(); // context: simple_english
		tagd::tag t2;
		rc = tdb.get(t2, "bird", &ssn);
		TS_ASSERT_EQUALS(t2.id(), "bird");
		TS_ASSERT(t2.related("has", "wings"));

		ssn.pop_context(); // context: <empty>
		tagd::tag t3;
		rc = tdb.get(t3, "bird", &ssn);
		TS_ASSERT_EQUALS(t3.id(), "bird");
		TS_ASSERT(t3.related(HARD_TAG_HAS, "wings"));

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

	void test_context_stack(void) {
        TDB_CONS_INIT();

        TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("thing", "physical_object", "simple_english"), &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

        TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

        TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("thing", "concept", "knowledge"), &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		tagd::tag t1;
		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));
		TS_ASSERT_EQUALS(ssn.context().size() , 1);
		if (ssn.context().size() > 0)
			TS_ASSERT_EQUALS(ssn.context()[0] , "simple_english");

		TS_ASSERT_TAGD_OK(tdb.get(t1, "thing", &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(t1.id(), "thing");
        TS_ASSERT(t1.related(HARD_TAG_REFERS_TO, "physical_object"));

		TS_ASSERT_TAGD_OK(ssn.push_context("living_thing"));
		tagd::tag t2;
		TS_ASSERT_TAGD_OK(tdb.get(t2, "thing", &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(t2.id(), "thing");
        TS_ASSERT(t2.related(HARD_TAG_REFERS_TO, "animal"));

		TS_ASSERT_TAGD_OK(ssn.push_context("mind"));  // knowledge is_a mind
		tagd::tag t3;
		TS_ASSERT_TAGD_OK(tdb.get(t3, "thing", &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(t3.id(), "thing");
        TS_ASSERT(t3.related(HARD_TAG_REFERS_TO, "concept"));

		ssn.pop_context();
		tagd::tag t4;
		TS_ASSERT_TAGD_OK(tdb.get(t4, "thing", &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(t4.id(), "thing");
        TS_ASSERT(t4.related(HARD_TAG_REFERS_TO, "animal"));

		ssn.clear_context();
		tagd::tag t5;
		auto tc = tdb.get(t5, "thing", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_AMBIGUOUS");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_AMBIGUOUS");

        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
	}

    void test_utf8_japanese_referent(void) {
        TDB_CONS_INIT();

		tagd::tag japanese("japanese", "language");
		TS_ASSERT_TAGD_OK(tdb.put(japanese, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

        tagd::referent jp_dog("イヌ", "dog", "japanese");
        TS_ASSERT_TAGD_OK(tdb.put(jp_dog, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");

		tagd::tag d;
		auto tc = tdb.get(d, "イヌ", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_AMBIGUOUS");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_AMBIGUOUS");

		tagd::tag t;
		TS_ASSERT_TAGD_OK(ssn.push_context("japanese"));
		TS_ASSERT_TAGD_OK(tdb.get(t, "イヌ", &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(t.id(), "イヌ");
        TS_ASSERT(t.related(HARD_TAG_REFERS_TO, "dog"));
	}

	void test_delete_referents(void) {
        TDB_CONS_INIT();

		tagd::code tc;
        tc = tdb.put(tagd::referent("thing", "physical_object", "simple_english"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("creature", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// non existant referent
        tc = tdb.del(tagd::referent("snarf", "living_thing", "simple_english"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		tdb.clear_errors();

		// refers.empty() && refers_to.empty() && context.empty()
        tc = tdb.del(tagd::referent(), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
		tdb.clear_errors();

		// refers.empty() && refers_to.empty() && !context.empty()
        tc = tdb.del(tagd::referent("", "", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        tagd::interrogator q_context_living_thing(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_context_living_thing.relation(HARD_TAG_CONTEXT, "living_thing"))
		tagd::tag_set S;
        tc = tdb.query(S, q_context_living_thing, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT(S.size() == 0);

		auto f_q_thing_physical_object_exists = [&]() {
			tagd::interrogator q_thing(HARD_TAG_WHAT, HARD_TAG_REFERENT);
			TS_ASSERT_TAGD_OK(q_thing.relation(HARD_TAG_REFERS, "thing"))
			S.clear();
			tc = tdb.query(S, q_thing, &ssn);
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
			TS_ASSERT(S.size() == 1);
			tagd::referent r_thing;
			for (auto s : S) {
				r_thing = s;
			}
			TS_ASSERT_EQUALS(r_thing.id() , "thing");
			TS_ASSERT_EQUALS(r_thing.sub_relator() , HARD_TAG_REFERS_TO);
			TS_ASSERT_EQUALS(r_thing.super_object() , "physical_object");
			TS_ASSERT_EQUALS(r_thing.context() , "simple_english");
		};

		f_q_thing_physical_object_exists();

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("creature", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// refers.empty() && !refers_to.empty() && context.empty()
		tc = tdb.del(tagd::referent("", "animal", ""), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// already deleted - does not exist
		tc = tdb.del(tagd::referent("", "animal", ""), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
		
		S.clear();
        tc = tdb.query(S, q_context_living_thing, &ssn); // _context living_thing
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("creature", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// refers.empty() && !refers_to.empty() && !context.empty()
		tc = tdb.del(tagd::referent("", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::interrogator q_refers_to_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_to_context.relation(HARD_TAG_REFERS_TO, "animal"))
        TS_ASSERT_TAGD_OK(q_refers_to_context.relation(HARD_TAG_CONTEXT, "living_thing"))
        S.clear();
        tc = tdb.query(S, q_refers_to_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("creature", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// !refers.empty() && refers_to.empty() && context.empty()
		tc = tdb.del(tagd::referent("thing", "", ""), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::interrogator q_refers(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers.relation(HARD_TAG_REFERS, "thing"))
        S.clear();
        tc = tdb.query(S, q_refers, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

		// make sure the other referent to animal still exists
		TS_ASSERT_TAGD_OK(ssn.push_context("living_thing"));
		tagd::abstract_tag creature;
		tc = tdb.get(creature, "animal", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(creature.id(), "creature");
        TS_ASSERT(creature.related(HARD_TAG_REFERS_TO, "animal"));
		ssn.clear_context();

        tc = tdb.put(tagd::referent("thing", "physical_object", "simple_english"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// !refers.empty() && refers_to.empty() && !context.empty()
		tc = tdb.del(tagd::referent("thing", "", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::interrogator q_refers_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_context.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_context.relation(HARD_TAG_CONTEXT, "living_thing"))
        S.clear();
        tc = tdb.query(S, q_refers_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

		// make sure the other referent to animal still exists
		S.clear();	
        tc = tdb.query(S, q_context_living_thing, &ssn);
		tagd::id_string what;
		for (auto s : S)
			what = s.id();
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(what, "creature");
		ssn.clear_context();

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// !refers.empty() && !refers_to.empty() && context.empty()
		tagd::referent ref_empty_ctx("thing", "animal", "");
		tc = tdb.del(ref_empty_ctx, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::interrogator q_refers_refers_to(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_refers_to.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to.relation(HARD_TAG_REFERS_TO, "animal"))
        S.clear();
        tc = tdb.query(S, q_refers_refers_to, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

        tc = tdb.put(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// !refers.empty() && !refers_to.empty() && !context.empty()
		tc = tdb.del(tagd::referent("thing", "animal", "living_thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::interrogator q_refers_refers_to_context(HARD_TAG_WHAT, HARD_TAG_REFERENT);
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_REFERS, "thing"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_REFERS_TO, "animal"))
        TS_ASSERT_TAGD_OK(q_refers_refers_to_context.relation(HARD_TAG_CONTEXT, "living_thing"))
        S.clear();
        tc = tdb.query(S, q_refers_refers_to_context, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");

        f_q_thing_physical_object_exists();

		//// delete referent no context
		// no cons for empty context
		// 	tc = tdb.del(tagd::referent("thing"), &ssn);
		// using POS_TAG object
		tc = tdb.del(tagd::tag("thing"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_AMBIGUOUS");

		// test referent gets wiped out
		tc = tdb.put(tagd::referent("thingy", "physical_object", "simple_english"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tc = tdb.del(tagd::referent("thingy", "physical_object", "simple_english"), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::abstract_tag t;
		tc = tdb.get(t, "thingy", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
	}

    void test_exists(void) {
        TDB_CONS_INIT();

        TS_ASSERT(tdb.exists("dog"));

        TS_ASSERT(!tdb.exists("unicorn"));
    }

    void test_pos(void) {
        TDB_CONS_INIT();

		tagd::part_of_speech pos = tdb.pos("dog", &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_TAG");

		pos = tdb.pos(HARD_TAG_HAS, &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_RELATOR");

		pos = tdb.pos(HARD_TAG_IS_A, &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_SUB_RELATOR");

		pos = tdb.pos("has", &ssn);  // referent relator, no context
        TS_ASSERT_EQUALS(pos_str(pos), "POS_UNKNOWN");

		pos = tdb.pos("is_a", &ssn);  // referent relator, no context
        TS_ASSERT_EQUALS(pos_str(pos), "POS_UNKNOWN");

		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));

		pos = tdb.pos("has", &ssn);  // referent relator
        TS_ASSERT_EQUALS(pos_str(pos), "POS_RELATOR");

		pos = tdb.pos("is_a", &ssn);  // referent relator
        TS_ASSERT_EQUALS(pos_str(pos), "POS_SUB_RELATOR");

		ssn.clear_context();

        tagd::referent thing("thing", "physical_object", "simple_english");
        TS_ASSERT_TAGD_OK(tdb.put(thing, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		pos = tdb.pos("thing", &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_UNKNOWN");

		TS_ASSERT_TAGD_OK(ssn.push_context("simple_english"));
		pos = tdb.pos("thing", &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_TAG");
		ssn.clear_context();

		pos = tdb.pos("unicorn", &ssn);
        TS_ASSERT_EQUALS(pos_str(pos), "POS_UNKNOWN");
    }

    void test_insert_relations(void) {
        TDB_CONS_INIT();

        tagd::tag a("dog", "mammal");  // existing
        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "teeth"))
        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "tail"))
        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "legs", "4"))
        tagd::code tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b("robin", "bird");  // non-existing
        TS_ASSERT_TAGD_OK(b.relation(HARD_TAG_HAS, "beak"))
        TS_ASSERT_TAGD_OK(b.relation(HARD_TAG_HAS, "wings"))
        TS_ASSERT_TAGD_OK(b.relation(HARD_TAG_HAS, "feathers"))
        tc = tdb.put(b, &ssn);
        TS_ASSERT_EQUALS(tc, tagd::TAGD_OK);

		TS_ASSERT_TAGD_OK(tdb.put(tagd::tag("bite", "action"), &ssn));
        // already existing, only put relations
        tagd::tag c("dog");
        TS_ASSERT_TAGD_OK(c.relation("can", "bite"))
        tc = tdb.put(c, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::tag d;
		tc = tdb.get(d, "dog", &ssn);
		TS_ASSERT(d.related(HARD_TAG_HAS, "legs", "4"));
		TS_ASSERT(d.related(HARD_TAG_HAS, "tail"));
		TS_ASSERT(d.related("can", "bite"));
    }

    void test_duplicate(void) {
        TDB_CONS_INIT();

        tagd::tag dog("dog", "mammal");
        tagd::code tc = tdb.put(dog, &ssn); // duplicate tag, no relations
        TS_ASSERT_EQUALS(tc, tagd::TS_DUPLICATE);

        TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "fur"))
        tc = tdb.put(dog, &ssn); // not duplicate, relation was inserted
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(dog, &ssn); // tag duplicate, relations duplicate
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_DUPLICATE");

		TS_ASSERT_TAGD_OK(tdb.put(tagd::tag("bite", "action"), &ssn));
        TS_ASSERT_TAGD_OK(dog.relation("can", "bite"))
        tc = tdb.put(dog, &ssn); // one duplicate (has tail), one insert (can bite)
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		TS_ASSERT_TAGD_OK(tdb.put(tagd::relator("wags", "movement"), &ssn));
		TS_ASSERT_TAGD_OK(dog.relation("wags", "tail"))
        tc = tdb.put(dog, &ssn); // relator makes unique

        tagd::tag a("dog");
        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "tail"))
        tc = tdb.put(a, &ssn);  // existing tag, empty sub, existing relation
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_DUPLICATE");

        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "teeth"))
        tc = tdb.put(a, &ssn);  // existing tag, empty sub, one new relation
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
    }

	void test_ignore_duplicate_rank(void) {
        TDB_CONS_INIT();

		tagd::tag dolphin("dolphin", "mammal");
        tagd::code tc = tdb.put(dolphin, &ssn, tagdb::F_IGNORE_DUPLICATES); // new tag, no relations
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::tag t1;
		tc = tdb.get(t1, "dolphin", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		tagd::rank r1 = t1.rank();

        tc = tdb.put(dolphin, &ssn, tagdb::F_IGNORE_DUPLICATES); // duplicate not inserted
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::tag t2;
		tc = tdb.get(t2, "dolphin", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		tagd::rank r2 = t2.rank();

        TS_ASSERT_EQUALS(r1.dotted_str(), r2.dotted_str());
    }

    void test_ignore_duplicate(void) {
        TDB_CONS_INIT();

        tagd::tag dog("dog", "mammal");
        tagd::code tc = tdb.put(dog, &ssn, tagdb::F_IGNORE_DUPLICATES); // duplicate tag, no relations
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "fur"))
        tc = tdb.put(dog, &ssn, tagdb::F_IGNORE_DUPLICATES); // not duplicate, relation was inserted
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tc = tdb.put(dog, &ssn, tagdb::F_IGNORE_DUPLICATES); // tag duplicate, relations duplicate
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		TS_ASSERT_TAGD_OK(tdb.put(tagd::tag("bite", "action"), &ssn));
        TS_ASSERT_TAGD_OK(dog.relation("can", "bite"))
        tc = tdb.put(dog, &ssn, tagdb::F_IGNORE_DUPLICATES); // one duplicate (has tail), one insert (can bite)
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		TS_ASSERT_TAGD_OK(tdb.put(tagd::relator("wags", "movement"), &ssn));
		TS_ASSERT_TAGD_OK(dog.relation("wags", "tail"))
        tc = tdb.put(dog, &ssn, tagdb::F_IGNORE_DUPLICATES); // relator makes unique

        tagd::tag a("dog");
        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "tail"))
        tc = tdb.put(a, &ssn, tagdb::F_IGNORE_DUPLICATES);  // existing tag, empty sub, existing relation
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        TS_ASSERT_TAGD_OK(a.relation(HARD_TAG_HAS, "teeth"))
        tc = tdb.put(a, &ssn, tagdb::F_IGNORE_DUPLICATES);  // existing tag, empty sub, one new relation
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
    }

    void test_move(void) {
        TDB_CONS_INIT();

        tagd::tag a("sea_creature", "living_thing");
        tagd::code tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b("fish", "insect");
        tc = tdb.put(b, &ssn);  // oops
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		b = tagd::tag("fish", "sea_creature");
	        tc = tdb.put(b, &ssn);  // move
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		size_t sz = a.rank().dotted_str().size();
		// a rank should prefix b rank
        TS_ASSERT_EQUALS(a.rank().dotted_str(), b.rank().dotted_str().substr(0, sz));
    }

    void test_move_entity(void) {
        tagdb_type tdb;
        TS_ASSERT_TAGD_OK(tdb.init(db_fname));
		tagdb::session ssn = tdb.get_session();

        tagd::tag a("animal", HARD_TAG_ENTITY);
        tagd::code tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag b("dog", HARD_TAG_ENTITY);
        tc = tdb.put(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::tag c("dog", "animal");
        tc = tdb.put(c, &ssn);  // move
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		size_t sz = a.rank().dotted_str().size();
		// a rank should prefix c rank
        TS_ASSERT_EQUALS(a.rank().dotted_str(), c.rank().dotted_str().substr(0, sz));
    }

    void test_undef_tag_refs(void) {
        TDB_CONS_INIT();

        tagd::tag dog("dog", "nosuchthing");
        tagd::code tc = tdb.put(dog, &ssn);
        TS_ASSERT_EQUALS(tc, tagd::TS_SUB_UNK);

	        dog = tagd::tag("dog", "mammal");
	        TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "tailandwings"))
        tc = tdb.put(dog, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_OBJECT_UNK");

        dog.relations.clear();
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(dog.relation("flobdapperbates", "tail")), "TAGD_OK");
        tc = tdb.put(dog, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_RELATOR_UNK");

        tagd::tag a("dog");  // existing no sub, no relations
        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
    }

    void test_related(void) {
        TDB_CONS_INIT();

		tagd::code tc;
		tagd::tag t;

        tagd::tag_set S;
        tc = tdb_related(tdb, S, tagd::predicate(HARD_TAG_HAS, "legs")); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 3);

        S.clear();
        tc = tdb_related(tdb, S, tagd::predicate(HARD_TAG_HAS, "legs", "3", tagd::OP_GT, tagd::TYPE_INTEGER)); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 3);

        S.clear();
        tc = tdb_related(tdb, S, tagd::predicate(HARD_TAG_HAS, "legs", "30", tagd::OP_GT, tagd::TYPE_INTEGER), &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
        tc = tdb_related(tdb, S, tagd::predicate(HARD_TAG_RELATOR, "body_part")); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 8);

        S.clear();
        tc = tdb_related(tdb, S, tagd::predicate("snarfs", "cockamamy")); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
        tc = tdb_related(tdb, S, tagd::predicate("can", "")); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 5);
    }

    void test_query(void) {
        TDB_CONS_INIT();

        // TODO "_what" is not actually used in the query
        // this will change over time, as different types
        // of interrogators will denote different queries
        tagd::interrogator q(HARD_TAG_WHAT, "mammal");
        TS_ASSERT_TAGD_OK(q.relation("can", "swim"))
        tagd::tag_set S;
        tagd::code tc = tdb.query(S, q, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "whale"));

        S.clear();
        tagd::interrogator q_mammal_children(HARD_TAG_WHAT, "mammal");
        tc = tdb.query(S, q_mammal_children, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT(ssn.ok())
        TS_ASSERT_EQUALS(S.size(), 4);
        TS_ASSERT(tag_set_exists(S, "dog"));
        TS_ASSERT(tag_set_exists(S, "cat"));
        TS_ASSERT(tag_set_exists(S, "whale"));
        TS_ASSERT(tag_set_exists(S, "bat"));

        S.clear();
        tagd::interrogator q_dog_no_children(HARD_TAG_WHAT, "dog");
        tc = tdb.query(S, q_dog_no_children, &ssn, tagdb::F_NO_NOT_FOUND_ERROR);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT(ssn.ok())
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
        tc = tdb.query(S, q_dog_no_children, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_NOT_FOUND");
        TS_ASSERT(!ssn.ok())
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
        tagd::interrogator r(HARD_TAG_WHAT);
        TS_ASSERT_TAGD_OK(r.relation(HARD_TAG_HAS, "fangs"))
        tc = tdb.query(S, r, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, "spider"));
        TS_ASSERT(tag_set_exists(S, "snake"));

        S.clear();
        tagd::interrogator s(HARD_TAG_WHAT);
        TS_ASSERT_TAGD_OK(s.relation(HARD_TAG_HAS, "teeth"))
        tc = tdb.query(S, s, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 3);
        TS_ASSERT(tag_set_exists(S, "mammal"));
        TS_ASSERT(tag_set_exists(S, "spider"));  // fangs are teeth
        TS_ASSERT(tag_set_exists(S, "snake"));

        S.clear();
        tagd::interrogator q_not_found(HARD_TAG_WHAT);
        TS_ASSERT_TAGD_OK(q_not_found.relation(HARD_TAG_HAS, "dentures"))
        tc = tdb.query(S, q_not_found, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
		ssn.clear_errors();
        tc = tdb.query(S, q_not_found, &ssn, tagdb::F_NO_NOT_FOUND_ERROR);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 0);

        S.clear();
        tagd::interrogator q1(HARD_TAG_WHAT);
        TS_ASSERT_TAGD_OK(q1.relation(HARD_TAG_HAS, "legs", "2", tagd::OP_GT))
        TS_ASSERT_TAGD_OK(q1.relation(HARD_TAG_HAS, "body_part", "8", tagd::OP_LT_EQ))
        tc = tdb.query(S, q1, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 3);
        TS_ASSERT(tag_set_exists(S, "dog"));
        TS_ASSERT(tag_set_exists(S, "cat"));
        TS_ASSERT(tag_set_exists(S, "spider"));

		int num = 0;
        for(tagd::tag_set::iterator it = S.begin(); it != S.end(); ++it) {
            if (it->id() == "dog") {
				if (it->related(HARD_TAG_HAS, "legs", "4")) num++;
			}
			else if (it->id() == "cat") {
				if (it->related(HARD_TAG_HAS, "legs", "4")) num++;
			}
			else if (it->id() == "spider") {
				if (it->related(HARD_TAG_HAS, "legs", "8")) num++;
			}
        }
		TS_ASSERT_EQUALS(num , 3);

		S.clear();
        tagd::interrogator q2(HARD_TAG_WHAT, "animal");
		TS_ASSERT_TAGD_OK(q2.relation(HARD_TAG_HAS, ""))
        TS_ASSERT_TAGD_OK(q2.relation("can", ""))
        tc = tdb.query(S, q2, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 5);
        TS_ASSERT(tag_set_exists(S, "dog"));
        TS_ASSERT(tag_set_exists(S, "cat"));
        TS_ASSERT(tag_set_exists(S, "whale"));
        TS_ASSERT(tag_set_exists(S, "bat"));
        TS_ASSERT(tag_set_exists(S, "bird"));

		// TODO tagd::merge_containing tags fails to merge predicates
    }

    void test_query_super_object(void) {
        TDB_CONS_INIT();

        // TODO "_what" is not actually used in the query
        // this will change over time, as different types
        // of interrogators will denote different queries
        tagd::interrogator q(HARD_TAG_WHAT, "mammal");

        tagd::tag_set S;
        tagd::code tc = tdb.query(S, q, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 4);
        TS_ASSERT(tag_set_exists(S, "dog"));
        TS_ASSERT(tag_set_exists(S, "cat"));
        TS_ASSERT(tag_set_exists(S, "whale"));
        TS_ASSERT(tag_set_exists(S, "bat"));
    }

	void test_query_parent_relation(void) {
        TDB_CONS_INIT();

        tagd::interrogator q(HARD_TAG_WHAT);
        TS_ASSERT_TAGD_OK(q.relation(HARD_TAG_HAS, "blood", "warm"))
        TS_ASSERT_TAGD_OK(q.relation("can", "swim"))
        tagd::tag_set S;
        tagd::code tc = tdb.query(S, q, &ssn);
		if (!tdb.ok()) {
			tdb.print_errors();
		}
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "whale"));
	}

	void test_search(void) {
        TDB_CONS_INIT();

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

	void test_query_search(void) {
        TDB_CONS_INIT();

        tagd::interrogator q(HARD_TAG_WHAT, "mammal");
		TS_ASSERT_TAGD_OK(q.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "can swim"))
        tagd::tag_set S;
        tagd::code tc = tdb.query(S, q, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "whale"));

        S.clear();
        tagd::interrogator r(HARD_TAG_WHAT);
		TS_ASSERT_TAGD_OK(r.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "has fangs"))
        tc = tdb.query(S, r, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, "spider"));
        TS_ASSERT(tag_set_exists(S, "snake"));

        S.clear();
        tagd::interrogator s(HARD_TAG_WHAT);
		TS_ASSERT_TAGD_OK(s.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "has teeth"))
        tc = tdb.query(S, s, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "mammal"));
		// fangs are teeth, but not a FTS match
        TS_ASSERT(!tag_set_exists(S, "spider"));
        TS_ASSERT(!tag_set_exists(S, "snake"));

		S.clear();
        tagd::interrogator q2(HARD_TAG_WHAT, "animal");
		TS_ASSERT_TAGD_OK(q2.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "has can"))
        tc = tdb.query(S, q2, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 5);
        TS_ASSERT(tag_set_exists(S, "dog"));
        TS_ASSERT(tag_set_exists(S, "cat"));
        TS_ASSERT(tag_set_exists(S, "whale"));
        TS_ASSERT(tag_set_exists(S, "bat"));
        TS_ASSERT(tag_set_exists(S, "bird"));

		S.clear();
		tagd::interrogator s1(HARD_TAG_SEARCH);
		TS_ASSERT_TAGD_OK(s1.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "warm blood"))
		TS_ASSERT_TAGD_OK(s1.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark"))
        tc = tdb.query(S, s1, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "dog"));

		// test update search terms
		TS_ASSERT_TAGD_OK(tdb.put(tagd::tag("bite", "action"), &ssn));
        tagd::tag t("dog");
        TS_ASSERT_TAGD_OK(t.relation("can", "bite"))
        TS_ASSERT_TAGD_OK(tdb.put(t, &ssn));
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		S.clear();
		tagd::interrogator s2(HARD_TAG_SEARCH);
		TS_ASSERT_TAGD_OK(s2.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "can bite"))
        tc = tdb.query(S, s2, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 1);
        TS_ASSERT(tag_set_exists(S, "dog"));
    }

    void test_get_hard_tag(void) {
		TDB_CONS_INIT();

        tagd::tag t;
        tagd::code tc = tdb.get(t, HARD_TAG_URL, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(t.id(), HARD_TAG_URL);
	}

    void test_put_hard_tag(void) {
		TDB_CONS_INIT();

        tagd::tag t("_haha", HARD_TAG_ENTITY);
        tagd::code tc = tdb.put(t, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_MISUSE");
	}

    void test_put_get_tag_sub_relator(void) {
		TDB_CONS_INIT();

		tagd::tag t1("husky", HARD_TAG_TYPE_OF, "dog");
		TS_ASSERT_TAGD_OK(tdb.put(t1, &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");

		tagd::abstract_tag t2;
		TS_ASSERT_TAGD_OK(tdb.get(t2, "husky", &ssn));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tdb.code()), "TAGD_OK");
		TS_ASSERT_EQUALS(t2.sub_relator() , HARD_TAG_TYPE_OF);
	}

	void test_sqlite_round_trip_preserves_identity_and_referent_transforms(void) {
		TDB_CONS_INIT();

		tagd::tag husky("husky", HARD_TAG_TYPE_OF, "dog");
		TS_ASSERT_TAGD_OK(husky.relation(HARD_TAG_HAS, "tail"));
		TS_ASSERT_TAGD_OK(husky.relation("can", "bark"));
		TS_ASSERT_TAGD_OK(tdb.put(husky, &ssn));

		tagd::abstract_tag populated_husky;
		TS_ASSERT_TAGD_OK(tdb.get(populated_husky, "husky", &ssn));
		TS_ASSERT_EQUALS(populated_husky.id(), "husky");
		TS_ASSERT_EQUALS(populated_husky.sub_relator(), HARD_TAG_TYPE_OF);
		TS_ASSERT_EQUALS(populated_husky.super_object(), "dog");
		TS_ASSERT(!populated_husky.rank().empty());
		TS_ASSERT(populated_husky.related(HARD_TAG_HAS, "tail"));
		TS_ASSERT(populated_husky.related("can", "bark"));

		tagd::tag collie("collie", "dog");
		TS_ASSERT_TAGD_OK(tdb.put(collie, &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("puppy", "collie", "simple_english"), &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("fluff", "tail", "simple_english"), &ssn));

		tagdb::session english_ssn = tdb.get_session();
		TS_ASSERT_TAGD_OK(english_ssn.push_context("simple_english"));

		tagd::tag puppy("puppy", "is_a", "dog");
		TS_ASSERT_TAGD_OK(puppy.relation("has", "fluff"));
		TS_ASSERT_TAGD_OK(puppy.relation("can", "bark"));
		TS_ASSERT_TAGD_OK(tdb.put(puppy, &english_ssn));

		tagd::abstract_tag canonical_collie;
		TS_ASSERT_TAGD_OK(tdb.get(canonical_collie, "collie", &ssn, tagdb::F_NO_TRANSFORM_REFERENTS));
		TS_ASSERT_EQUALS(canonical_collie.id(), "collie");
		TS_ASSERT_EQUALS(canonical_collie.sub_relator(), HARD_TAG_IS_A);
		TS_ASSERT_EQUALS(canonical_collie.super_object(), "dog");
		TS_ASSERT(!canonical_collie.rank().empty());
		TS_ASSERT(canonical_collie.related(HARD_TAG_HAS, "tail"));
		TS_ASSERT(canonical_collie.related("can", "bark"));

		tagd::abstract_tag populated_puppy;
		TS_ASSERT_TAGD_OK(tdb.get(populated_puppy, "puppy", &english_ssn));
		TS_ASSERT_EQUALS(populated_puppy.id(), "puppy");
		TS_ASSERT_EQUALS(populated_puppy.sub_relator(), "is_a");
		TS_ASSERT_EQUALS(populated_puppy.super_object(), "dog");
		TS_ASSERT_EQUALS(populated_puppy.rank().dotted_str(), canonical_collie.rank().dotted_str());
		TS_ASSERT(populated_puppy.related("has", "fluff"));
		TS_ASSERT(populated_puppy.related("can", "bark"));
		TS_ASSERT(populated_puppy.related(HARD_TAG_REFERS_TO, "collie"));
	}

	void test_related_query_preserves_ranked_transformed_hydration(void) {
		TDB_CONS_INIT();

		tagd::tag collie("collie", "dog");
		TS_ASSERT_TAGD_OK(collie.relation(HARD_TAG_HAS, "tail"));
		TS_ASSERT_TAGD_OK(collie.relation("can", "bark"));
		TS_ASSERT_TAGD_OK(tdb.put(collie, &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("puppy", "collie", "simple_english"), &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("fluff", "tail", "simple_english"), &ssn));

		tagd::abstract_tag canonical_collie;
		TS_ASSERT_TAGD_OK(tdb.get(canonical_collie, "collie", &ssn, tagdb::F_NO_TRANSFORM_REFERENTS));

		tagdb::session english_ssn = tdb.get_session();
		TS_ASSERT_TAGD_OK(english_ssn.push_context("simple_english"));

		tagd::tag_set related_tags;
		tagd::interrogator q(HARD_TAG_WHAT, "puppy");
		TS_ASSERT_TAGD_OK(q.relation("has", "fluff"));
		TS_ASSERT_TAGD_OK(tdb.query(related_tags, q, &english_ssn));
		TS_ASSERT_EQUALS(related_tags.size(), 1);

		auto it = related_tags.begin();
		TS_ASSERT_DIFFERS(it, related_tags.end());
		if (it != related_tags.end()) {
			TS_ASSERT_EQUALS(it->id(), "puppy");
			TS_ASSERT_EQUALS(it->sub_relator(), "is_a");
			TS_ASSERT_EQUALS(it->super_object(), "dog");
			TS_ASSERT_EQUALS(it->rank().dotted_str(), canonical_collie.rank().dotted_str());
			TS_ASSERT(it->related("has", "fluff"));
		}
	}

	void test_children_query_preserves_ranked_transformed_hydration(void) {
		TDB_CONS_INIT();

		tagd::tag collie("collie", "dog");
		TS_ASSERT_TAGD_OK(tdb.put(collie, &ssn));
		tagd::tag rough_collie("rough_collie", "collie");
		TS_ASSERT_TAGD_OK(rough_collie.relation(HARD_TAG_HAS, "tail"));
		TS_ASSERT_TAGD_OK(tdb.put(rough_collie, &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("puppy", "collie", "simple_english"), &ssn));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("lassie", "rough_collie", "simple_english"), &ssn));

		tagd::abstract_tag canonical_child;
		TS_ASSERT_TAGD_OK(tdb.get(canonical_child, "rough_collie", &ssn, tagdb::F_NO_TRANSFORM_REFERENTS));

		tagdb::session english_ssn = tdb.get_session();
		TS_ASSERT_TAGD_OK(english_ssn.push_context("simple_english"));

		tagd::tag_set children;
		tagd::interrogator q(HARD_TAG_WHAT, "puppy");
		TS_ASSERT_TAGD_OK(tdb.query(children, q, &english_ssn));
		TS_ASSERT_EQUALS(children.size(), 1);

		auto it = children.begin();
		TS_ASSERT_DIFFERS(it, children.end());
		if (it != children.end()) {
			TS_ASSERT_EQUALS(it->id(), "lassie");
			TS_ASSERT_EQUALS(it->sub_relator(), "is_a");
			TS_ASSERT_EQUALS(it->super_object(), "puppy");
			TS_ASSERT_EQUALS(it->rank().dotted_str(), canonical_child.rank().dotted_str());
		}
	}

    void test_relations(void) {
		TDB_CONS_INIT();

        tagd::tag dog;
        TS_ASSERT_TAGD_OK(tdb.get(dog, "dog", &ssn));
        TS_ASSERT_EQUALS(dog.relations.size(), 3);
		TS_ASSERT(dog.related(HARD_TAG_HAS, "legs", "4"));
		TS_ASSERT(dog.related(HARD_TAG_HAS, "tail"));

        tagd::predicate_set how;
		TS_ASSERT_EQUALS(dog.related("bark", how), 1);
		auto it = how.begin();
		TS_ASSERT(it != how.end());
		if (it != how.end()) {
			TS_ASSERT_EQUALS(it->relator, "can");
			TS_ASSERT_EQUALS(it->object, "bark");
		}

		how.clear();
		TS_ASSERT_EQUALS(dog.related("legs", how), 1);
		it = how.begin();
		TS_ASSERT(it != how.end());
		if (it != how.end()) {
			TS_ASSERT_EQUALS(it->relator, HARD_TAG_HAS);
			TS_ASSERT_EQUALS(it->object, "legs");
			TS_ASSERT_EQUALS(it->modifier, "4");
		}

		TS_ASSERT(!dog.related(HARD_TAG_HAS, "fins"));
	}

/// URI TESTS ///

    void test_put_url(void) {
        tagdb_type tdb;
        tagd::code tc = tdb.init(db_fname);
		auto ssn = tdb.get_session();
        populate_tags(tdb);

        tagd::url a("http://www.hypermega.com/a/b/c?x=1&y=2#here");
        TS_ASSERT_TAGD_OK(a.relation("about", "computer_security"))
		TS_ASSERT_EQUALS(a.id(), "http://www.hypermega.com/a/b/c?x=1&y=2#here"); 
        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(a.id(), "http://www.hypermega.com/a/b/c?x=1&y=2#here"); 

        tagd::url b;
        tc = tdb.get(b, a.id(), &ssn); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(b.id(), "http://www.hypermega.com/a/b/c?x=1&y=2#here"); 
		TS_ASSERT_EQUALS(b.hduri(), "hd:com!hypermega!www!/a/b/c!?x=1&y=2!#here!!!!http");
        TS_ASSERT(b.relations.size() > 1);
        TS_ASSERT(b.related("about", "computer_security"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_HOST, "www.hypermega.com"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "hypermega"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "com"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, "www"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PATH, "/a/b/c"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_QUERY, "?x=1&y=2"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT, "#here"));
    }

    void test_get_hduri(void) {
        tagdb_type tdb;
        tagd::code tc = tdb.init(db_fname);
		auto ssn = tdb.get_session();
        populate_tags(tdb);

		const std::string url_str{"http://www.hypermega.com/a/b/c?x=1&y=2#here"};
		const std::string hduri{"hd:com!hypermega!www!/a/b/c!?x=1&y=2!#here!!!!http"};
        tagd::url a(url_str);
        TS_ASSERT_TAGD_OK(a.relation("about", "computer_security"))
        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(a.hduri(), hduri);

        tagd::abstract_tag b;
        tc = tdb.get(b, hduri, &ssn); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(b.id(), url_str);  // hduri upgraded to url str
        TS_ASSERT_EQUALS(b.super_object() , HARD_TAG_URL);
        TS_ASSERT(b.related("about", "computer_security"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_HOST, "www.hypermega.com"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "hypermega"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "com"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, "www"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_PATH, "/a/b/c"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_QUERY, "?x=1&y=2"));
        TS_ASSERT(b.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT, "#here"));

        tagd::url c;
        tc = tdb.get(c, url_str, &ssn); 
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS(c.id(), url_str);
		TS_ASSERT_EQUALS(c.hduri(), hduri);
        TS_ASSERT_EQUALS(c.super_object() , HARD_TAG_URL);
        TS_ASSERT(c.related("about", "computer_security"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_HOST, "www.hypermega.com"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "hypermega"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "com"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, "www"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_PATH, "/a/b/c"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_QUERY, "?x=1&y=2"));
        TS_ASSERT(c.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT, "#here"));
    }

    void test_query_url(void) {
        TDB_CONS_INIT();

		tagd::code tc;
        tagd::url a("http://en.wikipedia.org/wiki/Dog");
        TS_ASSERT_TAGD_OK(a.relation("about", "dog"))
        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::tag starwars("starwars", "movie");
		TS_ASSERT_TAGD_OK(tdb.put(starwars, &ssn));
        tagd::url b("http://starwars.wikia.com/wiki/Dog");
        TS_ASSERT_TAGD_OK(b.relation("about", "starwars"))
        tc = tdb.put(b, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        tagd::url c("http://animal.discovery.com/tv-shows/dogs-101");
        TS_ASSERT_TAGD_OK(c.relation("about", "dog"))
        tc = tdb.put(c, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

        // TODO "_what" is not actually used in the query
        tagd::interrogator q(HARD_TAG_WHAT, HARD_TAG_URL);
        TS_ASSERT_TAGD_OK(q.relation("about", "dog"))
        tagd::tag_set S;
        tc = tdb.query(S, q, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, "http://en.wikipedia.org/wiki/Dog"));
        TS_ASSERT(tag_set_exists(S, "http://animal.discovery.com/tv-shows/dogs-101"));

        S.clear();
        tagd::interrogator r(HARD_TAG_WHAT, HARD_TAG_URL);
        TS_ASSERT_TAGD_OK(r.relation(HARD_TAG_HAS, HARD_TAG_PATH, "/wiki/Dog"))
        tc = tdb.query(S, r, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 2);
        TS_ASSERT(tag_set_exists(S, "http://en.wikipedia.org/wiki/Dog"));
        TS_ASSERT(tag_set_exists(S, "http://starwars.wikia.com/wiki/Dog"));

        // _what is 'about' something
        tagd::interrogator what_about(HARD_TAG_INTERROGATOR);
        TS_ASSERT_TAGD_OK(what_about.relation(tagd::predicate("about", "")))
        S.clear();
        tc = tdb.query(S, what_about, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        TS_ASSERT_EQUALS(S.size(), 3);
        TS_ASSERT(tag_set_exists(S, "http://en.wikipedia.org/wiki/Dog"));
        TS_ASSERT(tag_set_exists(S, "http://starwars.wikia.com/wiki/Dog"));
        TS_ASSERT(tag_set_exists(S, "http://animal.discovery.com/tv-shows/dogs-101"));
    }

	void test_delete_url(void) {
        TDB_CONS_INIT();
		tagd::code tc;
        tagd::url a("http://en.wikipedia.org/wiki/Dog");
        TS_ASSERT_TAGD_OK(a.relation("about", "dog"))
        tc = tdb.put(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		// delete relation
		a.not_relation("about", "dog");
		TS_ASSERT(!a.relation("about", "dog"));
		tc = tdb.del(a, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::url b;
        tc = tdb.get(b, "http://en.wikipedia.org/wiki/Dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT(!b.related("about", "dog"));

		// delete url	
        tagd::url c("http://en.wikipedia.org/wiki/Dog");
		tc = tdb.del(c, &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");

		tagd::url d;
        tc = tdb.get(d, "http://en.wikipedia.org/wiki/Dog", &ssn);
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TS_NOT_FOUND");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ssn.code()), "TS_NOT_FOUND");
    }

    void test_util(void) {
		std::string db_file = tagdb::util::user_db();
		TS_ASSERT(!db_file.empty());
	}

	// TODO test tagdb::dump() and tagdb::dump_terms()
};
