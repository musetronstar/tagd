// Test suites

#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "tagl.h"
#include "tagspace.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_type, tagd::abstract_tag> tag_map;
typedef tagspace::flags_t ts_flags_t;

// pure virtual interface
class tagspace_tester : public tagspace::tagspace {
	private:
		tag_map db;
		
		void put_test_tag(
			const tagd::id_type& id,
			const tagd::id_type& super,
			const tagd::part_of_speech& pos)
		{
			tagd::abstract_tag t(id, super, pos);
			db[t.id()] = t;
		}

		// animals
		tagd::abstract_tag _dog;
		tagd::abstract_tag _cat;
	public:

		tagspace_tester() {
			put_test_tag("living_thing", "_entity", tagd::POS_TAG);
			put_test_tag("animal", "_entity", tagd::POS_TAG);
			put_test_tag("legs", "_entity", tagd::POS_TAG);
			put_test_tag("tail", "_entity", tagd::POS_TAG);
			put_test_tag("fur", "_entity", tagd::POS_TAG);
			put_test_tag("bark", "_entity", tagd::POS_TAG);
			put_test_tag("meow", "_entity", tagd::POS_TAG);
			put_test_tag("bite", "_entity", tagd::POS_TAG);
			put_test_tag("swim", "_entity", tagd::POS_TAG);
			put_test_tag("internet_security", "_entity", tagd::POS_TAG);
			put_test_tag("child", "_entity", tagd::POS_TAG);
			put_test_tag("action", "_entity", tagd::POS_TAG);
			put_test_tag("fun", "action", tagd::POS_TAG);
			put_test_tag("_is_a", "_entity", tagd::POS_SUPER);
			put_test_tag("_about", "_entity", tagd::POS_RELATOR);
			put_test_tag("_has", "_entity", tagd::POS_RELATOR);
			put_test_tag("_can", "_entity", tagd::POS_RELATOR);
			put_test_tag("_what", "_entity", tagd::POS_INTERROGATOR);
			put_test_tag("_referent", "_super", tagd::POS_REFERENT);
			put_test_tag("_refers", "_entity", tagd::POS_REFERS);
			put_test_tag("_refers_to", "_entity", tagd::POS_REFERS_TO);
			put_test_tag("_context", "_entity", tagd::POS_CONTEXT);

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
		}

		tagd::part_of_speech pos(const tagd::id_type& id) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return tagd::POS_UNKNOWN;

			return it->second.pos();
		}

		tagd::code get(tagd::abstract_tag& t, const tagd::id_type& id, const ts_flags_t& flags = ts_flags_t()) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return tagd::TS_NOT_FOUND;

			t = it->second;
			return tagd::TAGD_OK;
		}

		tagd::code put(const tagd::abstract_tag& t, const ts_flags_t& flags = ts_flags_t()) {
			db[t.id()] = t;

			return tagd::TAGD_OK;
		}

		tagd::code exists(const tagd::id_type& id) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return tagd::TS_NOT_FOUND;

			return tagd::TAGD_OK;
		}

		tagd::code query(tagd::tag_set& T, const tagd::interrogator& q) {
			if (q.super().empty() &&
					q.related("legs") &&
					q.related("tail") ) {
				T.insert(_cat);
				T.insert(_dog);
			} else if (q.super() == "animal") {
				T.insert(_cat);
				T.insert(_dog);
			}

			return tagd::TS_NOT_FOUND;
		}

		tagd::code dump(std::ostream& os = std::cout) {
			assert(false);
			return tagd::TS_ERR;
		}

		tagd::code dump_grid(std::ostream& os = std::cout) {
			assert(false);
			return tagd::TS_ERR;
		}
};

class callback_tester : public TAGL::callback {
		tagspace::tagspace *_TS;

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
		
		callback_tester(tagspace::tagspace *ts) :
			last_code(), last_tag(NULL), err_cmd(-1) {
			_TS = ts;
		}

		void cmd_get(const tagd::abstract_tag& t) {
			cmd = CMD_GET;
			renew_last_tag(t.pos());
			last_code = _TS->get(*last_tag, t.id());
			if(t.pos() == tagd::POS_URL)
				((tagd::url*)last_tag)->init(t.id());
		}

		void cmd_put(const tagd::abstract_tag& t) {
			cmd = CMD_PUT;
			renew_last_tag(t.pos());
			*last_tag = t;
			if (t.pos() == tagd::POS_URL)
				((tagd::url*)last_tag)->init(t.id());
			last_code = _TS->put(*last_tag);
		}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = CMD_QUERY;
			renew_last_tag();

			*last_tag = q;
			last_tag_set.clear();
			last_code = _TS->query(last_tag_set, q);
		}

		void error(const TAGL::driver& D) {
			cmd = D.cmd();

			renew_last_tag();
			if (!D.tag().empty())
				*last_tag = D.tag();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd_code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

    void test_ctor(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
	}

    void test_subject(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("GET dog");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

    void test_get_super_identity_error(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("GET dog _is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_subject_super_identity(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog _is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
	}

    void test_unknown_super_relator(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog snarfs animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_unknown_super(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog _is_a snarfadoodle");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_subject_predicate(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog _has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_subject_newline_predicate(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog\n_has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_subject_predicate_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog _has legs, tail, fur");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
	}

    void test_subject_newline_predicate_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT dog\n_has legs, tail, fur");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
	}

    void test_subject_identity_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_identity_newline_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_subject_newline_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_url(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT http://www.hypermega.com/a/b/c#here?x=1&y=2\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "http://www.hypermega.com/a/b/c#here?x=1&y=2" )
		TS_ASSERT_EQUALS( tagl.tag().super() , HARD_TAG_URL )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_URL )
		TS_ASSERT( tagl.tag().related("_about", "internet_security") )
	}

    void test_url_dot_dash_plus_scheme(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT svn+ssh://www.hypermega.com\n"
				"_about internet_security\n"
				"\n"
				"PUT git-svn-https://www.hypermega.com\n"
				"_about internet_security\n"
				"\n"
				"PUT never.seen.a.dot.scheme://www.hypermega.com\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "never.seen.a.dot.scheme://www.hypermega.com" )
		TS_ASSERT_EQUALS( tagl.tag().super() , HARD_TAG_URL )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_URL )
		TS_ASSERT( tagl.tag().related("_about", "internet_security") )
	}

    void test_multiple_statements_whitespace(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"GET dog\n"
				" \n \t\n "
				"GET cat\n"
				"\n\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
	}

    void test_blank(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT( tagl.tag().empty() )
	}

    void test_blank_lines(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(" \n \t \n\n\t ");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT( tagl.tag().empty() )
	}

    void test_multiple_statements(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
				"\n"
				"GET dog\n"
				"\n"
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

	}

    void test_multiple_parse(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tc = tagl.execute(
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_quantifiers(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
				"PUT dog _has legs= 4, tail = 1, fur\n"
				"_can bark, bite"
			);	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_has", "tail", "1") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tc = tagl.execute(
				"PUT cat _is_a animal \n"
				"_can meow, bite\n"
				"_has legs =4"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_one_line_quantifiers(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
			"PUT dog _has legs = 4, tail = 1 _can bark, bite"
		);	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related("_has", "legs", "4") )
		TS_ASSERT( tagl.tag().related("_has", "tail", "1") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

	}

    void test_single_line(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
		  "PUT dog _is_a animal _has legs, tail, fur _can bark, bite; PUT cat _is_a animal _has legs, tail, fur _can meow, bite"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		// tag only holds last tag statement
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_put_subject_unknown(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
		  "PUT snipe _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "snipe" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_put_subject_known(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
		  "PUT dog _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

    void test_put_super_unknown(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute(
		  "PUT dog _is_a snarf _has legs"
		);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_ignore_newline_eof_error(void) {
		tagspace_tester TS;
		tagd_code tc;
		TAGL::driver a(&TS);
		tc = a.execute("GET dog\n");	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( a.cmd() , CMD_GET )
		TS_ASSERT_EQUALS( a.tag().id() , "dog" )

		TAGL::driver b(&TS);
		tc = b.execute("PUT dog _is_a animal\n");	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( b.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( b.tag().id() , "dog" )
		TS_ASSERT_EQUALS( b.tag().super() , "animal" )

		TAGL::driver c(&TS);
		tc = c.execute("PUT dog _has legs\n");	
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( c.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( c.tag().id() , "dog" )
		TS_ASSERT( c.tag().related("_has", "legs") )

		TAGL::driver d(&TS);
		tc = d.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( d.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( d.tag().id() , "dog" )
		TS_ASSERT_EQUALS( d.tag().super() , "animal" )
		TS_ASSERT( d.tag().related("_has", "legs") )
		TS_ASSERT( d.tag().related("_has", "tail") )
		TS_ASSERT( d.tag().related("_has", "fur") )
		TS_ASSERT( d.tag().related("_can", "bark") )
		TS_ASSERT( d.tag().related("_can", "bite") )
	}

    void test_parseln_terminator(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal;");
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
	}

   void test_parseln_finish(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagl.parseln("PUT cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_INIT" )
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

    void test_parseln_no_finish(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagl.parseln("PUT cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )
	}

	void test_callback(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
				"\n"
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		tagd::abstract_tag t;
		tagd::code ts_rc = TS.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ts_rc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

	void test_callback_semicolon(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite;"
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "meow") )
		TS_ASSERT( tagl.tag().related("_can", "bite") )

		tagd::abstract_tag t;
		tagd::code ts_rc = TS.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(ts_rc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

	void test_put_referent(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT doggy _refers_to dog");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_put_referent_context(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("PUT doggy _refers_to dog _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_set_context(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("SET _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( TS.context().size() , 1 )
		if ( TS.context().size() > 0 )
			TS_ASSERT_EQUALS( TS.context()[0] , "child" )
	}

	void test_set_blank_context(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("SET _context child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( TS.context().size() , 1 )
		tc = tagl.execute("SET _context \"\"");
		TS_ASSERT_EQUALS( TS.context().size() , 0 )
	}

	void test_set_context_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("SET _context child, animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( TS.context().size() , 2 )
		if ( TS.context().size() == 2 )
			TS_ASSERT_EQUALS( TS.context()[0] , "child" )
			TS_ASSERT_EQUALS( TS.context()[1] , "animal" )
	}

	void test_set_context_list_3(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.execute("SET _context child, animal, fun");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( TS.context().size() , 3 )
		if ( TS.context().size() == 3 )
			TS_ASSERT_EQUALS( TS.context()[0] , "child" )
			TS_ASSERT_EQUALS( TS.context()[1] , "animal" )
			TS_ASSERT_EQUALS( TS.context()[2] , "fun" )
	}

    void test_query(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute("QUERY _what _is_a animal _has legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , "_what" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )
	}

    void test_query_wildcard_relator(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute("QUERY _what _is_a animal * legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )
	}

    void test_query_no_super_wildcard_relator(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute("QUERY _what * legs, tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT( cb.last_tag->super().empty() )
		
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("_has", "legs") )
		TS_ASSERT( it->related("_has", "tail") )
	}
//     void test_query_not_found(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagd_code tc = tagl.execute("GET _what _is_a snipe");
// 		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT( cb.err_cmd == CMD_GET )
// 	}

	void test_query_referents(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);

		// refers.empty() && refers_to.empty() && context.empty()
		tagl.execute("QUERY _what _is_a _referent");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 0 )

		// refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute("QUERY _what _context animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "animal") )

		// refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute("QUERY _what _refers_to animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )

		// refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute("QUERY _what _refers_to animal _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )

		// !refers.empty() && refers_to.empty() && context.empty()
		tagl.execute("QUERY _what _refers animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "animal") )

		// !refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute("QUERY _what _refers thing _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )
	
		// !refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute("QUERY _what _refers thing _refers_to animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )
	
		// !refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute("QUERY _what _refers thing _refers_to animal _context living_thing");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 3 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "animal") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )
	}

	void test_url_callback(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute(
				"PUT http://hypermega.com\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "http://hypermega.com" )
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "com:hypermega:http" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "_url" )
		TS_ASSERT( cb.last_tag->related("_about", "internet_security") )
	}

	void test_put_get_url(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagd_code tc = tagl.execute(
				"PUT http://hypermega.com\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		tc = tagl.execute( "GET http://hypermega.com ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "http://hypermega.com" )
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "com:hypermega:http" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "_url" )
		TS_ASSERT( cb.last_tag->related("_about", "internet_security") )
	}

    void test_evbuffer_scan(void) {
		struct evbuffer *input = evbuffer_new();
		std::string s("PUT dog _is_a animal _has legs, fur _can bark");
		evbuffer_add(input, s.c_str(), s.size());

		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagd_code tc = tagl.evbuffer_execute(input);

		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_has", "fur") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

    void test_get_tagdurl(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.tagdurl_get("/dog");
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
	}

    void test_get_tagdurl_trailing_path(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.tagdurl_get("/dog/");
		tagl.finish();
		// two path separators indicate a query
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super() , "dog" )
	}

    void test_put_tagdurl(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.tagdurl_put("/dog");
		tagl.execute("_is_a animal _has legs _can bark");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

    void test_put_tagdurl_evbuffer_body(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.tagdurl_put("/dog");

		struct evbuffer *input = evbuffer_new();

		std::string s("_is_a animal _has legs _can bark");
		evbuffer_add(input, s.c_str(), s.size());
		tagl.evbuffer_execute(input);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )

		evbuffer_free(input);
	}

    void test_tagdurl_query(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl.tagdurl_get("/animal/legs,tail");
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super() , "animal" )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )
	}

    void test_tagdurl_super_placeholder_query(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl.tagdurl_get("/*/legs,tail");
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT( cb.last_tag->super().empty() )
		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT( tagl.tag().super().empty() )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super(), "animal" )
		TS_ASSERT( it->related("legs") )
		TS_ASSERT( it->related("tail") )
	}
};
