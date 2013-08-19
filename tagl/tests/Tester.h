// Test suites

#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "tagl.h"
#include "tagspace.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_type, tagd::abstract_tag> tag_map;

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
			put_test_tag("animal", "_entity", tagd::POS_TAG);
			put_test_tag("legs", "_entity", tagd::POS_TAG);
			put_test_tag("tail", "_entity", tagd::POS_TAG);
			put_test_tag("fur", "_entity", tagd::POS_TAG);
			put_test_tag("bark", "_entity", tagd::POS_TAG);
			put_test_tag("meow", "_entity", tagd::POS_TAG);
			put_test_tag("bite", "_entity", tagd::POS_TAG);
			put_test_tag("swim", "_entity", tagd::POS_TAG);
			put_test_tag("internet_security", "_entity", tagd::POS_TAG);
			put_test_tag("_is_a", "_entity", tagd::POS_SUPER);
			put_test_tag("_about", "_entity", tagd::POS_RELATOR);
			put_test_tag("_has", "_entity", tagd::POS_RELATOR);
			put_test_tag("_can", "_entity", tagd::POS_RELATOR);
			put_test_tag("_what", "_entity", tagd::POS_INTERROGATOR);

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

		ts_res_code get(tagd::abstract_tag& t, const tagd::id_type& id) {
			tag_map::iterator it = db.find(id);
			if (it == db.end()) return TS_NOT_FOUND;

			t = it->second;
			return TS_OK;
		}

		ts_res_code put(const tagd::abstract_tag& t) {
			db[t.id()] = t;

			return TS_OK;
		}

		ts_res_code exists(const tagd::id_type&) {
			assert(false);
			return TS_ERR;
		}

		ts_res_code query(tagd::tag_set& T, const tagd::interrogator& q) {
			assert(!q.super().empty());  // can't do lookups on relations

			if (q.super() == "animal") {
				T.insert(_cat);
				T.insert(_dog);
			}

			return TS_NOT_FOUND;
		}

		ts_res_code dump(std::ostream& os = std::cout) {
			assert(false);
			return TS_ERR;
		}

		ts_res_code dump_grid(std::ostream& os = std::cout) {
			assert(false);
			return TS_ERR;
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
		ts_res_code last_code;
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
			if (D.tag() != NULL)
				*last_tag = *(D.tag());
		}
};

class Tester : public CxxTest::TestSuite {
	public:

    void test_ctor(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_INIT" )
	}

    void test_subject(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("GET dog");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )
	}

    void test_get_super_identity_error(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("GET dog _is_a animal");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
	}

    void test_subject_super_identity(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog _is_a animal");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_unknown_super_relator(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog snarfs animal");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
	}

    void test_unknown_super(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog _is_a snarfadoodle");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
	}

    void test_subject_predicate(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog _has legs");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_newline_predicate(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog\n_has legs");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_predicate_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog _has legs, tail, fur");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_newline_predicate_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("PUT dog\n_has legs, tail, fur");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_identity_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog _is_a animal\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_identity_newline_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog _has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_subject_newline_predicate_multiple_list(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog\n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_url(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT http://www.hypermega.com/a/b/c#here?x=1&y=2\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_url_dot_dash_plus_scheme(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT svn+ssh://www.hypermega.com\n"
				"_about internet_security\n"
				"\n"
				"PUT git-svn-https://www.hypermega.com\n"
				"_about internet_security\n"
				"\n"
				"PUT never.seen.a.dot.scheme://www.hypermega.com\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_multiple_statements_whitespace(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"GET dog\n"
				" \n \t\n "
				"GET cat\n"
				"\n\n"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_blank(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute("");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )
	}

    void test_blank_lines(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(" \n \t \n\n\t ");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )
	}

    void test_multiple_statements(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
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
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_multiple_parse(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite"
			);	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		tc = tagl.execute(
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

	void test_quantifiers(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
				"PUT dog _has legs 4, tail 1, fur\n"
				"_can bark, bite"
			);	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		tc = tagl.execute(
				"PUT cat _is_a animal \n"
				"_can meow, bite\n"
				"_has legs 4"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_one_line_quantifiers(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
			"PUT dog _has legs 4, tail 1 _can bark, bite"
		);	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_single_line(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
		  "PUT dog _is_a animal _has legs, tail, fur _can bark, bite; PUT cat _is_a animal _has legs, tail, fur _can meow, bite"
		);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_put_subject_unknown(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
		  "PUT snipe _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_put_subject_known(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
		  "PUT dog _is_a animal _has legs"
		);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_put_super_unknown(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.execute(
		  "PUT dog _is_a snarf _has legs"
		);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
	}

    void test_ignore_newline_eof_error(void) {
		tagspace_tester TS;
		tagl_code tc;
		TAGL::driver a(&TS);
		tc = a.execute("GET dog\n");	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		TAGL::driver b(&TS);
		tc = b.execute("PUT dog _is_a animal\n");	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		TAGL::driver c(&TS);
		tc = c.execute("PUT dog _has legs\n");	
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		TAGL::driver d(&TS);
		tc = d.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_parseln_terminator(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal;");
		tagl.finish();
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_OK" )
	}

   void test_parseln_finish(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_INIT" )
		tagl.finish();
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_OK" )
		tagl.parseln("PUT cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		// statement not terminated yet
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_INIT" )
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_OK" )
	}

    void test_parseln_no_finish(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl.parseln("PUT dog _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can bark, bite");
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_OK" )
		tagl.parseln("PUT cat _is_a animal");
		tagl.parseln("_has legs, tail, fur");
		tagl.parseln("_can meow, bite");
		TS_ASSERT_EQUALS( tagl_code_str(tagl.code()), "TAGL_OK" )
	}

	void test_callback(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite\n"
				"\n"
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		tagd::abstract_tag t;
		ts_res_code ts_rc = TS.get(t, "dog");
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

// 	void test_pragma(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute(
// 				"CMD = PUT\n"
// 				"\n"
// 				"dog _is_a animal \n"
// 				"_has legs, tail, fur\n"
// 				"_can bark, bite\n"
// 				"\n"
// 				"cat _is_a animal \n"
// 				"_has legs, tail, fur\n"
// 				"_can meow, bite"
// 			);
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
// 		TS_ASSERT( tc == tagl.code() )
// 
// 		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
// 		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
// 		TS_ASSERT( cb.last_tag->related("_has", "legs") )
// 		TS_ASSERT( cb.last_tag->related("_has", "tail") )
// 		TS_ASSERT( cb.last_tag->related("_has", "fur") )
// 		TS_ASSERT( cb.last_tag->related("_can", "meow") )
// 		TS_ASSERT( cb.last_tag->related("_can", "bite") )
// 
// 		tagd::abstract_tag t;
// 		ts_res_code ts_rc = TS.get(t, "dog");
//         TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");
// 		TS_ASSERT_EQUALS( t.id(), "dog" )
// 		TS_ASSERT_EQUALS( t.super(), "animal" )
// 		TS_ASSERT( t.related("_has", "legs") )
// 		TS_ASSERT( t.related("_has", "tail") )
// 		TS_ASSERT( t.related("_has", "fur") )
// 		TS_ASSERT( t.related("_can", "bark") )
// 		TS_ASSERT( t.related("_can", "bite") )
// 	}

	void test_callback_semicolon(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl_code tc = tagl.execute(
				"PUT dog _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can bark, bite;"
				"PUT cat _is_a animal \n"
				"_has legs, tail, fur\n"
				"_can meow, bite"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )
		TS_ASSERT( cb.last_tag->related("_has", "fur") )
		TS_ASSERT( cb.last_tag->related("_can", "meow") )
		TS_ASSERT( cb.last_tag->related("_can", "bite") )

		tagd::abstract_tag t;
		ts_res_code ts_rc = TS.get(t, "dog");
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super(), "animal" )
		TS_ASSERT( t.related("_has", "legs") )
		TS_ASSERT( t.related("_has", "tail") )
		TS_ASSERT( t.related("_has", "fur") )
		TS_ASSERT( t.related("_can", "bark") )
		TS_ASSERT( t.related("_can", "bite") )
	}

//     void test_super_test(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute("TEST dog _is_a animal");
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
// 		TS_ASSERT( cb.cmd == CMD_TEST )
// 		TS_ASSERT( cb.test_ok )
// 	}
// 
//     void test_relator_test(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute(
// 			"TEST dog _is_a animal "
// 			"_has legs, tail fur "
// 			"_can bark"
// 		);
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
// 		TS_ASSERT( cb.cmd == CMD_TEST )
// 		TS_ASSERT( cb.test_ok )
// 	}
// 
//     void test_relator_test_fail(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute(
// 			"TEST dog _is_a animal "
// 			"_has legs, tail fur "
// 			"_can swim"   // lets pretend dogs cant swim
// 		);
// 
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
// 		TS_ASSERT( cb.cmd == CMD_TEST )
// 		TS_ASSERT( !cb.test_ok )
// 	}
// 
//     void test_unknown_super_relator_test(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute("TEST dog snarfs animal");
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
// 		TS_ASSERT( cb.err_cmd == CMD_TEST )
// 		TS_ASSERT( !cb.test_ok )
// 	}
// 
//     void test_unknown_super_test(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute("TEST dog _is_a snarfadoodle");
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
// 		TS_ASSERT( cb.err_cmd == CMD_TEST )
// 		TS_ASSERT( !cb.test_ok )
// 	}
// 
//     void test_unknown_predicate_list_test_fail(void) {
// 		tagspace_tester TS;
// 		callback_tester cb(&TS);
// 		TAGL::driver tagl(&TS, &cb);
// 		tagl_code tc = tagl.execute(
// 			"TEST dog _is_a animal "
// 			"_has legs, tail fur "
// 			"blah swim"   // unknown relator
// 		);
// 
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_ERR" )
// 		TS_ASSERT( cb.err_cmd == CMD_TEST )
// 		TS_ASSERT( !cb.test_ok )
// 	}

    void test_query(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl_code tc = tagl.execute("QUERY _what _is_a animal _has legs, tail");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "animal" )
		TS_ASSERT( cb.last_tag->related("_has", "legs") )
		TS_ASSERT( cb.last_tag->related("_has", "tail") )

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
// 		tagl_code tc = tagl.execute("GET _what _is_a snipe");
// 		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
// 		TS_ASSERT( cb.err_cmd == CMD_GET )
// 	}

	void test_url_callback(void) {
		tagspace_tester TS;
		callback_tester cb(&TS);
		TAGL::driver tagl(&TS, &cb);
		tagl_code tc = tagl.execute(
				"PUT http://hypermega.com\n"
				"_about internet_security"
			);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "http://hypermega.com" )
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hdurl(), "com:hypermega:http" )
		TS_ASSERT_EQUALS( cb.last_tag->super(), "_url" )
		TS_ASSERT( cb.last_tag->related("_about", "internet_security") )
	}

    void test_scanner(void) {
		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		//TAGL::driver::trace_on("trace: ");
		tagl_code tc = tagl.execute("PUT dog _is_a animal");
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )
	}

    void test_evbuffer_scan(void) {
		struct evbuffer *input = evbuffer_new();
		std::string s("PUT dog _is_a animal _has legs, fur _can bark");
		evbuffer_add(input, s.c_str(), s.size());

		tagspace_tester TS;
		TAGL::driver tagl(&TS);
		tagl_code tc = tagl.evbuffer_execute(input);
		TS_ASSERT_EQUALS( tagl_code_str(tc), "TAGL_OK" )

		evbuffer_free(input);
	}
};
