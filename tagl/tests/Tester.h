// Test suites

#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "tagl.h"
#include "tagdb.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_type, tagd::abstract_tag> tag_map;
typedef tagdb::flags_t ts_flags_t;

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

		tagdb_tester() {
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
			put_test_tag("_message", HARD_TAG_ENTITY, tagd::POS_TAG);
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

		tagd::part_of_speech pos(const tagd::id_type& id, ts_flags_t flags = ts_flags_t()) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return tagd::POS_UNKNOWN;

			return it->second.pos();
		}

		tagd::code get(tagd::abstract_tag& t, const tagd::id_type& id, ts_flags_t flags = ts_flags_t()) {
			tag_map::iterator it = db.find(id);
			if (it == db.end())
				return this->ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", id.c_str());

			t = it->second;
			return this->code(tagd::TAGD_OK);
		}

		tagd::code put(const tagd::abstract_tag& t, ts_flags_t flags = ts_flags_t()) {
			if (t.id() == t.super_object())
				return this->error(tagd::TS_MISUSE, "_id == _sub not allowed!");

			if (t.pos() == tagd::POS_UNKNOWN) {
				tagd::abstract_tag parent;
				if (this->get(parent, t.super_object()) == tagd::TAGD_OK) {
					tagd::abstract_tag cpy = t;
					cpy.pos(parent.pos());
					db[cpy.id()] = cpy;
				} else {
					return this->code();
				}
			} else if (t.pos() == tagd::POS_URL) {
				// we could make a tagd::url copy constructor/operator
				// but this is the only place where this madness occurs
				tagd::url u;
				u.init(t.id());
				u.relations = t.relations;
				db[u.hduri()] = u;
			} else {
				db[t.id()] = t;
			}

			return this->code(tagd::TAGD_OK);
		}

		tagd::code del(const tagd::abstract_tag& t, ts_flags_t flags = ts_flags_t()) {
			if (!t.super_object().empty() && t.pos() != tagd::POS_URL) {
				return this->ferror(tagd::TS_MISUSE,
					"sub must not be specified when deleting tag: %s", t.id().c_str());
			}

			tagd::abstract_tag existing;
			auto id = (
				t.pos() == tagd::POS_URL
				? static_cast<tagd::url const *>(&t)->hduri()
				: t.id() );

			this->get(existing, id, flags);
			if (this->code() != tagd::TAGD_OK)
				return this->code();

			if (t.relations.empty()) {
				if ( db.erase(id) )
					return this->code(tagd::TAGD_OK);
				else
					return this->ferror(tagd::TS_ERR, "del failed: %s", id.c_str());
			} else {
				for( auto p : t.relations ) {
					if (existing.not_relation(p) == tagd::TAG_UNKNOWN) {
						if (p.modifier.empty()) {
							this->ferror(tagd::TS_NOT_FOUND,
								"cannot delete non-existent relation: %s %s %s",
									id.c_str(), p.relator.c_str(), p.object.c_str());
						} else {
							this->ferror(tagd::TS_NOT_FOUND,
								"cannot delete non-existent relation: %s %s %s = %s",
									id.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str());
						}
					}
				}

				return this->put(existing, flags);
			}

			assert(false);  // shouldn't get here
			return this->error(tagd::TS_INTERNAL_ERR, "fix del() method");
		}

		tagd::code exists(const tagd::id_type& id) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return tagd::TS_NOT_FOUND;

			return tagd::TAGD_OK;
		}

		tagd::code query(tagd::tag_set& T, const tagd::interrogator& q, ts_flags_t flags = ts_flags_t()) {
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

		tagd::code dump(std::ostream& os = std::cout) {
			for (auto it = db.begin(); it != db.end(); ++it) {
				os << "-- " << it->first << " , " << pos_str(it->second.pos()) << std::endl;
				os << it->second << std::endl << std::endl;
			}

			return tagd::TAGD_OK;
		}

		tagd::code dump_grid(std::ostream& os = std::cout) {
			assert(false);
			return tagd::TS_ERR;
		}

		tagd::code dump_terms(std::ostream& os = std::cout) {
			assert(false);
			return tagd::TS_ERR;
		}
};

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
			if(t.pos() == tagd::POS_URL) {
				((tagd::url*)last_tag)->init(t.id());
				last_code = _tdb->get(*last_tag, ((tagd::url*)last_tag)->hduri());
			} else {
				last_code = _tdb->get(*last_tag, t.id());
			}
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
			last_code = _tdb->put(*last_tag);
		}

		void cmd_del(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_DEL;
			renew_last_tag(t.pos());
			*last_tag = t;
			if (t.pos() == tagd::POS_URL) {
				((tagd::url*)last_tag)->init(t.id());
			}

			last_code = _tdb->del(*last_tag);
		}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = TOK_CMD_QUERY;
			renew_last_tag();

			*last_tag = q;
			last_tag_set.clear();
			last_code = _tdb->query(last_tag_set, q);
		}

		void cmd_error() {
			cmd = _driver->cmd();

			renew_last_tag();
			if (!_driver->tag().empty())
				*last_tag = _driver->tag();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd_code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

    void test_ctor(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
	}

    void test_subject(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("<< dog");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

    void test_get_sub_identity_error(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("<< dog _is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_subject_sub_identity(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog _is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
	}

    void test_unknown_sub_relator(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog snarfs animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_unknown_super_object(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog _is_a snarfadoodle");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_subject_predicate(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog _has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_subject_newline_predicate(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog\n_has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_subject_predicate_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog _has legs, tail, fur");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
	}

    void test_subject_newline_predicate_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog\n_has legs, tail, fur");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
	}

    void test_subject_identity_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_identity_newline_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_newline_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_dash_modifier(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> shar_pei _is_a dog\n"
				"_has breed = Shar-Pei"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_url_modifier(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.execute(
				">> shar_pei _is_a dog\n"
				"_has breed = http://www.dogbreedinfo.com/sharpei.htm"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
	}

	void test_url_list(void) {
		tagdb_tester tdb;
		tdb.put(tagd::tag("simple", "_is_a", "_concept"));
		tdb.put(tagd::referent("Shar Pei", "shar_pei", "simple"));
		tdb.put(tagd::referent("SHARPEI","shar_pei","code"));
		TAGL::driver tagl(&tdb);
		tagl.execute(
				">> shar_pei _is_a dog\n"
				"_has information = http://www.dogbreedinfo.com/sharpei.htm, breed = http://www.akc.org/breeds/chinese_shar_pei/index.cfm, SHARPEI, \"Shar Pei\""
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
	}

	void test_delete_sub_not_allowed(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				"!! dog _is_a animal\n"
				"_can bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_delete(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
				"!! dog\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_DEL )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tc = tagl.execute( "!! dog" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		// TODO fix not_found_context_dichotomy (see parser.y)
		//tc = tagl.execute( "<< dog" );
		//TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
		tagl.execute( "<< dog" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )

		tc = tagl.execute( "!! dog" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_url(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> http://www.hypermega.com/a/b/c#here?x=1&y=2\n"
				"about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "http://www.hypermega.com/a/b/c#here?x=1&y=2" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_URL )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_URL )
		TS_ASSERT( tagl.tag().related("about", "internet_security") )
	}

    void test_url_dot_dash_plus_scheme(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> svn+ssh://www.hypermega.com\n"
				"about internet_security\n"
				"\n"
				">> git-svn-https://www.hypermega.com\n"
				"about internet_security\n"
				"\n"
				">> never.seen.a.dot.scheme://www.hypermega.com\n"
				"about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "never.seen.a.dot.scheme://www.hypermega.com" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_URL )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_URL )
		TS_ASSERT( tagl.tag().related("about", "internet_security") )
	}

    void test_multiple_statements_whitespace(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				"<< dog\n"
				" \n \t\n "
				"<< cat\n"
				"\n\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
	}

    void test_blank(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT( tagl.tag().empty() )
	}

    void test_blank_lines(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(" \n \t \n\n\t ");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT( tagl.tag().empty() )
	}

    void test_multiple_statements(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
				"\n"
				"<< dog\n"
				"\n"
				">> cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

	}

	void test_constrain_tag_id_consistent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.constrain_tag_id("dog");
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"\n"
				">> dog\n"
				"_can bark, bite\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_constrain_tag_id_inconsistent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.constrain_tag_id("dog");
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"\n"
				">> cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
	}

    void test_multiple_parse(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tc = tagl.execute(
				">> cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_quantifiers(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				">> dog _has legs= 4, tail = 1, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_has", "tail", "1") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tc = tagl.execute(
				">> cat _is_a animal \n"
				"_can meow, bite\n"
				"_has legs =4"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_one_line_quantifiers(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
			">> dog _has legs = 4, tail = 1 _can bark, bite"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_has", "tail", "1") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

	}

    void test_single_line(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
		  ">> dog _is_a animal _has legs, tail, fur _can bark, bite; >> cat _is_a animal _has legs, tail, fur _can meow, bite"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		// tag only holds last tag statement
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_put_subject_unknown(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
		  ">> snipe _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "snipe" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_put_subject_known(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
		  ">> dog _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_put_sub_unknown(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
		  ">> dog _is_a snarf _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_ignore_newline_eof_error(void) {
		tagdb_tester tdb;
		tagd::code tc;
		TAGL::driver a(&tdb);
		tc = a.execute("<< dog\n");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( a.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( a.tag().id() , "dog" )

		TAGL::driver b(&tdb);
		tc = b.execute(">> dog _is_a animal\n");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( b.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( b.tag().id() , "dog" )
		TS_ASSERT_EQUALS( b.tag().super_object() , "animal" )

		TAGL::driver c(&tdb);
		tc = c.execute(">> dog _has legs\n");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( c.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( c.tag().id() , "dog" )
		TS_ASSERT( c.tag().related("_has", "legs") )

		TAGL::driver d(&tdb);
		tc = d.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( d.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( d.tag().id() , "dog" )
		TS_ASSERT_EQUALS( d.tag().super_object() , "animal" )
		TS_ASSERT( d.tag().related("_has", "legs") )
		TS_ASSERT( d.tag().related("_has", "tail") )
		TS_ASSERT( d.tag().related("_has", "fur") )
		TS_ASSERT( d.tag().related("_can", "bark") )
		TS_ASSERT( d.tag().related("_can", "bite") )
	}

	void test_comment(void) {
		tagdb_tester tdb;
		tagd::code tc;

		TAGL::driver d(&tdb);
		// TODO random error sometimes raised by this statement
		//TAGL::driver::trace_on();
		tc = d.execute(
			"-- this is a comment\n"
			">> dog _is_a animal --so is this\n"
			"_has legs, tail-- no space between token and comment\n"
			"_has-* i'm a block comment *-fur\n"
			"--_can bark, bite"
		);
		//TAGL::driver::trace_off();
		if (d.has_errors())
			d.print_errors();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( d.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( d.tag().id() , "dog" )
		TS_ASSERT_EQUALS( d.tag().super_object() , "animal" )
		TS_ASSERT( d.tag().related("_has", "legs") )
		TS_ASSERT( d.tag().related("_has", "tail") )
		TS_ASSERT( d.tag().related("_has", "fur") )
		TS_ASSERT( !d.tag().related("_can", "bark") )
		TS_ASSERT( !d.tag().related("_can", "bite") )

		tc = d.execute(
				">> dog _is_a animal -* i'm a block comment\n"
				"and I don't end"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_parseln_terminator(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(">> dog _is_a animal;");
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
	}

   void test_parseln_finish(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(">> dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagl.parseln(">> cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_parseln_no_finish(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(">> dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagl.parseln(">> cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_quotes(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
			">> communication _is_a _entity;\n"
			">> information _is_a communication;\n"
			">> content _is_a information;\n"
			">> message _is_a information;\n"
			">> title _is_a content;"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(
				">> my_title _is_a title\n"
				"_has message = \"my title!\";"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related("_has", "message", "my title!") )

		tc = tagl.execute(
				">> my_quoted_title _is_a title\n"
				"_has message = \"my \\\"quoted\\\" title!\";"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_quoted_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related("_has", "message", "my \\\"quoted\\\" title!") )
	}

	void test_quotes_parseln(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
			">> communication _is_a _entity;\n"
			">> information _is_a communication;\n"
			">> content _is_a information;\n"
			">> message _is_a information;\n"
			">> title _is_a content;"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tagl.parseln(">> my_title _is_a title");
		tagl.parseln("_has message = \"my title!\";");
		tagl.finish();
		TS_ASSERT( !tagl.has_errors() )
		if (tagl.has_errors()) tagl.print_errors();
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related("_has", "message", "my title!") )

		tagl.parseln(">> my_quoted_title _is_a title");
		tagl.parseln("_has message = \"my \\\"quoted\\\" title!\";");
		tagl.finish();
		TS_ASSERT( !tagl.has_errors() )
		if (tagl.has_errors()) tagl.print_errors();
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_quoted_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related("_has", "message", "my \\\"quoted\\\" title!") )
	}

	void test_callback(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
				"\n"
				">> cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		tagd::abstract_tag t;
		tc = tdb.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super_object(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

	void test_callback_semicolon(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
				">> dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite;"
				">> cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagd::abstract_tag t;
		tc = tdb.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super_object(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

	void test_put_referent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> doggy _refers_to dog");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	// tagdb_tester won't allow this, but tagl parser will
	// void test_put_referent_self(void) {
	// 	tagdb_tester tdb;
	// 	TAGL::driver tagl(&tdb);
	// 	tagd::code tc = tagl.execute(">> dog _refers_to dog");
	// 	TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	// }

	void test_put_referent_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> doggy _refers_to dog _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_put_referent_referent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> refers_to _refers_to _refers_to");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_set_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("%% _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tdb.context().size() , 1 )
		if ( tdb.context().size() > 0 )
			TS_ASSERT_EQUALS( tdb.context()[0] , "child" )
	}

	void test_set_blank_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("%% _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tdb.context().size() , 1 )
		tc = tagl.execute("%% _context \"\"");
		TS_ASSERT_EQUALS( tdb.context().size() , 0 )
	}

	void test_set_ignore_duplicates(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		TS_ASSERT( tagl.flags() != tagdb::F_IGNORE_DUPLICATES )

		tagd::code tc = tagl.execute("%% _ignore_duplicates 1");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags() == tagdb::F_IGNORE_DUPLICATES )

		tc = tagl.execute("%% _ignore_duplicates 0");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags() != tagdb::F_IGNORE_DUPLICATES )

		tc = tagl.execute("%% _ignore_duplicates 5");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags() == tagdb::F_IGNORE_DUPLICATES )
	}

	void test_set_context_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("%% _context child, animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tdb.context().size() , 2 )
		if ( tdb.context().size() == 2 )
			TS_ASSERT_EQUALS( tdb.context()[0] , "child" )
			TS_ASSERT_EQUALS( tdb.context()[1] , "animal" )
	}

	void test_set_context_list_3(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute("%% _context child, animal, fun");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tdb.context().size() , 3 )
		if ( tdb.context().size() == 3 )
			TS_ASSERT_EQUALS( tdb.context()[0] , "child" )
			TS_ASSERT_EQUALS( tdb.context()[1] , "animal" )
			TS_ASSERT_EQUALS( tdb.context()[2] , "fun" )
	}

    void test_query(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute("?? _what _is_a animal _has legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , "_what" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		tc = tagl.execute("?? _what _is_a animal _has legs > 3, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
	}

    void test_query_wildcard_relator(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute("?? _what _is_a animal * legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "animal" )

		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )
	}

    void test_query_no_sub_wildcard_relator(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute("?? _what * legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT( cb.last_tag->super_object().empty() )

		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )
	}
//     void test_query_not_found(void) {
// 		tagdb_tester tdb;
// 		callback_tester cb(&tdb);
// 		TAGL::driver tagl(&tdb, &cb);
// 		tagd::code tc = tagl.execute("<< _what _is_a snipe");
// 		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT( cb.err_cmd == TOK_CMD_GET )
// 	}

	void test_query_referents(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);

		// refers.empty() && refers_to.empty() && context.empty()
		tagl.execute("?? _what _is_a _referent");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 0 )

		// refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute("?? _what _context animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "animal") )

		// refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute("?? _what _refers_to animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )

		// refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute("?? _what _refers_to animal _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )

		// !refers.empty() && refers_to.empty() && context.empty()
		tagl.execute("?? _what _refers animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "animal") )

		// !refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute("?? _what _refers thing _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )

		// !refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute("?? _what _refers thing _refers_to animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )

		// !refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute("?? _what _refers thing _refers_to animal _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 3 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )
	}

	void test_url_callback(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
				">> http://hypermega.com\n"
				"about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "http://hypermega.com" )
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "com:hypermega:http" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "_url" )
		TS_ASSERT( cb.last_tag->related("about", "internet_security") )
	}

	void test_put_get_url(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
				">> http://hypermega.com\n"
				"about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		tc = tagl.execute( "<< http://hypermega.com ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "http://hypermega.com" )
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "com:hypermega:http" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "_url" )
		TS_ASSERT( cb.last_tag->related("about", "internet_security") )
	}

	void test_put_del_get_url(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tdb.put(tagd::relator("powered_by",HARD_TAG_ENTITY));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tdb.code()), "TAGD_OK" )
		tdb.put(tagd::tag("wikimedia",HARD_TAG_ENTITY));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tdb.code()), "TAGD_OK" )

		tagd::code tc = tagl.execute(
				">> https://en.wikipedia.org/wiki/Dog\n"
				"about dog, cat\n"
				"powered_by wikimedia"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(
				"!! https://en.wikipedia.org/wiki/Dog\n"
				"about cat"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute( "<< https://en.wikipedia.org/wiki/Dog ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/Dog" )
		TS_ASSERT( cb.last_tag->related("about", "dog") )
		TS_ASSERT( !cb.last_tag->related("about", "cat") )
		TS_ASSERT( cb.last_tag->related("powered_by", "wikimedia") )

		tc = tagl.execute( "!! https://en.wikipedia.org/wiki/Dog" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute( "<< https://en.wikipedia.org/wiki/Dog ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
	}

	void test_get_hduri(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		// looks like quantifier in path
		tagd::code tc = tagl.execute(
				">> https://en.wikipedia.org/wiki/-99Dog\n"
				"about dog\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute( "<< https://en.wikipedia.org/wiki/-99Dog" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/-99Dog" )
		TS_ASSERT( cb.last_tag->related("about", "dog") )

		// hduri
		// tagl.trace_on();
		tc = tagl.execute( "<< org:wikipedia:en:/wiki/-99Dog:https" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/-99Dog" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( static_cast<tagd::url *>(cb.last_tag)->hduri(), "org:wikipedia:en:/wiki/-99Dog:https" )
		TS_ASSERT( cb.last_tag->related("about", "dog") )

		tc = tagl.execute( "!! org:wikipedia:en:/wiki/-99Dog:https" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		// TODO fix not_found_context_dichotomy (see parser.y)
		//tc = tagl.execute( "<< https://en.wikipedia.org/wiki/-99Dog ;" );
		//TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		tagl.execute( "<< https://en.wikipedia.org/wiki/-99Dog ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )

		tagl.execute( "<< org:wikipedia:en:/wiki/-99Dog:https" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
	}

	void test_uri_put_get_semicolon(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.execute(">> myuri:dog _is_a _entity;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(
				">> https://en.wikipedia.org/wiki/Dog\n"
				"about myuri:dog;\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute( "<< org:wikipedia:en:/wiki/Dog:https;\n" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/Dog" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( static_cast<tagd::url *>(cb.last_tag)->hduri(), "org:wikipedia:en:/wiki/Dog:https" )
		TS_ASSERT( cb.last_tag->related("about", "myuri:dog") )

		tc = tagl.execute( "!! org:wikipedia:en:/wiki/Dog:https;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		// TODO fix not_found_context_dichotomy (see parser.y)
		//tc = tagl.execute( "<< https://en.wikipedia.org/wiki/Dog ;" );
		//TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		tagl.execute( "<< https://en.wikipedia.org/wiki/Dog;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )

		tagl.execute( "<< org:wikipedia:en:/wiki/Dog:https;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
	}

    void test_evbuffer_scan(void) {
		struct evbuffer *input = evbuffer_new();
		std::string s(">> dog _is_a animal _has legs, fur _can bark");
		evbuffer_add(input, s.c_str(), s.size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);

		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

	void test_quoted_str_size_of_buf(void) {
		std::stringstream ss, ss1;
		ss << ">> my_message1 _is_a _entity\n"
		   << "_has _message = ";

		// fill buf
		auto sz = ss.str().size();
		while ( sz++ < TAGL::buf_sz )
			ss << ' ';

		// trigger a fill so that a quoted string
		// completely fills the buffer
		ss1 << '"';
		sz = ss1.str().size();
		while ( sz++ < TAGL::buf_sz - 1 )
			ss1 << (char)(sz % 10 + 48);  // ascii 0-9
		ss1 << '"';

		ss << ss1.str();
		ss << "\n\n";
		ss << ">> my_message2 _is_a _entity\n"
		   << "_has _message = \"another message\"";

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_quoted_str_larger_than_buf(void) {
		std::stringstream ss, ss1;
		ss << ">> my_message1 _is_a _entity\n"
		   << "_has _message = \"";

		auto sz = ss.str().size();
		while ( sz++  < (TAGL::buf_sz * 3) )
			ss << (char)(sz % 10 + 48);  // ascii 0-9

		ss << "\"\n\n";
		ss << ">> my_message2 _is_a _entity\n"
		   << "_has _message = \"another message\"";

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}
};
