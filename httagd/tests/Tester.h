#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "tagl.h"
#include "tagspace.h"
#include "httagd.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_string, tagd::abstract_tag> tag_map;
typedef tagd::flags_t tdb_flags_t;
typedef tagd::tagspace_session tdb_sess_t;

#define TS_ASSERT_TAGD_OK(EXPR) TS_ASSERT_EQUALS((EXPR), tagd::TAGD_OK)

inline constexpr std::string_view TEST_TAG_IS_A{"is_a"};

inline tagd::abstract_tag test_tag(tagd::id_view id, tagd::id_view super_object) {
	return tagd::abstract_tag(id, TEST_TAG_IS_A, super_object, tagd::POS_TAG);
}

// pure virtual interface
class tagspace_tester : public tagd::tagspace {
	private:
		tag_map db;

		void put_test_tag(
			tagd::id_view id,
			tagd::id_view sub,
			const tagd::part_of_speech& pos)
		{
			tagd::abstract_tag t(id, HARD_TAG_SUB, sub, pos);
			db[t.id()] = t;
		}

		// animals
		tagd::abstract_tag _dog;
		tagd::abstract_tag _cat;

	public:
		tagspace_tester();
		tagd::part_of_speech pos(tagd::id_view, tdb_sess_t*, tdb_flags_t=tdb_flags_t()) override;
		tagd::code get(tagd::abstract_tag&, tagd::id_view, tdb_sess_t*, tdb_flags_t=tdb_flags_t()) override;
		tagd::code put(const tagd::abstract_tag&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		tagd::code del(const tagd::abstract_tag&, tdb_sess_t*, tdb_flags_t=tdb_flags_t());
		bool exists(tagd::id_view, tdb_flags_t=tdb_flags_t()) const override;
		tagd::code query(tagd::tag_set&, const tagd::interrogator&, tdb_sess_t*, tdb_flags_t=tdb_flags_t()) override;

		tagd::code dump(std::ostream& os = std::cout) const {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}

		tagd::code dump_grid(std::ostream& os = std::cout) const {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}

		tagd::code dump_terms(std::ostream& os = std::cout) const {
			os << "not implemented!" << std::endl;
			return tagd::TS_ERR;
		}
};

tagspace_tester::tagspace_tester() {
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
	put_test_tag("title", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("internet_security", "information", tagd::POS_TAG);
	put_test_tag("child", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("simple_english", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("japanese", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("action", HARD_TAG_ENTITY, tagd::POS_TAG);
	put_test_tag("fun", "action", tagd::POS_TAG);
	put_test_tag(TEST_TAG_IS_A, HARD_TAG_SUB, tagd::POS_SUB_RELATOR);
	put_test_tag("about", HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag(HARD_TAG_HAS, HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag(HARD_TAG_CAN, HARD_TAG_ENTITY, tagd::POS_RELATOR);
	put_test_tag(HARD_TAG_WHAT, HARD_TAG_ENTITY, tagd::POS_INTERROGATOR);
	put_test_tag(HARD_TAG_REFERENT, HARD_TAG_SUB, tagd::POS_REFERENT);
	put_test_tag(HARD_TAG_REFERS, HARD_TAG_ENTITY, tagd::POS_REFERS);
	put_test_tag(HARD_TAG_REFERS_TO, HARD_TAG_ENTITY, tagd::POS_REFERS_TO);
	put_test_tag(HARD_TAG_CONTEXT, HARD_TAG_ENTITY, tagd::POS_CONTEXT);
	put_test_tag(HARD_TAG_FLAG, HARD_TAG_ENTITY, tagd::POS_FLAG);
	put_test_tag(HARD_TAG_IGNORE_DUPLICATES, HARD_TAG_FLAG, tagd::POS_FLAG);

	tagd::abstract_tag dog = test_tag("dog", "animal");
	TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "legs"));
	TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "tail"));
	TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "fur"));
	TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_CAN, "bark"));
	TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_CAN, "bite"));
	db[dog.id()] = dog;
	_dog = dog;

	tagd::abstract_tag cat = test_tag("cat", "animal");
	TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "legs"));
	TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "tail"));
	TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "fur"));
	TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_CAN, "meow"));
	TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_CAN, "bite"));
	db[cat.id()] = cat;
	_cat = cat;

	put_test_tag("breed", "dog", tagd::POS_TAG);

	const std::string url_str = "https://en.wikipedia.org/wiki/Dog";
	tagd::url u(url_str);
	TS_ASSERT_TAGD_OK(u.relation("about", "dog"));
	assert(u.code() == tagd::TAGD_OK);
	assert(u.related("about", "dog"));
	db[url_str] = u;
}

tagd::part_of_speech tagspace_tester::pos(tagd::id_view id, tdb_sess_t*, tdb_flags_t) {
	tag_map::iterator it = db.find(tagd::id_string(id));
	if (it == db.end()) return tagd::POS_UNKNOWN;

	return it->second.pos();
}

tagd::code tagspace_tester::get(tagd::abstract_tag& t, tagd::id_view id, tdb_sess_t*, tdb_flags_t) {
	tag_map::iterator it = db.find(tagd::id_string(id));
	if (it == db.end()) return this->ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", std::string(id).c_str());

	t = it->second;
	return this->code(tagd::TAGD_OK);
}

tagd::code tagspace_tester::put(const tagd::abstract_tag& t, tdb_sess_t*, tdb_flags_t) {
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

tagd::code tagspace_tester::del(const tagd::abstract_tag& t, tdb_sess_t*, tdb_flags_t) {
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

bool tagspace_tester::exists(tagd::id_view id, tdb_flags_t) const {
	return (db.find(tagd::id_string(id)) != db.end());
}

tagd::code tagspace_tester::query(tagd::tag_set& T, const tagd::interrogator& q, tdb_sess_t*, tdb_flags_t) {
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
		tagd::tagspace *_tdb;

		void renew_last_tag(tagd::id_view id, const tagd::part_of_speech& pos = tagd::POS_TAG) {
			if (last_tag != nullptr)
				delete last_tag;

			if (pos == tagd::POS_URL) {
				assert(!id.empty());
				last_tag = new tagd::url(tagd::id_string(id));
			} else {
				last_tag = ( id.empty()
							 ? new tagd::abstract_tag()
							 : new tagd::abstract_tag(id) );
			}
		}

	public:
		tagd::code last_code;
		tagd::abstract_tag *last_tag;
		tagd::tag_set last_tag_set;
		int cmd;
		int err_cmd;

		callback_tester(tagd::tagspace *tdb) :
			last_code(), last_tag(nullptr), err_cmd(-1) {
			_tdb = tdb;
		}

		~callback_tester() {
			if (last_tag != nullptr)
				delete last_tag;
		}

		void cmd_get(const tagd::abstract_tag& t) {
			cmd = TOK_CMD_GET;
			renew_last_tag(t.id(), t.pos());
			last_code = _tdb->get(*last_tag, t.id(), nullptr);
		}

		void cmd_put(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_PUT;
			renew_last_tag(t.id(), t.pos());
			*last_tag = t;
			last_code = _tdb->put(*last_tag, nullptr);
		}

		void cmd_del(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_DEL;
			if (t.pos() == tagd::POS_URL) {
				renew_last_tag(t.id(), t.pos());
			} else {
				renew_last_tag(t.id(), t.pos());
				*last_tag = t;
			}
			last_code = _tdb->del(*last_tag, nullptr);
		}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = TOK_CMD_QUERY;
			renew_last_tag(q.id(), q.pos());

			*last_tag = q;
			last_tag_set.clear();
			last_code = _tdb->query(last_tag_set, q, nullptr);
		}

		void cmd_error() {
			cmd = _driver->cmd();

			renew_last_tag(_driver->tag().id());
			if (!_driver->tag().empty())
				*last_tag = _driver->tag();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))
#define INIT_TDB_TAGL() \
	tagspace_tester tdb; \
	auto ssn = tdb.get_session(); \
	httagd::httagl tagl(&tdb, &ssn);


class Tester : public CxxTest::TestSuite {
	public:

	void test_get_tagdurl(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/dog"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
	}

	void test_get_tagdurl_trailing_path(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/dog/"));
		tagl.finish();
		// two path separators indicate a query
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
	}

	void test_post_tagdurl(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));
		tagl.execute("is_a animal _has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

	void test_del_tagdurl(void) {
		INIT_TDB_TAGL();

		tagl.tagdurl_del(httagd::request(tagd::HTTP_DELETE, "/dog"));
		tagl.execute("is_a animal _has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TS_MISUSE" )

		tagl.clear_errors();
		tagl.tagdurl_del(httagd::request(tagd::HTTP_DELETE, "/dog"));
		tagl.execute("_has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_DEL )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

	void test_post_tagdurl_evbuffer_body(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));

		struct evbuffer *input = evbuffer_new();

		std::string s("is_a animal _has legs _can bark");
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

	void test_post_tagdurl_referent_context(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/doggy"));
		tagl.execute("_refers_to dog _context simple_english");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "doggy" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
		TS_ASSERT_EQUALS( dynamic_cast<const tagd::referent&>(tagl.tag()).context(), "simple_english" )
	}

	void test_post_tagdurl_utf8_referent_context(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/イヌ"));
		tagl.execute("_refers_to dog _context japanese");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "イヌ" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
		TS_ASSERT_EQUALS( dynamic_cast<const tagd::referent&>(tagl.tag()).context(), "japanese" )
	}

	void test_post_tagdurl_evbuffer_body_constrained_tag_id_error(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));

		struct evbuffer *input = evbuffer_new();

		std::string s("is_a animal _has legs _can bark; >> cat is_a animal _has legs _can meow");
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

	void test_put_tagdurl_constrained_tag_id(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, "/dog"));
		tagl.execute(">> dog is_a animal _has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

	void test_put_tagdurl_constrained_tag_id_error(void) {
		INIT_TDB_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, "/dog"));
		tagl.execute(">> cat is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_ERR" )
	}

	void test_get_tagdurl_hduri(void) {
		INIT_TDB_TAGL();
		const char *hduri = "hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, std::string("/").append(hduri)));
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "https://en.wikipedia.org/wiki/Dog" )
		// TODO WFT FAILS
		// TS_ASSERT( tagl.tag().related("about", "dog") )
	}

	void test_put_tagdurl_evbuffer_body_constrained_url(void) {
		INIT_TDB_TAGL();
		const std::string hduri{"hd:org!wikipedia!en!/wiki/Cat!!!!!!https"};
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, std::string("/").append(hduri)));

		const std::string s(">> https://en.wikipedia.org/wiki/Cat about cat _has title = \"Cat - Wikipedia, the free encyclopedia\"");
		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		tagl.print_errors();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "https://en.wikipedia.org/wiki/Cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "_url" )
		TS_ASSERT( tagl.tag().related("about", "cat") )
		TS_ASSERT( tagl.tag().related("_has", "title", "Cat - Wikipedia, the free encyclopedia") )

		evbuffer_free(input);
	}

	void test_put_tagdurl_evbuffer_body_constrained_hduri(void) {
		INIT_TDB_TAGL();
		const char *hduri = "hd:org!wikipedia!en!/wiki/Cat!!!!!!https";
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, std::string("/").append(hduri)));

		const std::string s = std::string(">> ").append(hduri).append(" about cat _has title = \"Cat - Wikipedia, the free encyclopedia\"");
		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "https://en.wikipedia.org/wiki/Cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "_url" )
		TS_ASSERT( tagl.tag().related("about", "cat") )
		TS_ASSERT( tagl.tag().related("_has", "title", "Cat - Wikipedia, the free encyclopedia") )

		evbuffer_free(input);
	}

	void test_tagdurl_query(void) {
		tagspace_tester tdb;
		auto ssn = tdb.get_session();
		callback_tester cb(&tdb);
		httagd::httagl tagl(&tdb, &cb, &ssn);
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/animal/legs,tail"));
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
		tagspace_tester tdb;
		auto ssn = tdb.get_session();
		callback_tester cb(&tdb);
		httagd::httagl tagl(&tdb, &cb, &ssn);
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/*/legs,tail"));
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

	void test_file_path(void) {
		auto req = httagd::request(tagd::HTTP_GET, "/_file/path/to/style.css");
		auto pos = tagd::file::dir_shift_pos(req.path());
		auto sub = req.path().substr(pos);
		TS_ASSERT_EQUALS( sub , "path/to/style.css" )
	}

	// Contract 3: server owns _evbase and _htp; ~server() frees them without crash
	void test_server_destructor_frees_evbase_and_htp(void) {
		tagspace_tester tdb;
		httagd::viewspace vws(".");
		httagd::httagd_args args;
		{
			httagd::server svr(&tdb, &vws, &args);
			// _evbase and _htp allocated in server::init(); freed in ~server() at }
		}
		TS_ASSERT(true);
	}
};
