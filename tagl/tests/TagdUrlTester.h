#pragma once

#include "Tester.h"
#include <chrono>

class TagdUrlTester : public CxxTest::TestSuite {
	public:
	void test_bare_event_put_subject_constraints() {
		for (const auto* id : {"ev:t!h!p!program!s!1!_event", "err:t!h!p!program!s!1!_error"}) {
			for (const bool matches : {false, true}) {
				tagspace_tester tsp;
				callback_tester callback(&tsp);
				TAGL::driver input(&tsp, &callback);
				TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_PUT,
					std::string("/") + (matches ? id : "other")), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(input.execute(std::string(">> ") + id + ";"),
					matches ? tagd::TAGD_OK : tagd::TAGL_ERR);
				TS_ASSERT_EQUALS(tsp.pos(id) != tagd::POS_UNKNOWN, matches);
				TS_ASSERT_EQUALS(tsp.pos("other"), tagd::POS_UNKNOWN);
			}
		}
	}

	void test_structured_prefix_requires_structural_grammar() {
		for (const auto* prefix : {"hd:com!example!!!!!!!!", "hd:com!example!!/a!!!!!!"}) {
			for (const auto* scheme : {"https#bad", "https$bad", "https'bad", "123", ""}) {
				const auto id = std::string(prefix) + scheme;
				tagspace_tester tsp;
				TAGL::driver embedded(&tsp);
				TS_ASSERT_EQUALS(embedded.execute("/_/" + id + ";"), tagd::TAGL_ERR);

				for (const auto method : {tagd::HTTP_GET, tagd::HTTP_PUT}) {
					TAGL::driver http(&tsp);
					TS_ASSERT_EQUALS(http.scan_tagdurl(method, "/" + id), tagd::TAGL_ERR);
					TS_ASSERT(http.constrain_tag_id.empty());
				}
			}
		}
	}

	void test_structured_prefix_cannot_be_encoded_or_recased() {
		for (const auto* id : {"hd%3Acom!example!!!!!!jo%2521e!!https",
			"HD:com!example!!!!!!jo%2521e!!https", "hD:com!example!!!!!!joe!%2500!https",
			"hd%3Acom!example!!!!!!joe!%2500!https", "ev%3At!h!p!a!s!1!x%2521",
			"EV:t!h!p!a!s!1!x%2521", "err%3At!h!p!a!s!1!x%2521",
			"ERR:t!h!p!a!s!1!x%2521", "%68%64%3Acom!example!!!!!!!!https"}) {
			for (const auto* position : {"/", "/,", "/*/"}) {
				const auto path = std::string(position) + id + (position[1] == ',' ? "/legs" : "");
				tagspace_tester tsp;
				TAGL::driver embedded(&tsp);
				TS_ASSERT_EQUALS(embedded.execute("/_" + path + ";"), tagd::TAGL_ERR);
				TAGL::driver http(&tsp);
				TS_ASSERT_EQUALS(http.scan_tagdurl(tagd::HTTP_GET, path), tagd::TAGL_ERR);
			}
		}
	}

	void test_malformed_structured_prefixes_scan_in_bounded_time() {
		for (const auto* prefix : {"hd:/", "ev:/", "err:/", "x=hd:a,"}) {
			std::string path = "/_/";
			while (path.size() < 512 * 1024) {
				path += prefix;
			}
			path += 'x';

			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			const auto statement = "<< " + path + ";";
			auto* body = evbuffer_new();
			TS_ASSERT_EQUALS(evbuffer_add(body, statement.data(), statement.size()), 0);
			const auto start = std::chrono::steady_clock::now();
			TS_ASSERT_EQUALS(input.execute(body), tagd::TAGL_ERR);
			evbuffer_free(body);
			// A generous wall-clock bound catches repeated suffix scans without timing tiny inputs.
			TS_ASSERT(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
		}
	}

	void test_url_scheme_grammar_applies_to_every_url_input() {
		for (const auto* scheme : {"123", "https$bad"}) {
			for (const auto method : {tagd::HTTP_GET, tagd::HTTP_PUT}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(method,
					std::string("/:_url:?_url=") + scheme + "://example.com"), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
			}
		}

		for (const auto* path : {"/123%3A%2F%2Fexample.com", "/%20https%3A%2F%2Fexample.com"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_GET, path), tagd::TAGD_OK);
			input.finish();
			TS_ASSERT_EQUALS(input.code(), tagd::URL_ERR_SCHEME);
		}
	}

	void test_tagdurl_address_holds_parsed_options() {
		tagd::errorable errors;
		TAGL::tagdurl_address address;
		TS_ASSERT_EQUALS(TAGL::parse_tagdurl_address("/_/dog?c=a,b&q=y", address, errors),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(address.selector, "_");
		TS_ASSERT_EQUALS(address.operation, "/dog");
		TS_ASSERT_EQUALS(address.query, "?c=a,b&q=y");
		TS_ASSERT_EQUALS(address.options.at(TAGL::QUERY_OPT_SEARCH), "y");
		TS_ASSERT_EQUALS(address.contexts.size(), 2u);
		TS_ASSERT_EQUALS(address.contexts.front(), "a");
		TS_ASSERT_EQUALS(address.contexts.back(), "b");

		// The parsed address scans without splitting or parsing the target again.
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(TAGL::parse_tagdurl_address("/_/*/?q=bark", address, errors),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_GET, address.operation, address),
			tagd::TAGD_OK);
		input.finish();
		TS_ASSERT_EQUALS(input.code(), tagd::TAGD_OK);
		TS_ASSERT(input.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "bark"));

		for (const auto* target : {"/_/dog?foo=bar", "/_/dog?q=a&q=b", "/_/dog?c=a&c=b"}) {
			errors.clear_errors();
			TS_ASSERT_EQUALS(TAGL::parse_tagdurl_address(target, address, errors), tagd::TAGL_ERR);
		}
	}

	void test_url_authority_syntax_applies_to_every_url_input() {
		for (const auto* path : {"/_/:_url:?_url=https://EXAMPLE.com/A",
			"/_/https%3A%2F%2FEXAMPLE.com%2FA", "/_/hd:com!example!!/A!!!!!!https"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string(path) + ";"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.tag().id(), "https://example.com/A");
		}

		for (const auto* url : {"https://exa%20mple.com/", "https://a@b@example.com/",
			"https://example.com:abc/", "https://[::g]/", "https://ex%FFample.com/",
			"https://ex%C0%AFample.com/", "https://ex%C2%85ample.com/",
			"https://ex%E6%97%A5ample.com/", "https://ex%25FFample.com/"}) {
			for (const auto method : {tagd::HTTP_GET, tagd::HTTP_PUT}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(method,
					std::string("/:_url:?_url=") + url), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
			}
		}

		for (const auto& [path, rc] : std::vector<std::pair<std::string, tagd::code>>{
			{"/hd:com!EXAMPLE!!/A!!!!!!https", tagd::HDURI_ERR},
			{"/hd:com!ex%25FFample!!/!!!!!!https", tagd::URL_ERR_HOST},
			{"/hd:com!ex%25E6%2597%25A5ample!!/!!!!!!https", tagd::URL_ERR_HOST},
			{"/https%3A%2F%2Fex%25FFample.com%2F", tagd::URL_ERR_HOST},
			{"/https%3A%2F%2Fexa%20mple.com%2F", tagd::URL_ERR_HOST},
			{"/https%3A%2F%2Fexample.com%3Aabc%2F", tagd::URL_ERR_PORT}}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			if (input.scan_tagdurl(tagd::HTTP_GET, path) == tagd::TAGD_OK)
				input.finish();
			TS_ASSERT_EQUALS(input.code(), rc);
		}
	}

	void test_host_transport_escapes_preserve_literal_unicode_and_delimiters() {
		for (const auto* url : {"https://日.example.com/a", "https://sub!.example.com/a"}) {
			const tagd::url source(url);
			TS_ASSERT_EQUALS(source.code(), tagd::TAGD_OK);
			for (const auto& path : {source.hduri(), TAGL::encode_tagdurl_tag_id(source.hduri()),
				tagd::percent_encode(url, [](unsigned char) { return false; })}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_GET, "/" + path), tagd::TAGD_OK);
				input.finish();
				TS_ASSERT_EQUALS(input.code(), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(input.tag().id(), source.id());
			}
		}
	}

	void test_modifier_text_keeps_reserved_prefixes() {
		for (const auto& [encoded, text] : std::vector<std::pair<std::string, std::string>>{
			{"hd:bad", "hd:bad"}, {"ev:a", "ev:a"}, {"err:a!b", "err:a!b"},
			{"hd%3Abad", "hd:bad"}}) {
			tagspace_tester tsp;
			TAGL::driver tagl(&tsp);
			TS_ASSERT_EQUALS(tagl.execute("?? /_/animal/blood=" + encoded + "?q=warm;"),
				tagd::TAGD_OK);
			TS_ASSERT(tagl.tag().related("", "blood", text));
			TS_ASSERT(tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "warm"));
		}
	}

	void test_url_placeholder_consumes_raw_remainder() {
		const std::string url = "https://example.com/a/b?x=1&y=2&c=bad&q=dogs&_url=again"
			"&unknown=a&unknown=b+%21#top";
		for (const auto* terminator : {";", " ;"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute("/_/:_url:?c=japanese&_url=" + url + terminator),
				tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.tag().id(), url);
		}
		for (const auto* path : {"/,:_url:/legs", "/*/:_url:"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute("/_" + std::string(path) + "?_url=" + url + ";"),
				tagd::TAGD_OK);
			TS_ASSERT(path[1] == ','
				? input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, tagd::url(url).hduri()})
				: input.tag().related(tagd::url(url).hduri()));
		}
		tagspace_tester tsp;
		TAGL::driver http(&tsp);
		TS_ASSERT_EQUALS(http.scan_tagdurl(tagd::HTTP_GET, "/:_url:?_url=" + url + ";"), tagd::TAGD_OK);
		http.finish();
		TS_ASSERT_EQUALS(http.tag().id(), url + ";");
		TAGL::driver hduri(&tsp);
		TS_ASSERT_EQUALS(hduri.execute("/_/:_url:?_url=" + tagd::url(url).hduri() + ";"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(hduri.tag().id(), url);
	}

	void test_url_placeholder_errors() {
		for (const auto* path : {"/:_url:", "/dog?_url=https://example.com", "/:_url:?_url=",
			"/:_url:/legs?_url=https://example.com", "/,:_url:/:_url:?_url=https://example.com",
			"/*/:_url:=5?_url=https://example.com", "/:_url:?c=a&c=b&_url=https://example.com",
			"/:_url:?foo=bar&_url=https://example.com"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_GET, path), tagd::TAGL_ERR);
		}
	}

	void test_shared_option_validation() {
		for (const auto* options : {"foo=bar", "foo", "s=x", "q", "&q=a", "q=a&",
			"q=a&q=b", "c=japanese&c=", "q=%zz", "c=%00"}) {
			for (const auto mode : {TAGL::tagdurl_mode::resolve_subject,
				TAGL::tagdurl_mode::constrain_body}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_GET,
					std::string("/animal/?") + options, mode), tagd::TAGL_ERR);
			}
		}
		for (const auto* value : {"", "browse.html"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string("/_/dog?v=") + value + ";"),
				tagd::TS_NOT_IMPLEMENTED);
			TAGL::driver http(&tsp);
			TS_ASSERT_EQUALS(http.scan_tagdurl(tagd::HTTP_GET, std::string("/dog?v=") + value),
				tagd::TAGD_OK);
		}
	}

	void test_empty_search_and_context_options() {
		tagspace_tester tsp;
		TAGL::driver query(&tsp);
		TS_ASSERT_EQUALS(query.execute("/_/animal/?q=&c=;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(query.tag().super_object(), "animal");
		TAGL::driver root(&tsp);
		TS_ASSERT_EQUALS(root.scan_tagdurl(tagd::HTTP_GET, "/?q="), tagd::TAGL_ERR);
	}

	void test_url_family_query_positions() {
		const auto hduri = tagd::url("https://example.com/avatar.png?x=1#top").hduri();
		for (const auto& id : {hduri, std::string("ev:t!h!p!program!s!1!alarm%2Curgent"),
			std::string("err:t!h!p!program!s!1!alarm%2Curgent")}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute("/_/*/" + id + ";"), tagd::TAGD_OK);
			const auto expected = id.starts_with("hd:") ? id
				: id.substr(0, id.find("%2C")) + ",urgent";
			TS_ASSERT(input.tag().related(expected));
			for (const auto& path : {"/_/" + id + "/legs;", "/_/*/" + id + "=5;"}) {
				TAGL::driver invalid(&tsp);
				TS_ASSERT_EQUALS(invalid.execute(path), tagd::TAGL_ERR);
			}
		}
		tagspace_tester tsp;
		TS_ASSERT_EQUALS(tsp.put(test_tag("urgent", "animal")), tagd::TAGD_OK);
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("/_/," + hduri + "/legs;"), tagd::TAGD_OK);
		TS_ASSERT(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, hduri}));
		TS_ASSERT(input.tag().related("legs"));
		TS_ASSERT_EQUALS(input.execute("/_/*/ev:t!h!p!program!s!1!alarm,urgent;"), tagd::TAGD_OK);
		TS_ASSERT(input.tag().related("ev:t!h!p!program!s!1!alarm"));
		TS_ASSERT(input.tag().related("urgent"));
	}

	void test_tagdurl_encoding_and_identity_round_trips() {
		tagspace_tester mail;
		TAGL::driver encoded_url(&mail);
		TS_ASSERT_EQUALS(encoded_url.execute("/_/mailto%3Ajoe%40example.com;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(encoded_url.tag().id(), "mailto:joe@example.com");
		TS_ASSERT_EQUALS(encoded_url.tag().super_object(), HARD_TAG_URL);
		for (const std::string id : {"animal,legs", "a/b", "a=b&c?d#e%f", "O'Reilly",
			"two words", "日本", "a;"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(tagd::HTTP_GET,
				"/" + TAGL::encode_tagdurl_tag_id(id)), tagd::TAGD_OK);
			input.finish();
			TS_ASSERT_EQUALS(input.tag().id(), id);
		}
		TS_ASSERT_EQUALS(TAGL::encode_tagdurl_tag_id("a-b._~:c;'@+"), "a-b._~:c;'@+");
		TS_ASSERT_EQUALS(TAGL::encode_tagdurl_option("a b+c&d%#?/:=,日本"),
			"a+b%2Bc%26d%25%23?/:=,%E6%97%A5%E6%9C%AC");
		for (const std::string url : {"https://example.com/a%20b", "https://example.com/x!y",
			"https://example.com/O'Reilly", "https://example.com/O'", "https://example.com//a,b=c&d;?q=a#top",
			"https://example.com/日本", "http://joe@example.com", "http://joe:@example.com",
			"http://joe:%00@example.com", "mailto:joe@example.com"}) {
			const tagd::url source(url);
			for (const auto& id : {source.hduri(), TAGL::encode_tagdurl_tag_id(source.hduri())}) {
				for (const auto* terminator : {";", " ;"}) {
					tagspace_tester tsp;
					TAGL::driver input(&tsp);
					TS_ASSERT_EQUALS(input.execute("/_/" + id + terminator), tagd::TAGD_OK);
					TS_ASSERT_EQUALS(input.tag().id(), source.id());
				}
			}
		}
	}

	void test_encoded_structure_and_put_subject() {
		tagspace_tester tsp;
		TS_ASSERT_EQUALS(tsp.put(test_tag("animal,legs", "animal")), tagd::TAGD_OK);
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("/_/animal%2Clegs/;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.tag().super_object(), "animal,legs");
		TS_ASSERT(input.tag().type_relations.empty());
		TAGL::driver put(&tsp);
		TS_ASSERT_EQUALS(put.scan_tagdurl(tagd::HTTP_PUT, "/a%2Fb"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(put.constrain_tag_id, "a/b");
	}

	void test_invalid_tagdurl_escapes_and_reserved_ids() {
		for (const auto path : {"/bad%zz", "/bad%00", "/%3Afoo", "/:foo",
			"/hd:com!example!!/a%00!!!!!!http", " /dog",
			"/hd:com!example!!!!!!!joe!a%00!http", "/ev:t!h!p!a%00!s!1!_event",
			"/err:t!h!p!a!s!1!bad%00"}) {
			for (const auto command : {tagd::HTTP_GET, tagd::HTTP_PUT}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path), tagd::TAGL_ERR);
			}
		}
	}

	void test_event_field_boundaries_and_escapes() {
		for (const std::string prefix : {"ev:", "err:"}) {
			for (const auto* type : {"", "alarm,urgent", "alarm/urgent", "alarm?urgent",
				"alarm=urgent", "alarm&urgent", "alarm%21urgent", "alarm%2521urgent",
				"alarm%25urgent"}) {
				const auto id = prefix + "t!host/?x=1,a&b#c!principal!program!session!1!" + type;
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.execute("/_/" + TAGL::encode_tagdurl_tag_id(id) + ";"),
					tagd::TAGD_OK);
				TS_ASSERT_EQUALS(input.tag().id(), id);
			}
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			const auto id = prefix + "t!host/?x=1,a&b#c!principal!program!session!1!alarm";
			TS_ASSERT_EQUALS(input.execute("/_/" + id + "?c=;"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.tag().id(), id);
		}
	}

	void test_type_query_mapping() {
		for (const auto* subject : {"mammal,quadruped", "mammal,quadruped,carnivore",
			",quadruped", ",quadruped,carnivore"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			const std::string path = std::string("/_/") + subject + "/legs?q=can+bark";
			TS_ASSERT_EQUALS(input.execute(path + ";"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.cmd(), TOK_CMD_QUERY);
			TS_ASSERT_EQUALS(input.tag().id(), HARD_TAG_INTERROGATOR);
			TS_ASSERT_EQUALS(input.tag().super_object(), subject[0] == ',' ? "" : "mammal");
			TS_ASSERT(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "quadruped"}));
			TS_ASSERT_EQUALS(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "carnivore"}),
				std::string(subject).find("carnivore") != std::string::npos);
			TS_ASSERT(input.tag().related("legs"));
			TS_ASSERT(input.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark"));
		}
	}

	void test_explicit_get_with_query_or_type_list() {
		for (const auto* command : {"<<", "??"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string(command) + " /_/animal,legs/;"),
				tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.cmd(), TOK_CMD_QUERY);
			TS_ASSERT(input.tag().type_relations.contains({tagd::HARD_TAG_TYPE_OF, "legs"}));
		}
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("<< /_/animal,legs;"), tagd::TS_MISUSE);
	}

	void test_type_paths_validate_syntax_before_command() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE}) {
			for (const auto* path : {"/mammal,/", "/,/", "/mammal,,quadruped/",
				"/,quadruped=x/", "/mammal,", "/*,quadruped/"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
			}
			for (const auto* path : {"/dog,quadruped", "/,quadruped"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TS_MISUSE);
			}
		}
	}

	void test_write_query_paths_are_rejected_before_callbacks() {
		class capture : public TAGL::callback {
		public:
			unsigned mutations = 0;
			void cmd_get(const tagd::abstract_tag&) override {
			}
			void cmd_query(const tagd::interrogator&) override {
			}
			void cmd_put(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_delete(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_store(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_delete_storage(const tagd::abstract_tag&) override {
				++mutations;
			}
			void cmd_error() override {
			}
		};
		for (const auto* command : {">>", "!!", ">>+", "!!+"}) {
			for (const auto* path : {"/_/animal/", "/_/mammal,quadruped/", "/_/dog,quadruped"}) {
				tagspace_tester tsp;
				capture cb;
				TAGL::driver input(&tsp, &cb);
				TS_ASSERT_EQUALS(input.execute(std::string(command) + " " + path + ";"),
					tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(cb.mutations, 0u);
			}
		}
	}

	void test_context_lists_and_missing_contexts_for_every_command() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(command, "/dog?c=simple_english,japanese",
				TAGL::tagdurl_mode::constrain_body),
				tagd::TAGD_OK);
			TS_ASSERT(input.session_ptr());
			if (input.session_ptr()) {
				const tagd::id_vec expected{"simple_english", "japanese"};
				TS_ASSERT_EQUALS(input.session_ptr()->context(), expected);
			}
			input.finish();
			input.clear_context_levels();
			TS_ASSERT_EQUALS(input.scan_tagdurl(command, "/dog?c=simple_english,absent",
				TAGL::tagdurl_mode::constrain_body),
				tagd::TS_NOT_FOUND);
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_unknown_options_and_unencoded_url_path() {
		for (const auto* path : {"/dog?unknown=value", "/dog?c=japanese&&q=text"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_GET, path,
				TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
		}
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_PUT, "/https://example.com/file",
			TAGL::tagdurl_mode::constrain_body),
			tagd::TAGL_ERR);
	}

	void test_empty_context_elements_are_syntax_errors_before_command_checks() {
		for (const auto command : {TOK_CMD_GET, TOK_CMD_PUT, TOK_CMD_DELETE,
			TOK_CMD_STORE, TOK_CMD_DELETE_STORAGE}) {
			for (const auto* path : {"/dog?c=,japanese", "/dog?c=japanese,,simple_english",
				"/dog?c=japanese,", "/animal/?c=japanese%2C%2Csimple_english"}) {
				tagspace_tester tsp;
				TAGL::driver input(&tsp);
				TS_ASSERT_EQUALS(input.scan_tagdurl(command, path,
					TAGL::tagdurl_mode::constrain_body), tagd::TAGL_ERR);
				TS_ASSERT(input.constrain_tag_id.empty());
				if (input.session_ptr())
					TS_ASSERT(input.session_ptr()->context().empty());
			}
		}
	}

	void test_option_values_decode_once() {
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("/_/animal/?q=c%2B%2B+language;"), tagd::TAGD_OK);
		TS_ASSERT(input.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "c++ language"));
	}

	void test_empty_context_option_remains_accepted() {
		for (const auto* path : {"/dog?c="}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_GET, path,
				TAGL::tagdurl_mode::constrain_body), tagd::TAGD_OK);
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_unknown_context_diagnostic_for_session_and_tagdurl() {
		tagspace_tester tsp;
		auto session = tsp.get_session();
		TS_ASSERT_EQUALS(session.push_context("absent"), tagd::TS_NOT_FOUND);
		std::ostringstream session_errors;
		session.print_errors(session_errors);
		TS_ASSERT_DIFFERS(session_errors.str().find("unknown context: absent"), std::string::npos);

		TAGL::driver input(&tsp);
		TS_ASSERT_EQUALS(input.execute("<< /_/dog?c=absent;"), tagd::TS_NOT_FOUND);
		std::ostringstream errors;
		input.print_errors(errors);
		TS_ASSERT_DIFFERS(errors.str().find("unknown context: absent"), std::string::npos);
	}

	void test_embedded_put_and_store_resolve_hduri_subject_with_context() {
		for (const auto* command : {">>", ">>+"}) {
			tagspace_tester tsp;
			TAGL::driver input(&tsp);
			TS_ASSERT_EQUALS(input.execute(std::string(command)
				+ " /_/hd:org!wikipedia!en!/wiki/Dog!!!!!!https?c=japanese;"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(input.tag().id(), "https://en.wikipedia.org/wiki/Dog");
			TS_ASSERT_EQUALS(input.cmd(), std::string(command) == ">>" ? TOK_CMD_PUT : TOK_CMD_STORE);
			TS_ASSERT(input.constrain_tag_id.empty());
			TS_ASSERT(input.session_ptr());
			if (input.session_ptr())
				TS_ASSERT(input.session_ptr()->context().empty());
		}
	}

	void test_put_hduri_with_context() {
		tagspace_tester tsp;
		TAGL::driver input(&tsp);
		const std::string id = "hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		TS_ASSERT_EQUALS(input.scan_tagdurl(TOK_CMD_PUT, "/" + id + "?c=japanese",
			TAGL::tagdurl_mode::constrain_body),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.constrain_tag_id, id);
	}

	void test_cmd_get_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog", TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_get_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_GET, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_delete_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DELETE, "/dog",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DELETE)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_http_del_tag(void) {
		tagspace_tester tsp;
		callback_tester cb(&tsp);
		TAGL::driver tagl(&tsp, &cb);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_DELETE, "/dog");
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_DELETE)
		TS_ASSERT_EQUALS(cb.last_tag->id(), "dog")
	}

	void test_cmd_get_hduri_tag(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		const char *hduri = "/hd:org!wikipedia!en!/wiki/Dog!!!!!!https";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, hduri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), "https://en.wikipedia.org/wiki/Dog")
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_URL)
	}

	void test_cmd_get_event_error_uri_tag(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		const char *evuri = "/ev:2026-04-09T04:00:56.738Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!1!_event";
		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, evuri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), evuri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_EVENT)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_TAG)

		const char *erruri = "/err:2026-04-09T04:00:56.739Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!2!_error:ts_not_found";
		tc = tagl.scan_tagdurl(TOK_CMD_GET, erruri, TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
		TS_ASSERT_EQUALS(tagl.cmd(), TOK_CMD_GET)
		TS_ASSERT_EQUALS(tagl.tag().id(), erruri + 1)
		TS_ASSERT_EQUALS(tagl.tag().super_object(), HARD_TAG_ERROR_TS_NOT_FOUND)
		TS_ASSERT_EQUALS(tagl.tag().pos(), tagd::POS_ERROR)
	}

	// mirrors Tester.h `test_subject(void)`, but uses a tagdurl subject
	void test_get_statement_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);
		tagd::code tc = tagl.execute("<< /_/dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_GET )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_del_statement_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("!! /_/dog;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_DELETE )
		TS_ASSERT_EQUALS( tagl.tag().id(), "dog" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/mammal/;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_children_placeholder(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/*/legs,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT( tagl.tag().super_object().empty() )
		TS_ASSERT( tagl.tag().related("legs") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/meow,tail;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related("meow") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_parent_relations_with_modifier(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/blood=warm,tail,bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related("", "blood", "warm") )
		TS_ASSERT( tagl.tag().related("tail") )
		TS_ASSERT( tagl.tag().related("bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_search_terms(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/mammal/?q=can+bark;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "mammal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_sub_search_terms(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.execute("?? /_/animal/?q=warm+blood;");
		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_INTERROGATOR )
		TS_ASSERT_EQUALS( tagl.tag().super_object(), "animal" )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "warm blood") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_query_search_terms_without_trailing_slash(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/animal?q=warm+blood",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_root_search_terms(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "?q=can+bark",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.cmd(), TOK_CMD_QUERY )
		TS_ASSERT_EQUALS( tagl.tag().id(), HARD_TAG_SEARCH )
		TS_ASSERT( tagl.tag().related(HARD_TAG_HAS, HARD_TAG_TERMS, "can bark") )
		TS_ASSERT( tc == tagl.code() )
	}

	void test_get_context_option(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/dog?c=simple_english",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "simple_english" )

		tagl.clear_context_levels();
	}

	void test_get_utf8_subject(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ", TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
	}

	void test_get_context_option_japanese(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/イヌ?c=japanese",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "イヌ" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "japanese" )

		tagl.clear_context_levels();
	}

	void test_get_context_option_simple_english(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_GET, "/doggy?c=simple_english",
			TAGL::tagdurl_mode::constrain_body);
		tagl.finish();

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.tag().id(), "doggy" )
		TS_ASSERT( tagl.session_ptr() != nullptr )
		TS_ASSERT_EQUALS( tagl.session_ptr()->context().size(), 1 )
		if (tagl.session_ptr()->context().size() == 1)
			TS_ASSERT_EQUALS( tagl.session_ptr()->context()[0], "simple_english" )

		tagl.clear_context_levels();
	}

	void test_put_empty_tagdurl(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_slash_empty_tagdurl(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGL_ERR" )
	}

	void test_put_trailing_slash_tagdurl(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog/", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_put_illegal_search(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/pigeon?q=oops",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

	void test_cmd_put_constrain_tag_id(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_PUT, "/dog", TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_http_put_constrain_tag_id(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(tagd::HTTP_PUT, "/dog");

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TAGD_OK" )
		TS_ASSERT_EQUALS( tagl.constrain_tag_id, "dog" )
		TS_ASSERT_EQUALS( tagl.cmd(), -1 )
	}

	void test_del_illegal_search(void) {
		tagspace_tester tsp;
		TAGL::driver tagl(&tsp);

		tagd::code tc = tagl.scan_tagdurl(TOK_CMD_DELETE, "/bat?q=oops",
			TAGL::tagdurl_mode::constrain_body);

		TS_ASSERT_EQUALS( TAGD_CODE_STRING(tc), "TS_MISUSE" )
	}

};
