#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "tagl.h"
#include "tagdb.h"
#include "httagd.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_type, tagd::abstract_tag> tag_map;
typedef tagdb::flags_t tdb_flags_t;
typedef tagdb::session tdb_sess_t;

// pure virtual interface
class tagdb_tester : public tagdb::tagdb {
	private:
		tag_map db;

		void put_test_tag(
			const tagd::id_type& id,
			const tagd::id_type& sub,
			const tagd::part_of_speech& pos)
		{
			tagd::abstract_tag t(id, sub, pos);
			db[t.id()] = t;
		}

		// animals
		tagd::abstract_tag _dog;
		tagd::abstract_tag _cat;

	public:
		tagdb_tester();
		tagd::part_of_speech pos(const tagd::id_type&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		tagd::code get(tagd::abstract_tag&, const tagd::id_type&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		tagd::code put(const tagd::abstract_tag&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		tagd::code del(const tagd::abstract_tag&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		bool exists(const tagd::id_type&, tdb_flags_t=tdb_flags_t());
		tagd::code query(tagd::tag_set&, const tagd::interrogator&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());

		tagd::code dump(std::ostream& os = std::cout) {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}

		tagd::code dump_grid(std::ostream& os = std::cout) {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}

		tagd::code dump_terms(std::ostream& os = std::cout) {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}
};

tagdb_tester::tagdb_tester() {
	put_test_tag(HARD_TAG_ENTITY, HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("living_thing", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("animal", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("legs", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("tail", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("fur", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("bark", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("meow", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("bite", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("swim", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("information", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("internet_security", "information", tagd::POS_TAG);
	put_test_tag("child", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("action", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("fun", "action", tagd::POS_TAG);
	put_test_tag("_is_a", HARD_TAG_ENTITY, tagd::POS_SUB_RELATOR);
	put_test_tag("about", HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag("_has", HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag("_can", HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag("_what", HARD_TAG_ENTITY, tagd::POS_INTERROGATOR);
	put_test_tag("_referent", "_sub", tagd::POS_REFERENT);
	put_test_tag("_refers", HARD_TAG_ENTITY, tagd::POS_REFERS);
	put_test_tag("_refers_to", HARD_TAG_ENTITY, tagd::POS_REFERS_TO);
	put_test_tag("_context", HARD_TAG_ENTITY, tagd::POS_CONTEXT);
	put_test_tag(HARD_TAG_FLAG, HARD_TAG_ENTITY, tagd::POS_FLAG);
	put_test_tag("_ignore_duplicates", HARD_TAG_FLAG, tagd::POS_FLAG);

	tagd::abstract_tag dog("dog", "animal", tagd::POS_TAG);
	dog.relation("_has", "legs");
	dog.relation("_has", "tail");
	dog.relation("_has", "fur");
	dog.relation("_can", "bark");
	dog.relation("_can", "bite");
	db[dog.id()] = dog;
	_dog = dog;

	tagd::abstract_tag cat("cat", "animal", tagd::POS_TAG);
	cat.relation("_has", "legs");
	cat.relation("_has", "tail");
	cat.relation("_has", "fur");
	cat.relation("_can", "meow");
	cat.relation("_can", "bite");
	db[cat.id()] = cat;
	_cat = cat;

	put_test_tag("breed", "dog", tagd::POS_TAG);
}

tagd::part_of_speech tagdb_tester::pos(const tagd::id_type& id, tdb_sess_t*, tdb_flags_t) {
	tag_map::iterator it = db.find(id);
	if (it == db.end()) return tagd::POS_UNKNOWN;

	return it->second.pos();
}

tagd::code tagdb_tester::get(tagd::abstract_tag& t, const tagd::id_type& id, tdb_sess_t*, tdb_flags_t) {
	tag_map::iterator it = db.find(id);
	if (it == db.end()) return this->ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", id.c_str());

	t = it->second;
	return this->code(tagd::TAGD_OK);
}

tagd::code tagdb_tester::put(const tagd::abstract_tag& t, tdb_sess_t*, tdb_flags_t) {
	if (t.id() == t.super_object())
		return this->error(tagd::TS_MISUSE, "_id == _sub not allowed!");

	if (t.pos() == tagd::POS_UNKNOWN) {
		tagd::abstract_tag parent;
		if (this->get(parent, t.super_object(), nullptr) == tagd::TAGD_OK) {
			tagd::abstract_tag cpy = t;
			cpy.pos(parent.pos());
			db[cpy.id()] = cpy;
		} else {
			return this->code();
		}
	} else {
		db[t.id()] = t;
	}

	return this->code(tagd::TAGD_OK);
}

tagd::code tagdb_tester::del(const tagd::abstract_tag& t, tdb_sess_t*, tdb_flags_t) {
	if (t.pos() != tagd::POS_URL && !t.super_object().empty()) {
		return this->ferror(tagd::TS_MISUSE,
			"sub must not be specified when deleting tag: %s", t.id().c_str());
	}

	tagd::abstract_tag existing;
	this->get(existing, t.id(), nullptr);
	if (this->code() != tagd::TAGD_OK)
		return this->code();

	if (t.relations.empty()) {
		if ( !db.erase(t.id()) )
			return this->ferror(tagd::TS_ERR, "del failed: %s", t.id().c_str());
		else
			return this->code(tagd::TAGD_OK);
	} else {
		for( auto p : t.relations ) {
			if (existing.not_relation(p) == tagd::TAG_UNKNOWN) {
				if (p.modifier.empty()) {
					this->ferror(tagd::TS_NOT_FOUND,
						"cannot delete non-existent relation: %s %s %s",
							t.id().c_str(), p.relator.c_str(), p.object.c_str());
				} else {
					this->ferror(tagd::TS_NOT_FOUND,
						"cannot delete non-existent relation: %s %s %s = %s",
							t.id().c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str());
				}
			}
		}

		return this->put(existing, nullptr);
	}

	assert(false);  // shouldn't get here
	return this->error(tagd::TS_INTERNAL_ERR, "fix del() method");
}

bool tagdb_tester::exists(const tagd::id_type& id, tdb_flags_t) {
	return (db.find(id) != db.end());
}

tagd::code tagdb_tester::query(tagd::tag_set& T, const tagd::interrogator& q, tdb_sess_t*, tdb_flags_t) {
	if (q.super_object().empty() &&
			q.related("legs") &&
			q.related("tail") ) {
		T.insert(_cat);
		T.insert(_dog);
	} else if (q.super_object() == "animal") {
		T.insert(_cat);
		T.insert(_dog);
	}

	return tagd::TS_NOT_FOUND;
}

class callback_tester : public TAGL::callback {
		tagdb::tagdb *_tdb;

		void renew_last_tag(const tagd::part_of_speech& pos = tagd::POS_TAG) {
			if (last_tag != NULL)
				delete last_tag;

			if (pos == tagd::POS_URL)
				last_tag = new tagd::url();
			else
				last_tag = new tagd::abstract_tag();
		}

	public:
		tagd::code last_code;
		tagd::abstract_tag *last_tag;
		tagd::tag_set last_tag_set;
		int cmd;
		int err_cmd;

		callback_tester(tagdb::tagdb *tdb) :
			last_code(), last_tag(NULL), err_cmd(-1) {
			_tdb = tdb;
		}

		void cmd_get(const tagd::abstract_tag& t) {
			cmd = TOK_CMD_GET;
			renew_last_tag(t.pos());
			last_code = _tdb->get(*last_tag, t.id(), nullptr);
			if(t.pos() == tagd::POS_URL)
				((tagd::url*)last_tag)->init(t.id());
		}

		void cmd_put(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_PUT;
			renew_last_tag(t.pos());
			*last_tag = t;
			if (t.pos() == tagd::POS_URL)
				((tagd::url*)last_tag)->init(t.id());
			last_code = _tdb->put(*last_tag, nullptr);
		}

		void cmd_del(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_DEL;
			renew_last_tag(t.pos());
			*last_tag = t;
			if (t.pos() == tagd::POS_URL)
				((tagd::url*)last_tag)->init(t.id());
			last_code = _tdb->del(*last_tag, nullptr);
		}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = TOK_CMD_QUERY;
			renew_last_tag();

			*last_tag = q;
			last_tag_set.clear();
			last_code = _tdb->query(last_tag_set, q, nullptr);
		}

		void cmd_error() {
			cmd = _driver->cmd();

			renew_last_tag();
			if (!_driver->tag().empty())
				*last_tag = _driver->tag();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))
#define INIT_TDB_TAGL() \
	tagdb_tester tdb; \
	auto ssn = tdb.get_session(); \
	httagd::httagl tagl(&tdb, &ssn);


class Tester : public CxxTest::TestSuite {
	public:

    void test_get_tagdurl(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_get(httagd::request(httagd::HTTP_GET, "/dog"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
	}

    void test_get_tagdurl_trailing_path(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_get(httagd::request(httagd::HTTP_GET, "/dog/"));
		tagl.finish();
		// two path separators indicate a query
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
	}

    void test_post_tagdurl(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(httagd::HTTP_POST, "/dog"));
		tagl.execute("_is_a animal _has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

	void test_del_tagdurl(void) {
		INIT_TDB_TAGL();

		tagl.tagdurl_del(httagd::request(httagd::HTTP_DELETE, "/dog"));
		tagl.execute("_is_a animal _has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TS_MISUSE" )

		tagl.clear_errors();
		tagl.tagdurl_del(httagd::request(httagd::HTTP_DELETE, "/dog"));
		tagl.execute("_has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_DEL )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

    void test_post_tagdurl_evbuffer_body(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(httagd::HTTP_POST, "/dog"));

		struct evbuffer *input = evbuffer_new();

		std::string s("_is_a animal _has legs _can bark");
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )

		evbuffer_free(input);
	}

	void test_post_tagdurl_evbuffer_body_constrained_tag_id_error(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(httagd::HTTP_POST, "/dog"));

		struct evbuffer *input = evbuffer_new();

		std::string s("_is_a animal _has legs _can bark; >> cat _is_a animal _has legs _can meow");
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_ERR" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )

		evbuffer_free(input);
	}

    void test_tagdurl_query(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		callback_tester cb(&tdb);
		httagd::httagl tagl(&tdb, &cb, &ssn);
		tagl.tagdurl_get(httagd::request(httagd::HTTP_GET, "/animal/legs,tail"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )
	}

    void test_tagdurl_sub_placeholder_query(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		callback_tester cb(&tdb);
		httagd::httagl tagl(&tdb, &cb, &ssn);
		tagl.tagdurl_get(httagd::request(httagd::HTTP_GET, "/*/legs,tail"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT( cb.last_tag->super_object().empty() )
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT( tagl.tag().super_object().empty() )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )
	}

	// TODO test request::canonical_url(), abs_url(), abs_url_view_tag()
};
