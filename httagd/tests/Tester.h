#include <cxxtest/TestSuite.h>
#include <cstdio>
#include <memory>
#include <type_traits>
#include "tagl.h"
#include "tagspace.h"
#include "httagd.h"

#include <event2/buffer.h>

template<typename Viewspace>
concept accepts_temporary_view_result = requires(Viewspace& views, const httagd::view_id& id) {
	views.get(httagd::view{}, id);
};

typedef std::map<tagd::id_string, tagd::abstract_tag> tag_map;
typedef tagd::flags_t tsp_flags_t;
typedef tagd::tagspace_session tsp_sess_t;

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
		tagd::part_of_speech pos(tagd::id_view, tsp_sess_t*, tsp_flags_t=tsp_flags_t()) override;
		tagd::code get(tagd::abstract_tag&, tagd::id_view, tsp_sess_t*, tsp_flags_t=tsp_flags_t()) override;
		tagd::code put(const tagd::abstract_tag&, tsp_sess_t*, tsp_flags_t=tsp_flags_t());
		tagd::code del(const tagd::abstract_tag&, tsp_sess_t*, tsp_flags_t=tsp_flags_t());
		bool exists(tagd::id_view, tsp_flags_t=tsp_flags_t()) const override;
		tagd::code query(tagd::tag_set&, const tagd::interrogator&, tsp_sess_t*, tsp_flags_t=tsp_flags_t()) override;

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

tagd::part_of_speech tagspace_tester::pos(tagd::id_view id, tsp_sess_t*, tsp_flags_t) {
	tag_map::iterator it = db.find(tagd::id_string(id));
	if (it == db.end()) return tagd::POS_UNKNOWN;

	return it->second.pos();
}

tagd::code tagspace_tester::get(tagd::abstract_tag& t, tagd::id_view id, tsp_sess_t*, tsp_flags_t) {
	tag_map::iterator it = db.find(tagd::id_string(id));
	if (it == db.end()) return this->ferror(tagd::TS_NOT_FOUND, "unknown tag: %s", std::string(id).c_str());

	t = it->second;
	return this->code(tagd::TAGD_OK);
}

tagd::code tagspace_tester::put(const tagd::abstract_tag& t, tsp_sess_t*, tsp_flags_t) {
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

tagd::code tagspace_tester::del(const tagd::abstract_tag& t, tsp_sess_t*, tsp_flags_t) {
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

bool tagspace_tester::exists(tagd::id_view id, tsp_flags_t) const {
	return (db.find(tagd::id_string(id)) != db.end());
}

tagd::code tagspace_tester::query(tagd::tag_set& T, const tagd::interrogator& q, tsp_sess_t*, tsp_flags_t) {
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
		tagd::tagspace *_tsp;

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

		callback_tester(tagd::tagspace *tsp) :
			last_code(), last_tag(nullptr), err_cmd(-1) {
			_tsp = tsp;
		}

		~callback_tester() {
			if (last_tag != nullptr)
				delete last_tag;
		}

		void cmd_get(const tagd::abstract_tag& t) {
			cmd = TOK_CMD_GET;
			renew_last_tag(t.id(), t.pos());
			last_code = _tsp->get(*last_tag, t.id(), nullptr);
		}

		void cmd_replace(const tagd::abstract_tag& t) override {
			cmd_put(t);
			cmd = TOK_CMD_REPLACE;
		}

		void cmd_put(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tsp->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_PUT;
			renew_last_tag(t.id(), t.pos());
			*last_tag = t;
			last_code = _tsp->put(*last_tag, nullptr);
		}

		void cmd_delete(const tagd::abstract_tag& t) {
			if (t.id() == t.super_object()) {
				last_code = _tsp->error(tagd::TS_MISUSE, "id cannot be the same as sub");
				return;
			}
			cmd = TOK_CMD_DELETE;
			if (t.pos() == tagd::POS_URL) {
				renew_last_tag(t.id(), t.pos());
			} else {
				renew_last_tag(t.id(), t.pos());
				*last_tag = t;
			}
			last_code = _tsp->del(*last_tag, nullptr);
		}

		void cmd_query(const tagd::interrogator& q) {
			assert (q.pos() == tagd::POS_INTERROGATOR);

			cmd = TOK_CMD_QUERY;
			renew_last_tag(q.id(), q.pos());

			*last_tag = q;
			last_tag_set.clear();
			last_code = _tsp->query(last_tag_set, q, nullptr);
		}

		void cmd_error() {
			cmd = _driver->cmd();

			renew_last_tag(_driver->tag().id());
			if (!_driver->tag().empty())
				*last_tag = _driver->tag();
		}
};

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))
#define INIT_TSP_TAGL() \
	tagspace_tester tsp; \
	auto ssn = tsp.get_session(); \
	httagd::httagl tagl(&tsp, &ssn);


class view_test_request : public httagd::request {
public:
	view_test_request(const std::string& selected) : request(tagd::HTTP_GET, "/") {
		if (!selected.empty())
			_address.options.emplace(TAGL::QUERY_OPT_VIEW, selected);
	}
};

template<typename Test>
void with_view_transaction(const std::string& selected, Test test) {
	tagd::config cfg;
	TS_ASSERT_TAGD_OK(cfg.init("/tmp/tagd-test-home"));
	tagd::tagspace::memory tsp;
	TS_ASSERT_TAGD_OK(tsp.init());
	httagd::viewspace views(cfg);
	httagd::server svr(&tsp, &views, cfg);
	view_test_request req(selected);
	// A real evbuffer exercises response rendering without opening a listener.
	std::unique_ptr<evhtp_request_t, decltype(&evhtp_request_free)> raw(
		evhtp_request_new(nullptr, nullptr), evhtp_request_free);
	httagd::response res(raw.get());
	httagd::transaction tx(&svr, &req, &res, &tsp, nullptr, &views);
	auto ssn = tsp.get_session();
	httagd::callback cb(&tx);
	httagd::httagl driver(&tsp, &cb, &ssn);
	tx.drvr = &driver;
	// Match main_cb's shared error ownership, including later cmd_error delivery.
	tx.share_errors(driver).share_errors(tsp).share_errors(views).share_errors(ssn);
	test(tx, cb);
}

std::string view_response_body(httagd::transaction& tx) {
	auto* buffer = tx.res->output_buffer();
	std::string body(evbuffer_get_length(buffer), '\0');
	evbuffer_copyout(buffer, body.data(), body.size());
	return body;
}

class Tester : public CxxTest::TestSuite {
	public:
	void test_alias_conflicts_never_report_http_success() {
		for (const auto method : {tagd::HTTP_POST, tagd::HTTP_PATCH}) {
			for (const bool other_context : {false, true}) {
				with_view_transaction("", [&](auto& tx, auto& cb) {
					tx.req->method = method;
					for (const auto* id : {"dog", "cat", "english", "japanese"}) {
						TS_ASSERT_TAGD_OK(tx.tsp()->put(tagd::abstract_tag(id, HARD_TAG_SUB,
							HARD_TAG_ENTITY, tagd::POS_TAG)));
					}
					TS_ASSERT_TAGD_OK(tx.tsp()->put(tagd::referent("doggy", "dog", "english")));
					cb.cmd_put(tagd::referent("doggy", other_context ? "dog" : "cat",
						other_context ? "japanese" : "english"));
					TS_ASSERT_EQUALS(tx.most_severe(), tagd::TS_AMBIGUOUS);
					TS_ASSERT_EQUALS(tx.res->res_code(), -1);
				});
			}
		}
	}

	void test_mutation_status_ignores_duplicate_setting() {
		for (auto method : {tagd::HTTP_PUT, tagd::HTTP_POST, tagd::HTTP_PATCH}) {
			for (const bool ignore : {false, true}) {
				for (const std::string selected : {"", "tagl"}) {
					with_view_transaction(selected, [&](auto& tx, auto& cb) {
						tx.req->method = method;
						tx.drvr->flags = ignore ? tagd::F_IGNORE_DUPLICATES : 0;
						tagd::abstract_tag subject("status_subject", HARD_TAG_SUB, HARD_TAG_ENTITY,
							tagd::POS_TAG);
						TS_ASSERT_TAGD_OK(tx.tsp()->put(subject, tx.drvr->session_ptr()));
						if (method == tagd::HTTP_PUT)
							cb.cmd_replace(subject);
						else
							cb.cmd_put(subject);

						TS_ASSERT_TAGD_OK(tx.most_severe());
						if (method != tagd::HTTP_POST) {
							TS_ASSERT_EQUALS(tx.res->res_code(), selected.empty() ? 204 : 200);
							TS_ASSERT_EQUALS(view_response_body(tx).empty(), selected.empty());
						}
					});
				}
			}
		}
	}

	void test_request_retains_raw_target_and_shared_options() {
		const std::string target = "/_/:_url:?v=browse.html&c=&_url=https://example.com/a?x=1&q=a+b%21";
		httagd::request req(tagd::HTTP_GET, target);
		tagd::errorable errors;
		TS_ASSERT_EQUALS(req.parse_target(errors), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(req.tagdurl(), target);
		TS_ASSERT_EQUALS(req.address().selector, "_");
		TS_ASSERT_EQUALS(req.address().operation, "/:_url:");
		TS_ASSERT_EQUALS(req.query_opt_view(), "browse.html");
		TS_ASSERT(req.query_map().contains(TAGL::QUERY_OPT_CONTEXT));
		TS_ASSERT(!req.query_map().contains(TAGL::QUERY_OPT_SEARCH));
		TS_ASSERT_EQUALS(req.query_opt(TAGL::QUERY_OPT_URL), "https://example.com/a?x=1&q=a+b%21");
		req.addressed_path("/:_url:");
		TS_ASSERT_EQUALS(req.operation_path(), "/:_url:");
		TS_ASSERT_EQUALS(req.tagdurl(), target.substr(2));
		for (const auto* suffix : {"foo=bar", "foo", "q", "&q=a", "q=a&", "q=a&q=b"}) {
			httagd::request invalid(tagd::HTTP_GET, std::string("/_/animal/?") + suffix);
			errors.clear_errors();
			TS_ASSERT_EQUALS(invalid.parse_target(errors), tagd::TAGL_ERR);
		}

		// The root has no selector or subject; it carries only options.
		httagd::request root(tagd::HTTP_GET, "/?q=dog&c=a");
		errors.clear_errors();
		TS_ASSERT_EQUALS(root.parse_target(errors), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(root.query_opt_search(), "dog");
		TS_ASSERT_EQUALS(root.query_contexts().size(), 1u);
		TS_ASSERT_EQUALS(root.address().query, "?q=dog&c=a");
	}

	void test_generated_url_and_event_links_preserve_fields() {
		const std::string name = "test_generated_url_and_event_links_preserve_fields";
		TS_ASSERT(ctemplate::StringToTemplateCache(name, "{{id_lnk}}", ctemplate::DO_NOT_STRIP));
		for (const std::string value : {"http://joe@example.com", "http://joe:@example.com",
			"http://joe:%00@example.com", "mailto:joe@example.com", "https://example.com/a!b?x=1#top",
			"ev:t!h!p!program!s!1!alarm,urgent", "err:t!h!p!program!s!1!alarm,urgent"}) {
			httagd::tagd_template tpl(name);
			tpl.set_tag_link({}, "id", value);
			TS_ASSERT_TAGD_OK(tpl.expand(name));
			const tagd::url url(value);
			const auto id = url.ok() ? url.hduri() : value;
			TS_ASSERT_EQUALS(tpl.output_str(), "/_/" + TAGL::encode_tagdurl_tag_id(id));
			INIT_TSP_TAGL();
			TS_ASSERT_TAGD_OK(tagl.scan_tagdurl(tagd::HTTP_GET, tpl.output_str().substr(2)));
			tagl.finish();
			TS_ASSERT_EQUALS(tagl.tag().id(), value);
		}
	}

	void test_view_lookup_preserves_registered_handler() {
		// Lookup preserves the registered handler and gives the caller a retained copy.
		TS_ASSERT(!accepts_temporary_view_result<httagd::viewspace>);
		tagd::config cfg;
		TS_ASSERT_TAGD_OK(cfg.init("/tmp/tagd-test-home"));
		httagd::viewspace views(cfg);
		httagd::transaction tx(nullptr, nullptr, nullptr, nullptr, nullptr, &views);
		unsigned calls = 0;
		httagd::get_handler_t handler([&](httagd::transaction&, const httagd::view&,
			const tagd::abstract_tag&) {
			++calls;
			return tagd::TAGD_OK;
		});
		TS_ASSERT_TAGD_OK(views.put({"test", handler}));

		for (unsigned i = 0; i < 2; ++i) {
			httagd::view result;
			TS_ASSERT_TAGD_OK(views.get(result, httagd::get_view_id("test")));
			TS_ASSERT_TAGD_OK(result.get_function(tx, result, tagd::abstract_tag("dog")));
		}
		TS_ASSERT_EQUALS(calls, 2u);
	}

	void test_view_copy_move_and_cross_action_assignment() {
		using enum httagd::view_action;
		// Moving an owning handler value must not require exception recovery.
		TS_ASSERT(std::is_nothrow_move_constructible_v<httagd::view>);
		TS_ASSERT(std::is_nothrow_move_assignable_v<httagd::view>);
		httagd::transaction tx(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		auto called = UNDEF;
		auto handler = [&](httagd::view_action action) {
			return [&, action](auto&&...) {
				called = action;
				return tagd::TAGD_OK;
			};
		};
		const std::vector<httagd::view> originals{
			{"test", httagd::empty_handler_t(handler(EMPTY))},
			{"test", httagd::get_handler_t(handler(GET))},
			{"test", httagd::put_handler_t(handler(PUT))},
			{"test", httagd::delete_handler_t(handler(DELETE))},
			{"test", httagd::query_handler_t(handler(QUERY))},
			{"test", httagd::error_handler_t(handler(ERROR))}
		};
		auto invoke = [&](httagd::view& value, httagd::view_action action) {
			TS_ASSERT_EQUALS(value.action(), action);
			TS_ASSERT_EQUALS(value.name(), "test");
			called = UNDEF;
			tagd::abstract_tag tag("dog");
			tagd::interrogator query;
			tagd::tag_set results;
			tagd::errorable errors;
			auto rc = tagd::TAGD_ERR;
			switch (action) {
				case EMPTY: rc = value.empty_function(tx, value); break;
				case GET: rc = value.get_function(tx, value, tag); break;
				case PUT: rc = value.put_function(tx, value, tag); break;
				case DELETE: rc = value.delete_function(tx, value, tag); break;
				case QUERY: rc = value.query_function(tx, value, query, results); break;
				case ERROR: rc = value.error_function(tx, value, errors); break;
				case UNDEF: break;
			}
			TS_ASSERT_TAGD_OK(rc);
			TS_ASSERT_EQUALS(called, action);
		};

		for (const auto& original : originals) {
			httagd::view copied(original);
			invoke(copied, original.action());
			httagd::view moved(std::move(copied));
			invoke(moved, original.action());

			httagd::view assigned("other", httagd::get_handler_t(handler(GET)));
			assigned = original;
			invoke(assigned, original.action());
			httagd::view move_assigned("other", httagd::put_handler_t(handler(PUT)));
			move_assigned = std::move(assigned);
			invoke(move_assigned, original.action());
		}
	}

	void test_view_rejects_undefined_and_mismatched_actions() {
		httagd::transaction tx(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		tagd::abstract_tag tag("dog");
		tagd::interrogator query;
		tagd::tag_set results;
		tagd::errorable errors;
		httagd::view empty;
		TS_ASSERT_EQUALS(empty.empty_function(tx, empty), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(empty.get_function(tx, empty, tag), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(empty.put_function(tx, empty, tag), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(empty.delete_function(tx, empty, tag), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(empty.query_function(tx, empty, query, results), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(empty.error_function(tx, empty, errors), tagd::TAGD_ERR);

		unsigned calls = 0;
		httagd::view get("test", httagd::get_handler_t([&](auto&&...) {
			++calls;
			return tagd::TAGD_OK;
		}));
		TS_ASSERT_EQUALS(get.put_function(tx, get, tag), tagd::TAGD_ERR);
		TS_ASSERT_EQUALS(calls, 0u);
	}

	void test_view_configuration_validation() {
		tagd::config cfg;
		TS_ASSERT_TAGD_OK(cfg.init("/tmp/tagd-test-home"));
		httagd::viewspace views(cfg);
		TS_ASSERT(cfg.fallback_error_view().empty());
		TS_ASSERT_TAGD_OK(views.validate_config());
		TS_ASSERT_TAGD_OK(views.put({"read-only", httagd::get_handler_t([](auto&&...) {
			return tagd::TAGD_OK;
		})}));
		TS_ASSERT_TAGD_OK(views.put({"errors", httagd::error_handler_t([](auto&&...) {
			return tagd::TAGD_OK;
		})}));

		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_DEFAULT_VIEW, "read-only"));
		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, "errors"));
		TS_ASSERT_TAGD_OK(views.validate_config());
		TS_ASSERT_EQUALS(cfg.fallback_error_view(), "errors");
		TS_ASSERT(views.find(httagd::get_view_id("read-only")));
		TS_ASSERT(!views.find(httagd::error_view_id("read-only")));
		TS_ASSERT(!views.has_errors());

		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, "read-only"));
		TS_ASSERT_EQUALS(views.validate_config(), tagd::TS_NOT_FOUND);
		views.clear_errors();
		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, "tagl"));
		TS_ASSERT_TAGD_OK(views.validate_config());
		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_DEFAULT_VIEW, "absent"));
		TS_ASSERT_EQUALS(views.validate_config(), tagd::TS_NOT_FOUND);
		views.clear_errors();
		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_DEFAULT_VIEW, "tagl"));
		TS_ASSERT_TAGD_OK(views.validate_config());

		cfg.start_runtime();
		TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, "errors"));
		TS_ASSERT_EQUALS(cfg.fallback_error_view(), "errors");
		TS_ASSERT_TAGD_OK(views.validate_config());
	}

	void test_view_resolution_and_reserved_name() {
		using enum httagd::view_action;
		tagd::config cfg;
		TS_ASSERT_TAGD_OK(cfg.init("/tmp/tagd-test-home"));
		httagd::viewspace views(cfg);
		auto handler = [](auto&&...) {
			return tagd::TAGD_OK;
		};
		for (const auto& registered : std::vector<httagd::view>{
			{"complete", httagd::empty_handler_t(handler)},
			{"complete", httagd::get_handler_t(handler)},
			{"complete", httagd::put_handler_t(handler)},
			{"complete", httagd::delete_handler_t(handler)},
			{"complete", httagd::query_handler_t(handler)},
			{"errors-only", httagd::error_handler_t(handler)}
		}) {
			TS_ASSERT_TAGD_OK(views.put(registered));
		}

		for (auto action : {EMPTY, GET, PUT, DELETE, QUERY}) {
			httagd::view resolved;
			TS_ASSERT_TAGD_OK(views.resolve(resolved, {"complete", action}));
			TS_ASSERT_EQUALS(resolved.action(), action);
			TS_ASSERT_EQUALS(resolved.name(), "complete");
			TS_ASSERT_TAGD_OK(views.resolve(resolved, {"errors-only", action}));
			TS_ASSERT_EQUALS(resolved.action(), UNDEF);
			TS_ASSERT_TAGD_OK(views.resolve(resolved, {"tagl", action}));
			TS_ASSERT_EQUALS(resolved.action(), UNDEF);
			TS_ASSERT_EQUALS(views.resolve(resolved, {"missing", action}), tagd::TS_NOT_FOUND);
			views.clear_errors();
		}

		TS_ASSERT_EQUALS(views.put({"tagl", httagd::get_handler_t(handler)}), tagd::TS_MISUSE);
		TS_ASSERT(!views.contains_name("tagl"));
	}

	void test_callback_view_resolution_table() {
		using enum httagd::view_action;
		struct selection {
			std::string requested;
			std::string configured;
		};
		for (auto action : {EMPTY, GET, QUERY, PUT, DELETE}) {
			for (const auto& choice : std::vector<selection>{
				{"", ""}, {"tagl", "complete"}, {"complete", ""}, {"partial", ""},
				{"missing", ""}, {"", "complete"}, {"", "partial"}
			}) {
				with_view_transaction(choice.requested, [&](auto& tx, auto& cb) {
					TS_ASSERT_TAGD_OK(tx.svr->configuration().set(
						tagd::HARD_TAG_CONFIG_DEFAULT_VIEW, choice.configured));
					unsigned calls = 0;
					auto handler = [&](auto&&...) {
						++calls;
						// Success views observe completed storage, never a proposed mutation.
						if (action == PUT)
							TS_ASSERT(tx.tsp()->exists("view_subject"));
						if (action == DELETE)
							TS_ASSERT(!tx.tsp()->exists("view_subject"));
						tx.res->add(httagd::view_action_str(action));
						return tagd::TAGD_OK;
					};
					for (const auto& registered : std::vector<httagd::view>{
						{"complete", httagd::empty_handler_t(handler)},
						{"complete", httagd::get_handler_t(handler)},
						{"complete", httagd::query_handler_t(handler)},
						{"complete", httagd::put_handler_t(handler)},
						{"complete", httagd::delete_handler_t(handler)},
						{"partial", httagd::error_handler_t(handler)}
					}) {
						TS_ASSERT_TAGD_OK(tx.vws->put(registered));
					}

					tagd::abstract_tag subject("view_subject", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
					const bool seeded = action == GET || action == QUERY || action == DELETE;
					if (seeded)
						TS_ASSERT_TAGD_OK(tx.tsp()->put(subject, tx.drvr->session_ptr()));
					tagd::interrogator query(HARD_TAG_WHAT, HARD_TAG_SUB, HARD_TAG_ENTITY);
					switch (action) {
						case EMPTY: cb.empty(); break;
						case GET: cb.cmd_get(subject); break;
						case QUERY: cb.cmd_query(query); break;
						case PUT: cb.cmd_put(subject); break;
						case DELETE: cb.cmd_delete(tagd::abstract_tag(subject.id())); break;
						default: TS_FAIL("unexpected action");
					}

					const auto body = view_response_body(tx);
					if (choice.requested == "missing") {
						TS_ASSERT_EQUALS(tx.most_severe(), tagd::TS_NOT_FOUND);
						TS_ASSERT_EQUALS(calls, 0u);
						TS_ASSERT_EQUALS(tx.tsp()->exists(subject.id()), seeded);
						TS_ASSERT_DIFFERS(body.find("view not found: missing"), std::string::npos);
						return;
					}

					TS_ASSERT_TAGD_OK(tx.most_severe());
					const bool app = choice.requested == "complete"
						|| (choice.requested.empty() && choice.configured == "complete");
					TS_ASSERT_EQUALS(calls, app ? 1u : 0u);
					TS_ASSERT_EQUALS(tx.tsp()->exists(subject.id()),
						action == GET || action == QUERY || action == PUT);
					if (app) {
						TS_ASSERT_EQUALS(body, httagd::view_action_str(action));
					} else if (action == EMPTY) {
						TS_ASSERT_EQUALS(body, "This is tagd\n");
					} else if (action == GET || action == QUERY) {
						TS_ASSERT_DIFFERS(body.find(subject.id()), std::string::npos);
					} else {
						TS_ASSERT(body.empty());
					}
				});
			}
		}
	}

	void test_write_views_skip_failed_operations_and_render_errors_once() {
		for (const bool put : {true, false}) {
			with_view_transaction("write", [&](auto& tx, auto& cb) {
				unsigned successes = 0;
				unsigned errors = 0;
				auto success = [&](auto&&...) {
					++successes;
					return tagd::TAGD_OK;
				};
				TS_ASSERT_TAGD_OK(tx.vws->put({"write", httagd::put_handler_t(success)}));
				TS_ASSERT_TAGD_OK(tx.vws->put({"write", httagd::delete_handler_t(success)}));
				TS_ASSERT_TAGD_OK(tx.vws->put({"write", httagd::error_handler_t([&](auto&&...) {
					++errors;
					tx.res->add("operation error");
					return tagd::TAGD_OK;
				})}));

				if (put) {
					cb.cmd_put(tagd::abstract_tag("view_subject", HARD_TAG_SUB, "missing", tagd::POS_TAG));
				} else {
					cb.cmd_delete(tagd::abstract_tag("view_subject"));
				}
				cb.cmd_error();
				cb.cmd_error();
				TS_ASSERT_DIFFERS(tx.most_severe(), tagd::TAGD_OK);
				TS_ASSERT(!tx.tsp()->exists("view_subject"));
				TS_ASSERT_EQUALS(successes, 0u);
				TS_ASSERT_EQUALS(errors, 1u);
				TS_ASSERT_EQUALS(view_response_body(tx), "operation error");
			});
		}
	}

	void test_write_view_failure_preserves_completed_operation() {
		for (const bool put : {true, false}) {
			with_view_transaction("write", [&](auto& tx, auto& cb) {
				auto failure = [&](auto&&...) {
					return tx.error(tagd::TAGD_ERR, "view rendering failed");
				};
				TS_ASSERT_TAGD_OK(tx.vws->put({"write", httagd::put_handler_t(failure)}));
				TS_ASSERT_TAGD_OK(tx.vws->put({"write", httagd::delete_handler_t(failure)}));
				tagd::abstract_tag subject("view_subject", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
				if (put) {
					cb.cmd_put(subject);
				} else {
					TS_ASSERT_TAGD_OK(tx.tsp()->put(subject, tx.drvr->session_ptr()));
					cb.cmd_delete(tagd::abstract_tag(subject.id()));
				}
				const auto body = view_response_body(tx);
				cb.cmd_error();
				TS_ASSERT_EQUALS(tx.most_severe(), tagd::TAGD_ERR);
				TS_ASSERT_EQUALS(tx.tsp()->exists(subject.id()), put);
				TS_ASSERT_DIFFERS(body.find("view rendering failed"), std::string::npos);
				TS_ASSERT_EQUALS(view_response_body(tx), body);
			});
		}
	}

	void test_error_fallback_reads_current_configuration() {
		for (const auto* fallback : {"", "tagl", "missing", "first", "second"}) {
			with_view_transaction("read", [&](auto& tx, auto& cb) {
				TS_ASSERT_TAGD_OK(tx.vws->put({"read", httagd::get_handler_t([](auto&&...) {
					return tagd::TAGD_OK;
				})}));
				for (const auto* name : {"first", "second"}) {
					TS_ASSERT_TAGD_OK(tx.vws->put({name, httagd::error_handler_t([&, name](auto&&...) {
						tx.res->add(name);
						return tagd::TAGD_OK;
					})}));
				}
				auto& cfg = tx.svr->configuration();
				if (*fallback)
					TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, "first"));
				TS_ASSERT_TAGD_OK(tx.vws->validate_config());
				cfg.start_runtime();
				TS_ASSERT_TAGD_OK(cfg.set(tagd::HARD_TAG_CONFIG_FALLBACK_ERROR_VIEW, fallback));
				cb.cmd_get(tagd::abstract_tag("missing"));
				const auto body = view_response_body(tx);
				cb.cmd_error();
				TS_ASSERT_EQUALS(view_response_body(tx), body);
				TS_ASSERT_EQUALS(tx.most_severe(), tagd::TS_NOT_FOUND);
				if (std::string(fallback) == "first" || std::string(fallback) == "second") {
					TS_ASSERT_EQUALS(body, fallback);
				} else {
					TS_ASSERT_DIFFERS(body.find(">> err:"), std::string::npos);
				}
			});
		}
	}

	void test_empty_query_is_a_result_and_does_not_poison_later_commands() {
		for (const auto* selected : {"tagl", "query", "partial"}) {
			with_view_transaction(selected, [&](auto& tx, auto& cb) {
				unsigned query_calls = 0;
				unsigned error_calls = 0;
				TS_ASSERT_TAGD_OK(tx.vws->put({"query", httagd::query_handler_t(
					[&](auto&, const auto&, const auto&, const tagd::tag_set& results) {
						++query_calls;
						TS_ASSERT(results.empty());
						tx.res->add("empty result");
						return tagd::TAGD_OK;
					})}));
				for (const auto* name : {"query", "partial"}) {
					TS_ASSERT_TAGD_OK(tx.vws->put({name, httagd::error_handler_t([&](auto&&...) {
						++error_calls;
						return tagd::TAGD_OK;
					})}));
				}

				tagd::abstract_tag leaf("leaf", HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG);
				TS_ASSERT_TAGD_OK(tx.tsp()->put(leaf, tx.drvr->session_ptr()));
				cb.cmd_query(tagd::interrogator(HARD_TAG_WHAT, HARD_TAG_SUB, leaf.id()));
				TS_ASSERT_EQUALS(tx.size(), 0u);
				TS_ASSERT(!tx.drvr->has_errors());
				TS_ASSERT_EQUALS(tx.drvr->session_ptr()->code(), tagd::TS_NOT_FOUND);
				TS_ASSERT_EQUALS(query_calls, std::string(selected) == "query" ? 1u : 0u);
				TS_ASSERT_EQUALS(view_response_body(tx),
					std::string(selected) == "query" ? "empty result" : "");

				cb.cmd_put(tagd::abstract_tag("after_empty", HARD_TAG_SUB, HARD_TAG_ENTITY,
					tagd::POS_TAG));
				TS_ASSERT(tx.tsp()->exists("after_empty"));
				TS_ASSERT_EQUALS(tx.size(), 0u);
				TS_ASSERT_TAGD_OK(tx.drvr->session_ptr()->code());
				TS_ASSERT_EQUALS(error_calls, 0u);
			});
		}
	}

	void test_query_failure_still_uses_requested_family_error_view() {
		for (const auto* selected : {"tagl", "query", "partial"}) {
			with_view_transaction(selected, [&](auto& tx, auto& cb) {
				unsigned query_calls = 0;
				unsigned error_calls = 0;
				TS_ASSERT_TAGD_OK(tx.vws->put({"query", httagd::query_handler_t([&](auto&&...) {
					++query_calls;
					return tagd::TAGD_OK;
				})}));
				for (const auto* name : {"query", "partial"}) {
					TS_ASSERT_TAGD_OK(tx.vws->put({name, httagd::error_handler_t([&](auto&&...) {
						++error_calls;
						tx.res->add("query error");
						return tagd::TAGD_OK;
					})}));
				}

				// A query without any condition is misuse, unlike a valid query with no matches.
				tagd::interrogator query(HARD_TAG_WHAT);
				cb.cmd_query(query);
				TS_ASSERT(tx.size() > 0);
				TS_ASSERT_EQUALS(tx.most_severe(), tagd::TS_MISUSE);
				const auto body = view_response_body(tx);
				cb.cmd_error();
				TS_ASSERT_EQUALS(query_calls, 0u);
				TS_ASSERT_EQUALS(error_calls, std::string(selected) == "tagl" ? 0u : 1u);
				TS_ASSERT_EQUALS(view_response_body(tx), body);
				if (std::string(selected) != "tagl")
					TS_ASSERT_EQUALS(body, "query error");
			});
		}
	}

	void test_type_query_methods_and_path_errors() {
		for (const auto method : {tagd::HTTP_GET, tagd::HTTP_HEAD, tagd::HTTP_PUT,
			tagd::HTTP_POST, tagd::HTTP_DELETE}) {
			for (const auto* path : {"/animal/", "/animal,legs/", "/,legs/"}) {
				INIT_TSP_TAGL();
				httagd::request req(method, path);
				const auto rc = method == tagd::HTTP_PUT || method == tagd::HTTP_POST
					? tagl.tagdurl_put(req) : tagl.scan_request_tagdurl(req);
				TS_ASSERT_EQUALS(rc, method == tagd::HTTP_GET || method == tagd::HTTP_HEAD
					? tagd::TAGD_OK : tagd::TS_MISUSE);
			}
			for (const auto* path : {"/dog,legs", "/,legs"}) {
				INIT_TSP_TAGL();
				httagd::request req(method, path);
				const auto rc = method == tagd::HTTP_PUT || method == tagd::HTTP_POST
					? tagl.tagdurl_put(req) : tagl.scan_request_tagdurl(req);
				TS_ASSERT_EQUALS(rc, tagd::TS_MISUSE);
			}
			INIT_TSP_TAGL();
			httagd::request req(method, "/animal,,legs/");
			const auto rc = method == tagd::HTTP_PUT || method == tagd::HTTP_POST
				? tagl.tagdurl_put(req) : tagl.scan_request_tagdurl(req);
			TS_ASSERT_EQUALS(rc, tagd::TAGL_ERR);
		}
	}

	void test_contradictions_map_to_http_conflict() {
		TS_ASSERT_EQUALS(httagd::tagd_code_evhtp_res(tagd::TS_CONTRADICTION), EVHTP_RES_CONFLICT);
	}

	void test_ambiguity_maps_to_http_conflict() {
		TS_ASSERT_EQUALS(httagd::tagd_code_evhtp_res(tagd::TS_AMBIGUOUS), EVHTP_RES_CONFLICT);
	}

	void test_tag_link_preserves_invalid_url_identity() {
		const std::string name = "test_tag_link_preserves_invalid_url_identity";
		TS_ASSERT(ctemplate::StringToTemplateCache(name, "{{id}}|{{id_lnk}}",
			ctemplate::DO_NOT_STRIP));
		for (const std::string invalid : {"hd:com!example!!/a%20b!!!!!!http",
			"http://example.com:", "mailto://joe@example.com"}) {
			httagd::tagd_template tpl(name);
			tpl.set_tag_link(TAGL::tagdurl_option_map{}, "id", invalid);
			TS_ASSERT_TAGD_OK(tpl.expand(name));
			TS_ASSERT_EQUALS(tpl.output_str(), invalid + "|/_/" + TAGL::encode_tagdurl_tag_id(invalid));
		}
	}


	// These translator tests receive the operation path after shared target dispatch.
	// Full addressed HTTP paths and policy are exercised by addressing.exp.
	void test_get_tagdurl(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/dog"));
		tagl.finish();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
	}

	void test_get_tagdurl_trailing_path(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_get(httagd::request(tagd::HTTP_GET, "/dog/"));
		tagl.finish();
		// two path separators indicate a query
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id() , HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
	}

	void test_post_tagdurl(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));
		tagl.execute(">> dog is_a animal _has legs _can bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
		TS_ASSERT( tagl.tag().related("_can", "bark") )
	}

	void test_del_tagdurl(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_del(httagd::request(tagd::HTTP_DELETE, "/dog"));
		tagl.finish();
		TS_ASSERT_EQUALS(tagl.code(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DELETE);
		TS_ASSERT_EQUALS(tagl.tag().id(), "dog");
		TS_ASSERT(tagl.tag().relations.empty());
	}

	void test_post_tagdurl_evbuffer_body(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));

		struct evbuffer *input = evbuffer_new();

		std::string s(">> dog is_a animal _has legs _can bark;");
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
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/doggy"));
		tagl.execute(">> doggy _refers_to dog _context simple_english;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "doggy" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
		TS_ASSERT_EQUALS( dynamic_cast<const tagd::referent&>(tagl.tag()).context(), "simple_english" )
	}

	void test_post_tagdurl_utf8_referent_context(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/イヌ"));
		tagl.execute(">> イヌ _refers_to dog _context japanese;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_PUT )
		TS_ASSERT_EQUALS( tagl.tag().id() , "イヌ" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "dog" )
		TS_ASSERT_EQUALS( dynamic_cast<const tagd::referent&>(tagl.tag()).context(), "japanese" )
	}

	void test_post_tagdurl_evbuffer_rejects_multiple_statements() {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_POST, "/dog"));
		evbuffer* input = evbuffer_new();
		const std::string body = ">> dog _has legs; >> dog _has tail;";
		evbuffer_add(input, body.data(), body.size());
		TS_ASSERT_EQUALS(tagl.execute(input), tagd::TS_MISUSE);
		evbuffer_free(input);
	}

	void test_put_tagdurl_constrained_tag_id(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, "/dog"));
		tagl.execute(">>= dog is_a animal _has legs");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_REPLACE )
		TS_ASSERT_EQUALS( tagl.tag().id() , "dog" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "animal" )
		TS_ASSERT( tagl.tag().related("_has", "legs") )
	}

	void test_put_tagdurl_constrained_tag_id_error(void) {
		INIT_TSP_TAGL();
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, "/dog"));
		tagl.execute(">>= cat is_a animal");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGL_ERR" )
	}

	void test_get_tagdurl_hduri(void) {
		INIT_TSP_TAGL();
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
		INIT_TSP_TAGL();
		const std::string hduri{"hd:org!wikipedia!en!/wiki/Cat!!!!!!https"};
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, std::string("/").append(hduri)));

		const std::string s(">>= https://en.wikipedia.org/wiki/Cat about cat _has title = \"Cat - Wikipedia, the free encyclopedia\"");
		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		tagl.print_errors();
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_REPLACE )
		TS_ASSERT_EQUALS( tagl.tag().id() , "https://en.wikipedia.org/wiki/Cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "_url" )
		TS_ASSERT( tagl.tag().related("about", "cat") )
		TS_ASSERT( tagl.tag().related("_has", "title", "Cat - Wikipedia, the free encyclopedia") )

		evbuffer_free(input);
	}

	void test_put_tagdurl_evbuffer_body_constrained_hduri(void) {
		INIT_TSP_TAGL();
		const char *hduri = "hd:org!wikipedia!en!/wiki/Cat!!!!!!https";
		tagl.tagdurl_put(httagd::request(tagd::HTTP_PUT, std::string("/").append(hduri)));

		const std::string s = std::string(">>= ").append(hduri).append(" about cat _has title = \"Cat - Wikipedia, the free encyclopedia\"");
		struct evbuffer *input = evbuffer_new();
		evbuffer_add(input, s.c_str(), s.size());
		tagl.execute(input);
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tagl.code()), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd() , TOK_CMD_REPLACE )
		TS_ASSERT_EQUALS( tagl.tag().id() , "https://en.wikipedia.org/wiki/Cat" )
		TS_ASSERT_EQUALS( tagl.tag().super_object() , "_url" )
		TS_ASSERT( tagl.tag().related("about", "cat") )
		TS_ASSERT( tagl.tag().related("_has", "title", "Cat - Wikipedia, the free encyclopedia") )

		evbuffer_free(input);
	}

	void test_tagdurl_query(void) {
		tagspace_tester tsp;
		auto ssn = tsp.get_session();
		callback_tester cb(&tsp);
		httagd::httagl tagl(&tsp, &cb, &ssn);
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
		tagspace_tester tsp;
		auto ssn = tsp.get_session();
		callback_tester cb(&tsp);
		httagd::httagl tagl(&tsp, &cb, &ssn);
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

	void test_runtime_configuration_is_read_by_http_consumers(void) {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		httagd::viewspace vws(cfg);
		httagd::server svr(&cfg, &vws, cfg);
		httagd::request req(tagd::HTTP_GET, "/config");
		httagd::transaction tx(&svr, &req, nullptr, &cfg, nullptr, &vws);
		cfg.start_runtime();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_DEFAULT_VIEW, "live.html"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(tx.effective_opt_view(), "live.html");
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_TPL_DIR, "/tmp"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(vws.fpath("page.tpl"), "/tmp/page.tpl");
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_WWW_DIR, "/tmp"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(svr.configuration().favicon(), "/tmp/favicon.ico");
	}

	// Contract 3: server owns _evbase and _htp; ~server() frees them without crash
	void test_server_destructor_frees_evbase_and_htp(void) {
		tagspace_tester tsp;
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-test-home"), tagd::TAGD_OK);
		httagd::viewspace vws(cfg);
		{
			httagd::server svr(&tsp, &vws, cfg);
			// _evbase and _htp allocated in server::init(); freed in ~server() at }
		}
		TS_ASSERT(true);
	}
};
