// Test suites

// Contract: [[nodiscard]] is a C++17 attribute; this build requires it
static_assert(__has_cpp_attribute(nodiscard), "[[nodiscard]] required for TAGL::driver method contracts");

#include <cxxtest/TestSuite.h>
#include <cstdio>
#include <type_traits>
#include <unistd.h>
#include "tagl.h"
#include "tagdb.h"

#include <event2/buffer.h>

typedef std::map<tagd::id_string, tagd::abstract_tag> tag_map;
typedef tagdb::flags_t tdb_flags_t;
typedef tagdb::session tdb_session_t;

#define TS_ASSERT_TAGD_OK(EXPR) TS_ASSERT_EQUALS((EXPR), tagd::TAGD_OK)

inline constexpr std::string_view TEST_TAG_IS_A{"is_a"};
inline constexpr std::string_view TEST_TAG_TYPE_OF{"type_of"};

template <typename Part>
static void append_tagl_input(std::string& s, const Part& part) {
	s.append(part);
}

template <typename... Parts>
static std::string tagl_input(const Parts&... parts) {
	std::string s;
	(append_tagl_input(s, parts), ...);
	return s;
}

inline tagd::abstract_tag test_tag(tagd::id_view id) {
	return tagd::abstract_tag(id, TEST_TAG_IS_A, tagd::id_view{}, tagd::POS_TAG);
}

inline tagd::abstract_tag test_tag(tagd::id_view id, tagd::id_view super_object) {
	return tagd::abstract_tag(id, TEST_TAG_IS_A, super_object, tagd::POS_TAG);
}

inline tagd::abstract_tag test_tag(tagd::id_view id, tagd::id_view sub_relator, tagd::id_view super_object) {
	return tagd::abstract_tag(id, sub_relator, super_object, tagd::POS_TAG);
}

inline tagd::relator test_relator(tagd::id_view id, tagd::id_view super_object) {
	return tagd::relator(id, super_object);
}

// pure virtual interface
class tagdb_tester : public tagdb::tagdb {
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
		tagd::abstract_tag _mammal;
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
			put_test_tag("blood", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("bark", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("meow", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("bite", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("swim", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("information", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("language", "information", tagd::POS_TAG);
			put_test_tag("simple_english", "language", tagd::POS_TAG);
			put_test_tag("japanese", "language", tagd::POS_TAG);
			put_test_tag("internet_security", "information", tagd::POS_TAG);
			put_test_tag("child", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("action", HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag("fun", "action", tagd::POS_TAG);
			put_test_tag(HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_SUB_RELATOR);
			put_test_tag(TEST_TAG_IS_A, HARD_TAG_SUB, tagd::POS_SUB_RELATOR);
			put_test_tag(TEST_TAG_TYPE_OF, HARD_TAG_SUB, tagd::POS_SUB_RELATOR);
			put_test_tag("about", HARD_TAG_ENTITY, tagd::POS_RELATOR);
			put_test_tag(HARD_TAG_HAS, HARD_TAG_ENTITY, tagd::POS_RELATOR);
			put_test_tag(HARD_TAG_CAN, HARD_TAG_ENTITY, tagd::POS_RELATOR);
			put_test_tag(HARD_TAG_CAUSED_BY, HARD_TAG_RELATOR, tagd::POS_RELATOR);
			put_test_tag(HARD_TAG_INTERROGATOR, HARD_TAG_ENTITY, tagd::POS_INTERROGATOR);
			put_test_tag(HARD_TAG_WHAT, HARD_TAG_ENTITY, tagd::POS_INTERROGATOR);
			put_test_tag(HARD_TAG_TERMS, HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag(HARD_TAG_MESSAGE, HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag(HARD_TAG_EVENT, HARD_TAG_ENTITY, tagd::POS_TAG);
			put_test_tag(HARD_TAG_ERROR, HARD_TAG_EVENT, tagd::POS_TAG);
			put_test_tag(HARD_TAG_ERROR_TS_NOT_FOUND, HARD_TAG_ERROR, tagd::POS_TAG);
			put_test_tag(HARD_TAG_UNKNOWN_TAG, HARD_TAG_ERROR, tagd::POS_TAG);
			put_test_tag(HARD_TAG_REFERENT, HARD_TAG_SUB, tagd::POS_REFERENT);
			put_test_tag(HARD_TAG_REFERS, HARD_TAG_ENTITY, tagd::POS_REFERS);
			put_test_tag(HARD_TAG_REFERS_TO, HARD_TAG_ENTITY, tagd::POS_REFERS_TO);
			put_test_tag(HARD_TAG_CONTEXT, HARD_TAG_ENTITY, tagd::POS_CONTEXT);
			put_test_tag(HARD_TAG_FLAG, HARD_TAG_ENTITY, tagd::POS_FLAG);
			put_test_tag(HARD_TAG_IGNORE_DUPLICATES, HARD_TAG_FLAG, tagd::POS_FLAG);
			put_test_tag(HARD_TAG_INCLUDE, HARD_TAG_RELATOR, tagd::POS_INCLUDE);

			tagd::abstract_tag mammal = test_tag("mammal", "animal");
			// search terms
			TS_ASSERT_TAGD_OK(mammal.relation(HARD_TAG_HAS, HARD_TAG_TERMS, "warm blood"));
			db[mammal.id()] = mammal;
			_mammal = mammal;

			tagd::abstract_tag dog = test_tag("dog", "mammal");
			TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "legs"));
			TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "tail"));
			TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_HAS, "fur"));
			TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_CAN, "bark"));
			TS_ASSERT_TAGD_OK(dog.relation(HARD_TAG_CAN, "bite"));
			db[dog.id()] = dog;
			_dog = dog;

			tagd::abstract_tag cat = test_tag("cat", "mammal");
			TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "legs"));
			TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "tail"));
			TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_HAS, "fur"));
			TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_CAN, "meow"));
			TS_ASSERT_TAGD_OK(cat.relation(HARD_TAG_CAN, "bite"));
			db[cat.id()] = cat;
			_cat = cat;

			tagd::abstract_tag whale = test_tag("whale", "mammal");
			db[whale.id()] = whale;

			put_test_tag("breed", "dog", tagd::POS_TAG);

			this->code(tagd::TAGD_OK);
		}

		tagd::part_of_speech pos(tagd::id_view id, tdb_session_t * = nullptr, tdb_flags_t = tdb_flags_t()) override {
			tag_map::iterator it = db.find(tagd::id_string(id));
			if (it == db.end()) return tagd::POS_UNKNOWN;

			return it->second.pos();
		}

		tagd::code get(tagd::abstract_tag& t, tagd::id_view id, tdb_session_t * = nullptr, tdb_flags_t = tdb_flags_t()) override {
			tag_map::iterator it = db.find(tagd::id_string(id));
			if (it == db.end())
				return this->ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", std::string(id).c_str());

			t = it->second;
			return this->code(tagd::TAGD_OK);
		}

		tagd::code put(const tagd::abstract_tag& t, tdb_session_t *ssn = nullptr, tdb_flags_t = tdb_flags_t()) override {
			if (t.id()[0] == '_')
				return this->ferror(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", t.id().c_str());

			if (t.id() == t.super_object())
					return this->error(tagd::TS_MISUSE,
						(std::string("_id == ") + std::string(HARD_TAG_SUB) + " not allowed!").c_str());

			if (t.pos() == tagd::POS_UNKNOWN) {
				tagd::abstract_tag parent;
				if (this->get(parent, t.super_object(), ssn) == tagd::TAGD_OK) {
					tagd::abstract_tag cpy = t;
					cpy.pos(parent.pos());
					db[cpy.id()] = cpy;
				} else {
					return this->code();
				}
			} else if (t.pos() == tagd::POS_URL) {
				// we could make a tagd::url copy constructor/operator
				// but this is the only place where this madness occurs
				tagd::url u(t.id());
				u.relations = t.relations;
				db[u.hduri()] = u;
			} else {
				db[t.id()] = t;
			}

			return this->code(tagd::TAGD_OK);
		}

		tagd::code del(const tagd::abstract_tag& t, tdb_session_t *ssn, tdb_flags_t flags = tdb_flags_t()) override {
			if (!t.super_object().empty() && t.pos() != tagd::POS_URL) {
				return this->ferror(tagd::TS_MISUSE,
					"sub must not be specified when deleting tag: %s", t.id().c_str());
			}

			tagd::abstract_tag existing;
			tagd::id_string id;
			if (t.pos() == tagd::POS_URL) {
				tagd::url u(t.id());
				if (!u.ok())
					return this->ferror(u.code(), "del failed: %s", t.id().c_str());
				id = u.hduri();
			} else {
				id = t.id();
			}

			this->get(existing, id, ssn, flags);
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

				return this->put(existing, ssn, flags);
			}

			assert(false);  // shouldn't get here
			return this->error(tagd::TS_INTERNAL_ERR, "fix del() method");
		}

		// !!!tag_set results hard coded!!!
		tagd::code query(tagd::tag_set& T, const tagd::interrogator& q, tdb_session_t * = nullptr, tdb_flags_t = tdb_flags_t()) override {
			if ( q.super_object().empty() &&
					q.related("legs") &&
					q.related("tail") ) {
				T.insert(_cat);
				T.insert(_dog);
				return tagd::TAGD_OK;
			} else if (q.super_object() == "animal") {
				T.insert(_mammal);
				return tagd::TAGD_OK;
			} else if (q.super_object() == "mammal") {
				T.insert(_cat);
				T.insert(_dog);
				return tagd::TAGD_OK;
			}

			return tagd::TS_NOT_FOUND;
		}

		bool exists(tagd::id_view id, tdb_flags_t=tdb_flags_t()) override {
			return (db.find(tagd::id_string(id)) != db.end());
		}

		tagd::code dump(std::ostream& os = std::cout) override {
			for (auto it = db.begin(); it != db.end(); ++it) {
				os << "-- " << it->first << " , " << pos_str(it->second.pos()) << std::endl;
				os << it->second << std::endl << std::endl;
			}

			return tagd::TAGD_OK;
		}

		tagd::code dump_grid(std::ostream& = std::cout) override {
			assert(false);
			return tagd::TS_ERR;
		}

		tagd::code dump_terms(std::ostream& = std::cout) override {
			assert(false);
			return tagd::TS_ERR;
		}
};

class callback_tester : public TAGL::callback {
		tagdb::tagdb *_tdb;

		void renew_last_tag(const tagd::part_of_speech& pos = tagd::POS_TAG) {
			if (last_tag != nullptr)
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
		int cmd_error_calls;

		callback_tester(tagdb::tagdb *tdb) :
			last_code(), last_tag(nullptr), cmd_error_calls(0)  {
			_tdb = tdb;
		}

		~callback_tester() {
			if(last_tag != nullptr)
				delete last_tag;
		}

		const TAGL::driver *bound_driver() const {
			return _driver;
		}

		void cmd_get(const tagd::abstract_tag& t) {
			cmd = TOK_CMD_GET;
			if(t.pos() == tagd::POS_URL) {
				if (last_tag != nullptr)
					delete last_tag;
				auto u = new tagd::url(t.id());
				last_tag = u;
				last_code = _tdb->get(*last_tag, u->hduri(), _driver->session_ptr());
			} else {
				renew_last_tag(t.pos());
				last_code = _tdb->get(*last_tag, t.id(), _driver->session_ptr());
			}
		}

		void cmd_put(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_PUT;
			if (t.pos() == tagd::POS_URL) {
				if (last_tag != nullptr)
					delete last_tag;
				auto u = new tagd::url(t.id());
				last_tag = u;
				*last_tag = t;
				last_code = _tdb->put(*last_tag, _driver->session_ptr());
			} else {
				renew_last_tag(t.pos());
				*last_tag = t;
				last_code = _tdb->put(*last_tag, _driver->session_ptr());
			}
		}

			void cmd_del(const tagd::abstract_tag& t) {
				if (t.id() == t.super_object()) {
					last_code = _tdb->error(tagd::TS_MISUSE, "id cannot be the same as sub");
					return;
				}
				cmd = TOK_CMD_DEL;
				if (t.pos() == tagd::POS_URL) {
					if (last_tag != nullptr)
						delete last_tag;
					last_tag = new tagd::url(t.id());
					*last_tag = t;
				} else {
					renew_last_tag(t.pos());
					*last_tag = t;
				}

				last_code = _tdb->del(*last_tag, _driver->session_ptr());
			}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = TOK_CMD_QUERY;
			renew_last_tag();

			*last_tag = q;
			last_tag_set.clear();
			last_code = _tdb->query(last_tag_set, q, _driver->session_ptr());
		}

		void cmd_error() {
			cmd = _driver->cmd();
			++cmd_error_calls;
			last_code = _driver->code();

			renew_last_tag();
			if (!_driver->tag().empty())
				*last_tag = _driver->tag();
		}
};

class driver_tester : public TAGL::driver {
	public:
		driver_tester(tagdb::tagdb *tdb) : TAGL::driver(tdb) {}

		void init_parser() {
			this->init();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

	void test_execution_context_types_are_non_copyable_and_non_movable(void) {
		TS_ASSERT(!std::is_copy_constructible_v<TAGL::driver>);
		TS_ASSERT(!std::is_move_constructible_v<TAGL::driver>);
		TS_ASSERT(!std::is_copy_constructible_v<TAGL::scanner>);
		TS_ASSERT(!std::is_move_constructible_v<TAGL::scanner>);
		TS_ASSERT(!std::is_copy_constructible_v<TAGL::callback>);
		TS_ASSERT(!std::is_move_constructible_v<TAGL::callback>);
	}

    void test_ctor(void) {
		tagdb_tester tdb;
		{
			TAGL::driver tagl(&tdb);
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		}

		{
			// for test puposes only, we would never pass a nullptr TAGL::driver
			TAGL::scanner scnr(nullptr);
			TAGL::driver tagl(&tdb, &scnr);
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		}

		{
			// for test puposes only, we would never pass a nullptr as TAGL::driver*
			TAGL::scanner scnr(nullptr);
			TAGL::driver tagl(&tdb, &scnr);  // _own_scanner will be false, so _scanner not deleted
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		}

		{
			TAGL::scanner scnr(nullptr);
			callback_tester clbk(&tdb);
			TAGL::driver tagl(&tdb, &scnr, &clbk);
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		}

		{
			callback_tester clbk(&tdb);
			TAGL::driver tagl(&tdb, &clbk);
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		}
	}

	void test_const_driver_read_only_accessors(void) {
		tagdb_tester tdb;
		tdb_session_t *ssn = tdb.new_session();
		callback_tester cb(&tdb);
		{
			TAGL::driver tagl(&tdb, &cb, ssn);
			const TAGL::driver& ctagl = tagl;

			TS_ASSERT(ctagl.is_setup());
			TS_ASSERT(ctagl.tdb() == &tdb);
			TS_ASSERT(ctagl.session_ptr() == ssn);
			TS_ASSERT(ctagl.callback_ptr() == &cb);
		}
		delete ssn;
	}

	void test_text_token_views_remain_valid_after_later_stores(void) {
		TAGL::token_store store;
		const auto first = store.store_text(std::string("alpha"));
		const auto second = store.store_text(std::string("beta"));
		const char *first_ptr = first.z;
		const char *second_ptr = second.z;

		store.store_text(std::string("gamma"));
		store.store_text(std::string(256, 'x'));
		const auto later = store.store_text(std::string("delta"));

		TS_ASSERT(first_ptr != nullptr);
		TS_ASSERT(second_ptr != nullptr);
		TS_ASSERT_EQUALS(first.z, first_ptr);
		TS_ASSERT_EQUALS(second.z, second_ptr);
		TS_ASSERT_EQUALS(std::string(first.z, first.n), "alpha");
		TS_ASSERT_EQUALS(std::string(second.z, second.n), "beta");
		TS_ASSERT_EQUALS(std::string(later.z, later.n), "delta");
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
		tagd::code tc = tagl.execute(tagl_input("<< dog ", TEST_TAG_IS_A, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_subject_sub_identity(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().sub_relator() , TEST_TAG_IS_A )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
	}

    void test_unknown_sub_relator(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog snarfs mammal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_unknown_super_object(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog ", TEST_TAG_IS_A, " snarfadoodle"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
	}

    void test_sub_relator_symbol(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog -^ mammal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		if (tagl.has_errors())
			tagl.print_errors();
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().sub_relator() , HARD_TAG_SUB )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
	}

    void test_sub_relator_object_symbol_error(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> super -^ -^");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_sub_relator_object_symbol_sub_relator(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> subordinate -^ ", HARD_TAG_SUB));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "subordinate" )
		TS_ASSERT_EQUALS( tagl.tag().sub_relator() , HARD_TAG_SUB )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_SUB )
	}

    void test_relator_symbol(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog -> tail");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_RELATOR, "tail") )
	}

    void test_relator_object_symbol_error(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(">> dog -> ->");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_subject_predicate(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog ", HARD_TAG_HAS, " legs"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
	}

    void test_subject_newline_predicate(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog\n", HARD_TAG_HAS, " legs"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
	}

    void test_subject_predicate_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog ", HARD_TAG_HAS, " legs, tail, fur"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
	}

    void test_subject_newline_predicate_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> dog\n", HARD_TAG_HAS, " legs, tail, fur"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
	}

    void test_subject_identity_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal\n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

    void test_subject_identity_newline_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

    void test_subject_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

    void test_subject_newline_predicate_multiple_list(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog\n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_dash_modifier(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> shar_pei ", TEST_TAG_IS_A, " dog\n",
				HARD_TAG_HAS, " breed = Shar-Pei"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_url_modifier(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.execute(tagl_input(
				">> shar_pei ", TEST_TAG_IS_A, " dog\n",
				HARD_TAG_HAS, " breed = http://www.dogbreedinfo.com/sharpei.htm"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
	}

	void test_url_list(void) {
		tagdb_tester tdb;
		TS_ASSERT_TAGD_OK(tdb.put(test_tag("simple", TEST_TAG_IS_A, "concept")));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("Shar Pei", "shar_pei", "simple")));
		TS_ASSERT_TAGD_OK(tdb.put(tagd::referent("SHARPEI","shar_pei","code")));
		TAGL::driver tagl(&tdb);
		tagl.execute(tagl_input(
				">> shar_pei ", TEST_TAG_IS_A, " dog\n",
				HARD_TAG_HAS, " information = http://www.dogbreedinfo.com/sharpei.htm, breed = http://www.akc.org/breeds/chinese_shar_pei/index.cfm, SHARPEI, \"Shar Pei\""
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
	}

	void test_delete_sub_not_allowed(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				"!! dog ", TEST_TAG_IS_A, " mammal\n",
				HARD_TAG_CAN, " bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_delete(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input(
				"!! dog\n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_DEL )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

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

	void test_delete_missing_calls_cmd_error_once(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.execute("!! badger;");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( cb.cmd_error_calls, 1 )
		TS_ASSERT_DIFFERS( cb.last_tag, nullptr )
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

    void test_event_error_uri(void) {
		const char *evuri = "ev:2026-04-09T04:00:56.738Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!1!_event";
		const char *erruri = "err:2026-04-09T04:00:56.739Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!2!_error:ts_not_found";
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(
				std::string(">> ").append(evuri).append("\n")
				.append("about dog, ").append(erruri)
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , evuri )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_EVENT )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_TAG )
		TS_ASSERT( tagl.tag().related("about", "dog") )
		TS_ASSERT( tagl.tag().related("about", erruri) )

		tc = tagl.execute(
				std::string(">> ").append(erruri).append("\n")
				.append("about dog, ").append(evuri)
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , erruri )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_ERROR )
		TS_ASSERT( tagl.tag().related("about", "dog") )
		TS_ASSERT( tagl.tag().related("about", evuri) )
	}

    void test_printed_error_is_tagl(void) {
		tagd::errorable R;
		R.ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", "doggy");
		R.last_error_relation(tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "doggy"));

		std::stringstream ss;
		R.print_errors(ss);

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(ss.str());
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT( tagl.tag().id().find("err:") == 0 )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "doggy") )
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
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite\n",
				"\n",
				"<< dog\n",
				"\n",
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " meow, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

	}

	void testconstrain_tag_id_consistent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.constrain_tag_id = "dog";
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				"\n",
				">> dog\n",
				HARD_TAG_CAN, " bark, bite\n"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void testconstrain_tag_id_inconsistent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.constrain_tag_id = "dog";
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				"\n",
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " meow, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
	}

    void test_multiple_parse(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		tc = tagl.execute(tagl_input(
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " meow, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_quantifiers(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", HARD_TAG_HAS, " legs= 4, tail = 1, fur\n",
				HARD_TAG_CAN, " bark, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs", "4") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail", "1") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		tc = tagl.execute(tagl_input(
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_CAN, " meow, bite\n",
				HARD_TAG_HAS, " legs =4"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs", "4") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_one_line_quantifiers(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
			">> dog ", HARD_TAG_HAS, " legs = 4, tail = 1 ", HARD_TAG_CAN, " bark, bite"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs", "4") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail", "1") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

	}

    void test_single_line(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
		  ">> dog ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs, tail, fur ", HARD_TAG_CAN, " bark, bite; >> cat ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs, tail, fur ", HARD_TAG_CAN, " meow, bite"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		// tag only holds last tag statement
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_put_hard_tag(void) {
		tagdb_tester tdb;
		auto tc = tdb.put(test_tag("_my_hard_tag", TEST_TAG_IS_A, "_entity"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tdb.code()), "TS_MISUSE" )
	}

    void test_put_subject_unknown(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
		  ">> snipe ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "snipe" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
	}

    void test_put_subject_known(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
		  ">> dog ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
	}

    void test_put_sub_unknown(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
		  ">> dog ", TEST_TAG_IS_A, " snarf ", HARD_TAG_HAS, " legs"
		));
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
		tc = b.execute(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal\n"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( b.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( b.tag().id() , "dog" )
		TS_ASSERT_EQUALS( b.tag().super_object() , "mammal" )

		TAGL::driver c(&tdb);
		tc = c.execute(tagl_input(">> dog ", HARD_TAG_HAS, " legs\n"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( c.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( c.tag().id() , "dog" )
		TS_ASSERT( c.tag().related(HARD_TAG_HAS, "legs") )

		TAGL::driver d(&tdb);
		tc = d.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite\n"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( d.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( d.tag().id() , "dog" )
		TS_ASSERT_EQUALS( d.tag().super_object() , "mammal" )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( d.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( d.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_comment(void) {
		tagdb_tester tdb;
		tagd::code tc;

		TAGL::driver d(&tdb);
		tc = d.execute(tagl_input(
			"-- this is a comment\n",
			">> dog ", TEST_TAG_IS_A, " mammal --so is this\n",
			HARD_TAG_HAS, " legs, tail-- no space between token and comment\n",
			HARD_TAG_HAS, "-* i'm a block comment *-fur\n",
			"--", HARD_TAG_CAN, " bark, bite"
		));
		if (d.has_errors())
			d.print_errors();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( d.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( d.tag().id() , "dog" )
		TS_ASSERT_EQUALS( d.tag().super_object() , "mammal" )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( d.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( !d.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( !d.tag().related(HARD_TAG_CAN, "bite") )

		tc = d.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal -* i'm a block comment\n",
				"and I don't end"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

    void test_parseln_terminator(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal;"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
	}

   void test_parseln_finish(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " legs, tail, fur"));
		tagl.parseln(tagl_input(HARD_TAG_CAN, " bark, bite"));
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		tagl.parseln(tagl_input(">> cat ", TEST_TAG_IS_A, " mammal"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " legs, tail, fur"));
		tagl.parseln(tagl_input(HARD_TAG_CAN, " meow, bite"));
		// statement not terminated yet
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

    void test_parseln_no_finish(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagl.parseln(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " legs, tail, fur"));
		tagl.parseln(tagl_input(HARD_TAG_CAN, " bark, bite"));
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		tagl.parseln(tagl_input(">> cat ", TEST_TAG_IS_A, " mammal"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " legs, tail, fur"));
		tagl.parseln(tagl_input(HARD_TAG_CAN, " meow, bite"));
		tagl.parseln();  // end of input
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )
	}

	void test_parser_reinit_after_finish(void) {
		// Verifies the init/free/reinit cycle: finish() releases the parser;
		// a subsequent execute() re-initializes it and produces correct output.
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);

		tagd::code tc = tagl.execute(tagl_input(">> dog ", TEST_TAG_IS_A, " mammal;"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )

		tagl.finish();  // frees _parser; driver is now in a reset state

		tc = tagl.execute(tagl_input(">> cat ", TEST_TAG_IS_A, " mammal;"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id(), "cat" )
	}

	void test_quotes(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input(
			">> communication ", TEST_TAG_IS_A, " _entity;\n",
			">> information ", TEST_TAG_IS_A, " communication;\n",
			">> content ", TEST_TAG_IS_A, " information;\n",
			">> message ", TEST_TAG_IS_A, " information;\n",
			">> title ", TEST_TAG_IS_A, " content;"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(tagl_input(
				">> my_title ", TEST_TAG_IS_A, " title\n",
				HARD_TAG_HAS, " message = \"my title!\";"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "message", "my title!") )

		tc = tagl.execute(tagl_input(
				">> my_quoted_title ", TEST_TAG_IS_A, " title\n",
				HARD_TAG_HAS, " message = \"my \\\"quoted\\\" title!\";"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_quoted_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "message", "my \\\"quoted\\\" title!") )
	}

	void test_quotes_parseln(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input(
			">> communication ", TEST_TAG_IS_A, " _entity;\n",
			">> information ", TEST_TAG_IS_A, " communication;\n",
			">> content ", TEST_TAG_IS_A, " information;\n",
			">> message ", TEST_TAG_IS_A, " information;\n",
			">> title ", TEST_TAG_IS_A, " content;"
		));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tagl.parseln(tagl_input(">> my_title ", TEST_TAG_IS_A, " title"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " message = \"my title!\";"));
		tagl.finish();
		TS_ASSERT( !tagl.has_errors() )
		if (tagl.has_errors()) tagl.print_errors();
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "message", "my title!") )

		tagl.parseln(tagl_input(">> my_quoted_title ", TEST_TAG_IS_A, " title"));
		tagl.parseln(tagl_input(HARD_TAG_HAS, " message = \"my \\\"quoted\\\" title!\";"));
		tagl.finish();
		TS_ASSERT( !tagl.has_errors() )
		if (tagl.has_errors()) tagl.print_errors();
		TS_ASSERT_EQUALS( cb.last_tag->id(), "my_quoted_title" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "title" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "message", "my \\\"quoted\\\" title!") )
	}

	void test_callback(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite\n",
				"\n",
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " meow, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_CAN, "bite") )

		tagd::abstract_tag t;
		tc = tdb.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super_object(), "mammal" )
		TS_ASSERT( t.related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( t.related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( t.related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( t.related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( t.related(HARD_TAG_CAN, "bite") )
	}

	void test_callback_semicolon(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input(
				">> dog ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " bark, bite;",
				">> cat ", TEST_TAG_IS_A, " mammal \n",
				HARD_TAG_HAS, " legs, tail, fur\n",
				HARD_TAG_CAN, " meow, bite"
			));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tc == tagl.code() )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "cat" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_CAN, "bite") )

		TS_ASSERT_EQUALS( tagl.tag().id() , "cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "meow") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bite") )

		tagd::abstract_tag t;
		tc = tdb.get(t, "dog");
        TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( t.id(), "dog" )
		TS_ASSERT_EQUALS( t.super_object(), "mammal" )
		TS_ASSERT( t.related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( t.related(HARD_TAG_HAS, "tail") )
		TS_ASSERT( t.related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( t.related(HARD_TAG_CAN, "bark") )
		TS_ASSERT( t.related(HARD_TAG_CAN, "bite") )
	}

	void test_scanner_and_parser_debug_logging_in_process(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		std::stringstream log_ss;
		tagd::logger log(log_ss);

		log.level(tagd::log_level::EMERGENCY);
		log.level(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG);
		log.level(HARD_TAG_ROLE_PARSER, tagd::log_level::DEBUG);
		TAGL_SET_LOGGER(&log);

		tagd::code tc = tagl.execute("<< dog;");
		TAGL_SET_LOGGER(nullptr);

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_DIFFERS(log_ss.str().find("-- scanner begin"), std::string::npos)
		TS_ASSERT_DIFFERS(log_ss.str().find("-- parser input"), std::string::npos)
	}

	void test_put_referent_no_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		// no context
		tagd::code tc = tagl.execute(tagl_input(">> doggy ", HARD_TAG_REFERS_TO, " dog"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	// TODO tagdb_tester won't allow this, but tagl parser will
	// void test_put_referent_self(void) {
	// 	tagdb_tester tdb;
	// 	TAGL::driver tagl(&tdb);
	// 	tagd::code tc = tagl.execute(">> dog " HARD_TAG_REFERS_TO " dog");
	// 	TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	// }

	void test_put_referent_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> doggy ", HARD_TAG_REFERS_TO, " dog ", HARD_TAG_CONTEXT, " child"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_put_referent_referent(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(
			">> refers_to ", HARD_TAG_REFERS_TO, " ", HARD_TAG_REFERS_TO,
			" ", HARD_TAG_CONTEXT, " simple_english" ));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_put_utf8_subject(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> イヌ ", TEST_TAG_IS_A, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
	}

	void test_put_utf8_referent_context(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(tagl_input(">> イヌ ", HARD_TAG_REFERS_TO, " dog ", HARD_TAG_CONTEXT, " japanese"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
	}

	void test_query_utf8_referent_label(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		TS_ASSERT_EQUALS(
			TAGD_CODE_STRING(tagl.execute(tagl_input(">> イヌ ", HARD_TAG_REFERS_TO, " dog ", HARD_TAG_CONTEXT, " japanese"))),
			"TAGD_OK"
		)
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers イヌ ", HARD_TAG_CONTEXT, " japanese"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos(), tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), HARD_TAG_REFERENT )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "イヌ") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "japanese") )
	}

	void test_set_context(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);
		tagd::code tc;

		// test tagdb_tester
		TS_ASSERT( tdb.exists("child") )
		TS_ASSERT_EQUALS( ssn.context().size() , 0 )
		tc = ssn.push_context("child");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() > 0 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "child" )
		ssn.clear_context();
		TS_ASSERT_EQUALS( ssn.context().size() , 0 )

		tc = tagl.execute(tagl_input("%% ", HARD_TAG_CONTEXT, " blah"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
		tagl.clear_errors();
		TS_ASSERT_EQUALS( ssn.context().size() , 0 )

		tc = ssn.push_context("child");
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		tc = tagl.execute(tagl_input("%% ", HARD_TAG_CONTEXT, " mammal, action"));
		// tagl::finish() should have popped {mammal, action} and left child intact
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() > 0 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "child" )
	}

	void test_set_context_missing_calls_cmd_error_once(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb, &ssn);

		tagd::code tc = tagl.execute(tagl_input("%% ", HARD_TAG_CONTEXT, " haha"));

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( cb.cmd_error_calls, 1 )
		TS_ASSERT_EQUALS( ssn.context().size(), 0 )
	}

	void test_set_context_existing_tag(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);

		tagd::code tc = tagl.parseln(tagl_input("%% ", HARD_TAG_CONTEXT, " simple_english;"));

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size(), 1 )
		if ( ssn.context().size() == 1 )
			TS_ASSERT_EQUALS( ssn.context()[0], "simple_english" )
	}

	void test_set_context_existing_tag_shell_style(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);
		tagd::code tc = tagl.parseln(tagl_input("%% ", HARD_TAG_CONTEXT, " simple_english"));

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.parseln();  // end of input

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size(), 1 )
		if ( ssn.context().size() == 1 )
			TS_ASSERT_EQUALS( ssn.context()[0], "simple_english" )
	}

	void test_set_context_existing_tag_list(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);

		tagd::code tc = tagl.parseln(tagl_input("%% ", HARD_TAG_CONTEXT, " simple_english, japanese;"));

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size(), 2 )
		if ( ssn.context().size() == 2 ) {
			TS_ASSERT_EQUALS( ssn.context()[0], "simple_english" )
			TS_ASSERT_EQUALS( ssn.context()[1], "japanese" )
		}
	}

	void test_set_context_without_hard_tag_is_unknown_tag(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.execute("%% context animal");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		TS_ASSERT_EQUALS( cb.cmd_error_calls, 1 )
	}

	void test_set_blank_context(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);
		TS_ASSERT_TAGD_OK(ssn.push_context("mammal"));

		tagd::code tc = tagl.execute(tagl_input("%% ", HARD_TAG_CONTEXT, " child"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() > 0 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "mammal" )

		tc = tagl.execute(tagl_input("%% ", HARD_TAG_CONTEXT, " \"\""));
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() > 0 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "mammal" )
	}

	void test_set_ignore_duplicates(void) {
		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		TS_ASSERT( tagl.flags != tagdb::F_IGNORE_DUPLICATES )

		tagd::code tc = tagl.execute(tagl_input("%% ", HARD_TAG_IGNORE_DUPLICATES, " 1"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags == tagdb::F_IGNORE_DUPLICATES )

		tc = tagl.execute(tagl_input("%% ", HARD_TAG_IGNORE_DUPLICATES, " 0"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags != tagdb::F_IGNORE_DUPLICATES )

		tc = tagl.execute(tagl_input("%% ", HARD_TAG_IGNORE_DUPLICATES, " 5"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.flags == tagdb::F_IGNORE_DUPLICATES )
	}

	void test_set_context_list(void) {
		tagdb_tester tdb;
		auto ssn = tdb.get_session();
		TAGL::driver tagl(&tdb, &ssn);

		TS_ASSERT_TAGD_OK(ssn.push_context("living_thing"));
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() == 1 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "living_thing" )

		tagd::code tc = tagl.parseln(tagl_input("%% ", HARD_TAG_CONTEXT, " child, mammal, fun;"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size() , 4 )
		if ( ssn.context().size() == 4 ) {
			TS_ASSERT_EQUALS( ssn.context()[0] , "living_thing" )
			TS_ASSERT_EQUALS( ssn.context()[1] , "child" )
			TS_ASSERT_EQUALS( ssn.context()[2] , "mammal" )
			TS_ASSERT_EQUALS( ssn.context()[3] , "fun" )
		}

		tc = tagl.parseln(tagl_input("%% ", HARD_TAG_CONTEXT, " \"\";"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() == 1 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "living_thing" )

		tagl.finish();
		TS_ASSERT_EQUALS( ssn.context().size() , 1 )
		if ( ssn.context().size() == 1 )
			TS_ASSERT_EQUALS( ssn.context()[0] , "living_thing" )
	}

    void test_query(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs, tail"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "tail") )

		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_WHAT )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "tail") )

		TS_ASSERT_EQUALS( cb.last_tag_set.size() , 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )

		tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs > 3, tail"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( cb.last_tag->related(HARD_TAG_HAS, "tail") )
	}

    void test_query_children(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
	}

    void test_query_children_empty(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " dog"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )  // TAGL statment ok
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )  // cmd_query returned TS_NOT_FOUND
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "dog" )
		TS_ASSERT( cb.last_tag_set.size() == 0 );
	}

    void test_query_wildcard_relator(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " mammal * legs, tail"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), "mammal" )

		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )
	}

    void test_query_no_sub_wildcard_relator(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " * legs, tail"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT( cb.last_tag->super_object().empty() )

		TS_ASSERT( cb.last_tag->related("legs") )
		TS_ASSERT( cb.last_tag->related("tail") )

		TS_ASSERT( cb.last_tag_set.size() == 2 );
		tagd::tag_set::iterator it = cb.last_tag_set.begin();
		TS_ASSERT_EQUALS( it->id(), "cat" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )

		it++;
		TS_ASSERT_EQUALS( it->id(), "dog" )
		TS_ASSERT_EQUALS( it->super_object(), "mammal" )
		TS_ASSERT( it->related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( it->related(HARD_TAG_HAS, "tail") )
	}

    void test_query_not_found(void) {
 		tagdb_tester tdb;
 		callback_tester cb(&tdb);
 		TAGL::driver tagl(&tdb, &cb);
 		tagd::code tc = tagl.execute(tagl_input("<< ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " snipe"));
		// TODO why not NOT_FOUND?
 		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
 		TS_ASSERT_EQUALS( cb.last_tag->pos() , tagd::POS_INTERROGATOR )
 	}

    void test_query_tag(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		// search string must be in double quotes
		tagd::code tc = tagl.execute("?? dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_query_tag_cleanup_then_get_subject(void) {
		{
			tagdb_tester tdb;
			callback_tester cb(&tdb);
			TAGL::driver tagl(&tdb, &cb);
			tagd::code tc = tagl.execute("?? dog;");
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
		}

		{
			tagdb_tester tdb;
			TAGL::driver tagl(&tdb);
			tagd::code tc = tagl.execute("<< dog;");
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
			TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		}
	}

	void test_query_tag_cleanup_after_utf8_flows(void) {
		{
			tagdb_tester tdb;
			TAGL::driver tagl(&tdb);
			TS_ASSERT_EQUALS(
				TAGD_CODE_STRING(tagl.execute(tagl_input(">> イヌ ", HARD_TAG_REFERS_TO, " dog ", HARD_TAG_CONTEXT, " japanese"))),
				"TAGD_OK"
			)
		}

		{
			tagdb_tester tdb;
			TAGL::driver tagl(&tdb);
			TS_ASSERT_EQUALS(
				TAGD_CODE_STRING(tagl.execute(tagl_input(">> イヌ ", HARD_TAG_REFERS_TO, " dog ", HARD_TAG_CONTEXT, " japanese"))),
				"TAGD_OK"
			)
			TS_ASSERT_EQUALS(
				TAGD_CODE_STRING(tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers イヌ ", HARD_TAG_CONTEXT, " japanese"))),
				"TAGD_OK"
			)
		}

		{
			tagdb_tester tdb;
			callback_tester cb(&tdb);
			TAGL::driver tagl(&tdb, &cb);
			tagd::code tc = tagl.execute("?? dog;");
			TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
		}
	}

    void test_search(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute("?? \"dog\";");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

    void test_tag_sub_search(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);
		tagd::code tc = tagl.execute(
			"?? _interrogator -^ mammal -> _terms = \"warm blood\";" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_get_subject_search_terms_token_sequence(void) {
		tagdb_tester tdb;
		driver_tester tagl(&tdb);

		tagl.init_parser();
		tagl.parse_tok(TOK_CMD_GET, TAGL::EMPTY_VALUE);
		tagl.parse_tok(TOK_TAG, "animal");
		tagl.parse_tok(TOK_RELATOR, std::string(HARD_TAG_HAS));
		tagl.parse_tok(TOK_TAG, std::string(HARD_TAG_TERMS));
		tagl.parse_tok(TOK_EQ, TAGL::EMPTY_VALUE);
		tagl.parse_tok(TOK_QUOTED_STR, "warm blood");
		tagl.parse_tok(TOK_TERMINATOR, TAGL::EMPTY_VALUE);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_ERR" )
	}

	void test_query_referents(void) {
		tagdb_tester tdb;

		// a new tagdb::session will be created internally in parser.y upon pushing a context
		TAGL::driver tagl(&tdb);

		TS_ASSERT( tagl.session_ptr() == nullptr )

		// refers.empty() && refers_to.empty() && context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", TEST_TAG_IS_A, " ", HARD_TAG_REFERENT));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 0 )

		// refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", HARD_TAG_CONTEXT, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), HARD_TAG_REFERENT )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "mammal") )

		// refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", HARD_TAG_REFERS_TO, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "mammal") )

		// refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " ", HARD_TAG_REFERS_TO, " mammal ", HARD_TAG_CONTEXT, " living_thing"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "mammal") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )

		// !refers.empty() && refers_to.empty() && context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 1 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "mammal") )

		// !refers.empty() && refers_to.empty() && !context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers thing ", HARD_TAG_CONTEXT, " living_thing"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CONTEXT, "living_thing") )

		// !refers.empty() && !refers_to.empty() && context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers thing ", HARD_TAG_REFERS_TO, " mammal"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 2 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "mammal") )

		// !refers.empty() && !refers_to.empty() && !context.empty()
		tagl.execute(tagl_input("?? ", HARD_TAG_WHAT, " _refers thing ", HARD_TAG_REFERS_TO, " mammal ", HARD_TAG_CONTEXT, " living_thing"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().relations.size() , 3 )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS, "thing") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_REFERS_TO, "mammal") )
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
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "hd:com!hypermega!!!!!!!!http" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), HARD_TAG_URL )
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
		TS_ASSERT_EQUALS( ((tagd::url*)cb.last_tag)->hduri(), "hd:com!hypermega!!!!!!!!http" )
		TS_ASSERT_EQUALS( cb.last_tag->super_object(), HARD_TAG_URL )
		TS_ASSERT( cb.last_tag->related("about", "internet_security") )
	}

	void test_put_del_get_url(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		TS_ASSERT_TAGD_OK(tdb.put(test_relator("powered_by",HARD_TAG_ENTITY)));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tdb.code()), "TAGD_OK" )
		TS_ASSERT_TAGD_OK(tdb.put(test_tag("wikimedia",HARD_TAG_ENTITY)));
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
		tagl.print_errors();
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
		const char *hduri = "hd:org!wikipedia!en!/wiki/-99Dog!!!!!!https";
		tc = tagl.execute( std::string("<< ").append(hduri) );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/-99Dog" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( static_cast<tagd::url *>(cb.last_tag)->hduri(), hduri )
		TS_ASSERT( cb.last_tag->related("about", "dog") )

		tc = tagl.execute( std::string("!! ").append(hduri) );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		// TODO fix not_found_context_dichotomy (see parser.y)
		//tc = tagl.execute( "<< https://en.wikipedia.org/wiki/-99Dog ;" );
		//TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		tagl.execute( "<< https://en.wikipedia.org/wiki/-99Dog ;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )

		tc = tagl.execute( std::string("<< ").append(hduri) );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
	}

	void test_constrain_hduri(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		const std::string hduri{"hd:org!wikipedia!en!/wiki/Dog!!!!!!https"};
		tagl.constrain_tag_id = hduri;
		tagd::code tc = tagl.execute(
				">> https://en.wikipedia.org/wiki/Dog\n"
				"about dog\n"
			);
		tagl.print_errors();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}


	void test_uri_put_get_semicolon(void) {
		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		tagd::code tc = tagl.execute(tagl_input(">> myuri:dog ", TEST_TAG_IS_A, " _entity;"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(
				">> https://en.wikipedia.org/wiki/Dog\n"
				"about myuri:dog;\n"
			);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		const char *hduri = "hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		tc = tagl.execute(std::string("<< ").append(hduri));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		TS_ASSERT_EQUALS( cb.last_tag->id(), "https://en.wikipedia.org/wiki/Dog" )
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( static_cast<tagd::url *>(cb.last_tag)->hduri(), hduri )
		TS_ASSERT( cb.last_tag->related("about", "myuri:dog") )

		tc = tagl.execute(std::string("!! ").append(hduri));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		// TODO fix not_found_context_dichotomy (see parser.y)
		//tc = tagl.execute( "<< https://en.wikipedia.org/wiki/Dog ;" );
		//TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
		tagl.execute( "<< https://en.wikipedia.org/wiki/Dog;" );
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )

		tc = tagl.execute(std::string("<< ").append(hduri));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(cb.last_code), "TS_NOT_FOUND" )
	}

    void test_evbuffer_scan(void) {
		struct evbuffer *input = evbuffer_new();
		const std::string s = tagl_input(">> dog ", TEST_TAG_IS_A, " mammal ", HARD_TAG_HAS, " legs, fur ", HARD_TAG_CAN, " bark");
		evbuffer_add(input, s.c_str(), s.size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);

		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "legs") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, "fur") )
		TS_ASSERT( tagl.tag().related(HARD_TAG_CAN, "bark") )
	}

	void test_quoted_str_size_of_buf(void) {
		std::stringstream ss, ss1;
		ss << tagl_input(">> my_message1 ", TEST_TAG_IS_A, " _entity\n")
		   << tagl_input(HARD_TAG_HAS, " ", HARD_TAG_MESSAGE, " = ");

		// fill buf
		auto sz = ss.str().size();
		while ( sz++ < TAGL::BUF_SZ )
			ss << ' ';

		// trigger a fill so that a quoted string
		// completely fills the buffer
		ss1 << '"';
		sz = ss1.str().size();
		while ( sz++ < TAGL::BUF_SZ - 1 )
			ss1 << (char)(sz % 10 + 48);  // ascii 0-9
		ss1 << '"';

		ss << ss1.str();
		ss << "\n\n";
		ss << tagl_input(">> my_message2 ", TEST_TAG_IS_A, " _entity\n")
		   << tagl_input(HARD_TAG_HAS, " ", HARD_TAG_MESSAGE, " = \"another message\"");

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
		ss << tagl_input(">> my_message1 ", TEST_TAG_IS_A, " _entity\n")
		   << tagl_input(HARD_TAG_HAS, " ", HARD_TAG_MESSAGE, " = \"");

		auto sz = ss.str().size();
		while ( sz++  < (TAGL::BUF_SZ * 3) )
			ss << (char)(sz % 10 + 48);  // ascii 0-9

		ss << "\"\n\n";
		ss << tagl_input(">> my_message2 ", TEST_TAG_IS_A, " _entity\n")
		   << tagl_input(HARD_TAG_HAS, " ", HARD_TAG_MESSAGE, " = \"another message\"");

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_tag_token_larger_than_buf(void) {
		std::stringstream ss;
		std::string id;

		while ( id.size() < (TAGL::BUF_SZ * 3) )
			id.push_back((char)(id.size() % 10 + 97));  // ascii a-j

		ss << ">> " << id << " " << TEST_TAG_IS_A << " mammal;";

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , id )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "mammal" )
	}

	void test_tagl_file_token_survives_later_scanner_activity(void) {
		std::stringstream ss;
		std::string path = "tagl_include_lifetime_" + std::to_string(getpid()) + ".tagl";
		FILE *fp = fopen(path.c_str(), "w");
		TS_ASSERT(fp != nullptr)
		if (fp == nullptr)
			return;
		fclose(fp);

		ss << "%% " << HARD_TAG_INCLUDE << " " << path;
		for (size_t i = 0; i < TAGL::BUF_SZ * 2; ++i)
			ss << ' ';
		ss << ';';

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);
		remove(path.c_str());

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
	}

	void test_include_file_restores_callback_binding(void) {
		std::string path = "tagl_include_callback_" + std::to_string(getpid()) + ".tagl";
		FILE *fp = fopen(path.c_str(), "w");
		TS_ASSERT(fp != nullptr)
		if (fp == nullptr)
			return;
		fputs(">> dog ", fp);
		fputs(TEST_TAG_IS_A.data(), fp);
		fputs(" mammal;\n", fp);
		fclose(fp);

		tagdb_tester tdb;
		callback_tester cb(&tdb);
		TAGL::driver tagl(&tdb, &cb);

		TS_ASSERT_EQUALS(cb.bound_driver(), &tagl)

		tagd::code tc = tagl.include_file(path);
		remove(path.c_str());

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS(cb.bound_driver(), &tagl)
	}

	void test_tag_token_survives_later_scanner_activity(void) {
		std::stringstream ss;
		ss << "<< dog";
		for (size_t i = 0; i < TAGL::BUF_SZ * 2; ++i)
			ss << ' ';
		ss << ';';

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
	}

	void test_uri_token_survives_later_scanner_activity(void) {
		std::stringstream ss;
		const std::string uri{"https://en.wikipedia.org/wiki/Dog"};
		ss << "<< " << uri;
		for (size_t i = 0; i < TAGL::BUF_SZ * 2; ++i)
			ss << ' ';
		ss << ';';

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(std::string(">> ").append(uri).append("\nabout internet_security"));
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )

		tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , uri )
		TS_ASSERT_EQUALS( tagl.tag().pos() , tagd::POS_URL )
	}

	// Contract 2: own_session() transfers session ownership to driver; destructor deletes it
	void test_own_session_destructor_releases_session(void) {
		tagdb_tester tdb;
		{
			TAGL::driver tagl(&tdb);
			tagl.own_session(tdb.new_session());
			// session owned by tagl; destructor at } deletes it without crash or double-free
		}
		TS_ASSERT(true);
	}

	void test_quoted_string_token_survives_later_scanner_activity(void) {
		std::stringstream ss;
		ss << tagl_input(">> my_message ", TEST_TAG_IS_A, " _entity\n")
		   << tagl_input(HARD_TAG_HAS, " ", HARD_TAG_MESSAGE, " = \"hello world\"");
		for (size_t i = 0; i < TAGL::BUF_SZ * 2; ++i)
			ss << ' ';
		ss << ';';

		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, ss.str().c_str(), ss.str().size());

		tagdb_tester tdb;
		TAGL::driver tagl(&tdb);
		tagd::code tc = tagl.execute(input);
		evbuffer_free(input);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "my_message" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "hello world") )
	}
};
