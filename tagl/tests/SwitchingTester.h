#pragma once
#include "tagl.h"
#include "tagspace-config.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <event2/buffer.h>
#include <unistd.h>

class SwitchingTester : public CxxTest::TestSuite {
	class writer : public TAGL::callback {
	public:
		void cmd_get(const tagd::abstract_tag&) override {}
		void cmd_query(const tagd::interrogator&) override {}
		void cmd_error() override {}

		void cmd_put(const tagd::abstract_tag& t) override {
			auto rc = _driver->tsp()->put(t, _driver->session_ptr(), _driver->flags);
			if (rc != tagd::TAGD_OK) {
				_driver->copy_errors(*_driver->tsp());
				_driver->code(rc);
			}
		}

		void cmd_del(const tagd::abstract_tag& t) override {
			_driver->code(_driver->tsp()->del(t, _driver->session_ptr()));
		}
	};
public:
	void test_explicit_url_declaration_preserves_decoded_components() {
		class capture : public writer {
		public:
			std::string id, path;
			void cmd_put(const tagd::abstract_tag& value) override {
				const auto* parsed = dynamic_cast<const tagd::url*>(&value);
				TS_ASSERT(parsed);
				if (parsed) {
					id = parsed->id();
					path = parsed->path();
				}
			}
		} cb;
		tagd::tagspace::memory space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		TAGL::driver input(&space, &cb);
		const tagd::url original("https://example.com/path?q=1");
		TS_ASSERT_EQUALS(input.execute(">> HTTPS://example.com/path?q=1 _sub _url;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cb.id, original.id());
		TS_ASSERT_EQUALS(cb.path, original.path());
	}

	void test_url_identity_is_validated_by_tagspace_for_both_commands() {
		for (const auto* command : {">>", ">>+"}) {
			tagd::tagspace::memory space;
			TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
			class declaration_writer : public writer {
			public:
				void cmd_store(const tagd::abstract_tag& value) override {
					cmd_put(value);
				}
			} cb;
			TAGL::driver input(&space, &cb);
			const std::string prefix = std::string(command) + " file:///tmp/declaration";
			TS_ASSERT_EQUALS(input.execute(prefix + " _sub _file;"), tagd::TAGD_OK);
			input.clear_errors();
			TS_ASSERT_EQUALS(input.execute(prefix + " _sub _url;"), tagd::TS_MISUSE);
		}
	}

	void test_store_operands_are_data_and_predicate_fragments_are_single_statements() {
		class capture : public writer {
		public:
			unsigned calls = 0;
			tagd::abstract_tag stored;
			void cmd_store(const tagd::abstract_tag& value) override {
				++calls;
				stored = value;
			}
		} cb;
		tagd::tagspace::memory space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		TAGL::driver input(&space, &cb);
		const tagd::url source("file:///tmp/quotes;\" >> text");
		const tagd::url invalid("http://");
		TS_ASSERT_EQUALS(input.execute_store(invalid, ""), invalid.code());
		TS_ASSERT_EQUALS(cb.calls, 0u);
		input.clear_errors();
		TS_ASSERT_EQUALS(input.execute_store(source, "_has _message = \"beach; friends\""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cb.calls, 1u);
		TS_ASSERT_EQUALS(cb.stored.id(), source.id());
		TS_ASSERT(cb.stored.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "beach; friends"));
		for (const auto* suffix : {"; >> unwanted _sub _entity;", "%% _ignore_duplicates _true;",
			"_has _message = ok\n\n", "_has _message =", "-- comment only", "_sub _file"}) {
			TS_ASSERT_DIFFERS(input.execute_store(source, suffix), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(cb.calls, 1u);
			input.clear_errors();
		}
		TS_ASSERT_EQUALS(input.execute_store(source, "_has _message = recovered"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cb.calls, 2u);
	}

	void test_filepile_event_message_round_trip() {
		class capture : public writer {
		public:
			tagd::abstract_tag parsed;
			void cmd_put(const tagd::abstract_tag& value) override {
				parsed = value;
			}
		} cb;
		tagd::tagspace::memory space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		auto session = space.get_session();
		tagd::event ev(session, "filepile", std::string(tagd::HARD_TAG_FILEPILE_DELETE_EVENT));
		const std::string message = "DELETE file:///tmp/source; result=TAGD_OK; changed=true";
		TS_ASSERT_EQUALS(ev.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, message), tagd::TAGD_OK);
		std::ostringstream serialized;
		serialized << ">> " << ev << ';';
		TAGL::driver input(&space, &cb);
		TS_ASSERT_EQUALS(input.execute(serialized.str()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cb.parsed.id(), ev.id());
		TS_ASSERT(cb.parsed.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, message));
	}

	void test_default_storage_callback_leaves_warning_output_to_frontends() {
		class warning_space : public tagd::stored_tagspace {
		public:
			tagd::code init() {
				return initialize_memory();
			}
			tagd::code delete_storage(tagd::filepile_delete_result& result,
				const tagd::abstract_tag&, tagd::tagspace_session*, tagd::flags_t) override {
				result.warnings.emplace_back(tagd::TS_NOT_FOUND, "stored content already missing");
				return tagd::TAGD_OK;
			}
		} space;
		TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&space, &cb);
		std::ostringstream output;
		auto* previous = std::cerr.rdbuf(output.rdbuf());
		const auto rc = input.execute("!!+ file:///tmp/missing;");
		std::cerr.rdbuf(previous);
		TS_ASSERT_EQUALS(rc, tagd::TAGD_OK);
		TS_ASSERT(output.str().empty());
	}

	void test_dump_load_preserves_string_escapes() {
		const std::string declarations = R"tagl(
>> a _sub _entity _has _message = "4\"x";
>> b _sub _entity _has _message = "x\"4";
>> c _sub _entity _has _message = "4\\x";
>> d _sub _entity _has _message = "x\\4";
>> e _sub _entity _has _message = "\\\"";
>> f _sub _entity _has _message = "a\\";
>> g _sub _entity _has _message = "\"\"";
)tagl";
		std::string serialized = declarations;
		std::string first_dump;
		for (unsigned cycle = 0; cycle != 3; ++cycle) {
			tagd::tagspace::memory space;
			TS_ASSERT_EQUALS(space.init(), tagd::TAGD_OK);
			writer cb;
			TAGL::driver input(&space, &cb);
			TS_ASSERT_EQUALS(input.execute(serialized), tagd::TAGD_OK);

			std::ostringstream dump;
			TS_ASSERT_EQUALS(space.dump(dump), tagd::TAGD_OK);
			serialized = dump.str();
			if (cycle == 0) {
				first_dump = serialized;
			} else {
				TS_ASSERT_EQUALS(serialized, first_dump);
			}
			for (const auto id : {"a", "b", "c", "d", "e", "f", "g"}) {
				tagd::abstract_tag restored;
				TS_ASSERT_EQUALS(space.get(restored, id), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(restored.relations.size(), 1u);
				if (!restored.relations.empty()) {
					const auto& value = *restored.relations.begin();
					TS_ASSERT_EQUALS(value.modifier_type, tagd::TYPE_STRING);
					TS_ASSERT_DIFFERS(declarations.find(value.modifier), std::string::npos);
				}
			}
		}
	}

	void test_type_objects_are_local_and_new_self_assertions_are_noops() {
		tagd::tagspace::memory source, target;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&source, &cb);
		TS_ASSERT_EQUALS(input.execute(">> https://example.com/type; "
			">> dog _sub _entity _type_of https://example.com/type;"), tagd::TAGD_OK);
		std::ostringstream dump;
		TS_ASSERT_EQUALS(source.dump(dump), tagd::TAGD_OK);
		writer output_callback;
		TAGL::driver output(&target, &output_callback);
		TS_ASSERT_EQUALS(output.execute(dump.str()), tagd::TAGD_OK);
		tagd::abstract_tag dog;
		TS_ASSERT_EQUALS(target.get(dog, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.type_relations.size(), 1u);
		TS_ASSERT_EQUALS(input.execute(">> dog _type_of /foreign/type;"), tagd::TS_MISUSE);
		input.clear_errors();
		TS_ASSERT_EQUALS(input.execute(">> new_subject _sub _entity _type_of new_subject, _entity;"),
			tagd::TAGD_OK);
		tagd::abstract_tag created;
		TS_ASSERT_EQUALS(source.get(created, "new_subject"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(created.super_object(), tagd::HARD_TAG_ENTITY);
		TS_ASSERT(created.type_relations.empty());
	}

	void test_expanded_get_and_explicit_dump_preserve_value_metadata() {
		tagd::tagspace::memory source, target;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&source, &cb);
		TS_ASSERT_EQUALS(input.execute(">> legs _sub _entity; >> label _sub _entity; "
			">> quadruped _sub _entity _has legs = 4; "
			">> dog _sub _entity _type_of quadruped _has legs = 4.0, label = \"4\";"), tagd::TAGD_OK);
		tagd::abstract_tag attack("comment*- >> injected _sub _entity; -*", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(attack.relation(tagd::HARD_TAG_HAS, "label", "4"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(source.put(attack), tagd::TAGD_OK);
		tagd::abstract_tag dog("dog");
		TS_ASSERT_EQUALS(dog.type_relation(tagd::HARD_TAG_TYPE_OF, attack.id()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(source.put(dog), tagd::TAGD_OK);
		std::ostringstream dump;
		TS_ASSERT_EQUALS(source.dump(dump), tagd::TAGD_OK);
		TAGL::driver output(&target, &cb);
		const auto loaded = output.execute(dump.str());
		if (loaded != tagd::TAGD_OK) {
			TS_TRACE(dump.str());
			output.print_errors();
		}
		TS_ASSERT_EQUALS(loaded, tagd::TAGD_OK);
		TS_ASSERT_EQUALS(source.get(dog, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.relations.size(), 3u);
		TS_ASSERT(!target.exists("injected"));
		tagd::abstract_tag restored;
		TS_ASSERT_EQUALS(target.get(restored, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(restored.relations.size(), 2u);
		TS_ASSERT_EQUALS(restored.type_relations.size(), 2u);
		for (const auto& p : restored.relations) {
			TS_ASSERT_EQUALS(p.modifier_type, p.object == "legs" ? tagd::TYPE_FLOAT : tagd::TYPE_STRING);
		}
	}

	void test_expanded_tagl_can_be_put_as_explicit_predicates() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&ts, &cb);
		TS_ASSERT_EQUALS(input.execute(">> quadruped _sub _entity _has _message = inherited; "
			">> dog _sub _entity _type_of quadruped;"), tagd::TAGD_OK);
		tagd::abstract_tag dog;
		TS_ASSERT_EQUALS(ts.get(dog, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.str(), "dog _sub _entity\n_type_of quadruped\n_has _message = inherited");
		TS_ASSERT_EQUALS(input.execute(">> " + dog.str() + ";"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(input.execute("!! dog _type_of quadruped;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(dog, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.relations.size(), 1u);
		TS_ASSERT(dog.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "inherited"));
	}

	void test_type_relations_parse_without_changing_identity_and_round_trip() {
		tagd::tagspace::memory source, target;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&source, &cb);
		TS_ASSERT_EQUALS(input.execute(">> animal _sub _entity; >> mobile _sub _entity; "
			">> quadruped _sub animal; >> classifies _sub _type_of; "
			">> dog _is_a animal _type_of quadruped, mobile classifies mobile;"), tagd::TAGD_OK);
		tagd::abstract_tag dog;
		TS_ASSERT_EQUALS(source.get(dog, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.super_object(), "animal");
		TS_ASSERT_EQUALS(dog.type_relations.size(), 3u);
		TS_ASSERT(dog.relations.empty());

		for (const auto suffix : {"= 4", "< 4", "= \"\"", "= NULL"}) {
			input.clear_errors();
			TS_ASSERT_EQUALS(input.execute(">> dog _type_of mobile " + std::string(suffix) + ";"),
				tagd::TAGL_ERR);
		}
		input.clear_errors();
		TS_ASSERT_EQUALS(input.execute(">> dog _type_of dog;"),
			tagd::TAGD_OK);
		std::ostringstream dump;
		TS_ASSERT_EQUALS(source.dump(dump), tagd::TAGD_OK);
		TAGL::driver output(&target, &cb);
		TS_ASSERT_EQUALS(output.execute(dump.str()), tagd::TAGD_OK);
		tagd::abstract_tag restored;
		TS_ASSERT_EQUALS(target.get(restored, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(restored.type_relations, dog.type_relations);
		TS_ASSERT_EQUALS(restored.sub_relator(), dog.sub_relator());
	}

	void test_malformed_hduri_does_not_mutate_storage() {
		tagd::tagspace::memory source;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&source, &cb);
		TS_ASSERT_EQUALS(input.execute(">> http://example.com/;"), tagd::TAGD_OK);
		std::ostringstream before;
		TS_ASSERT_EQUALS(source.dump(before), tagd::TAGD_OK);

		for (const auto hduri : {"hd:com!example!!/%20!!!!!!http",
			"hd:com!example!!/a?b!!!!!!http", "hd:com!example!http"}) {
			input.clear_errors();
			TS_ASSERT_EQUALS(input.execute(">> " + std::string(hduri) + ";"), tagd::HDURI_ERR);
			std::ostringstream after;
			TS_ASSERT_EQUALS(source.dump(after), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(after.str(), before.str());
		}
	}

	void test_distinct_url_identities_survive_storage_and_dump_load() {
		tagd::tagspace::memory source, target;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver input(&source, &cb);
		const std::vector<std::string> ids{
			"http://example.com/a!b?x=1#f", "http://example.com/a%21b?x=1#f",
			"http://example.com/a%2521b?x=1#f", "http://joe:@example.com/",
			"http://joe:%00@example.com/", "file:///tmp/a%21b", "file:///tmp/a%2521b",
			"file:///tmp/" + std::string(2100, '%')
		};
		for (const auto& id : ids) {
			const tagd::url url(id);
			TS_ASSERT_EQUALS(input.execute(">> " + url.hduri() + " _has _message = \"" + id + "\";"),
				tagd::TAGD_OK);
		}

		std::ostringstream text;
		TS_ASSERT_EQUALS(source.dump(text), tagd::TAGD_OK);
		TAGL::driver output(&target, &cb);
		TS_ASSERT_EQUALS(output.execute(text.str()), tagd::TAGD_OK);
		for (auto* space : {&source, &target}) {
			for (const auto& id : ids) {
				tagd::url fetched;
				const tagd::url expected(id);
				TS_ASSERT_EQUALS(space->get(fetched, expected.hduri()), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(fetched.id(), id);
				TS_ASSERT_EQUALS(fetched.path(), expected.path());
				TS_ASSERT_EQUALS(fetched.query(), expected.query());
				TS_ASSERT(fetched.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, id));
			}
		}
	}

	void test_filepile_escaped_names_survive_reopen_and_canonical_dump() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagl-hduri-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		if (!directory) {
			throw std::runtime_error("cannot create test directory");
		}
		const std::filesystem::path root(directory);
		struct cleanup {
			std::filesystem::path path;
			~cleanup() {
				std::filesystem::remove_all(path);
			}
		} remove{root};
		std::ofstream(root / "a!b") << "bang";
		std::ofstream(root / "a%21b") << "percent";
		const std::string first = "file://" + root.string() + "/a%21b";
		const std::string second = "file://" + root.string() + "/a%2521b";
		tagd::store_result one, two;
		{
			tagd::tagspace::persistent source;
			TS_ASSERT_EQUALS(source.create("files", root.string()), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(source.store(one, tagd::url(first)), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(source.store(two, tagd::url(second)), tagd::TAGD_OK);
			TS_ASSERT_DIFFERS(one.link, two.link);
			TS_ASSERT_DIFFERS(one.stored.id(), two.stored.id());
		}

		tagd::tagspace::persistent reopened;
		TS_ASSERT_EQUALS(reopened.init("files", root.string()), tagd::TAGD_OK);
		tagd::store_result repeated;
		TS_ASSERT_EQUALS(reopened.store(repeated, tagd::url(first)), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(repeated.link, one.link);
		TS_ASSERT_EQUALS(reopened.store(repeated, tagd::url(second)), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(repeated.link, two.link);
		std::ofstream(root / "a!b") << "changed bang";
		tagd::store_result next;
		TS_ASSERT_EQUALS(reopened.store(next, tagd::url(first)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(next.revision, 2U);
		TS_ASSERT_EQUALS(reopened.store(repeated, tagd::url(second)), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(repeated.revision, 1U);

		std::ostringstream text;
		TS_ASSERT_EQUALS(reopened.dump(text), tagd::TAGD_OK);

		tagd::tagspace::memory target;
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver loader(&target, &cb);
		TS_ASSERT_EQUALS(loader.execute(text.str()), tagd::TS_MISUSE);
		// Ordinary loading cannot recreate managed records; explicit restore is separate work.
		for (const auto* result : {&one, &two, &next}) {
			tagd::url fetched;
			TS_ASSERT_EQUALS(reopened.get(fetched, result->source.id()), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(fetched.id(), result->source.id());
			const auto embedded = result->link.substr(5, result->link.rfind('#') - 5);
			TS_ASSERT_EQUALS(tagd::url(embedded).id(), result->source.id());

			TS_ASSERT_DIFFERS(text.str().find(result->link), std::string::npos);
			tagd::abstract_tag link;
			TS_ASSERT_EQUALS(reopened.get(link, result->link), tagd::TAGD_OK);
			TS_ASSERT(link.related(tagd::HARD_TAG_FROM, result->source.hduri()));
			TS_ASSERT(link.related(tagd::HARD_TAG_TO, result->stored.hduri()));
			TS_ASSERT(link.related(tagd::predicate(tagd::HARD_TAG_HAS, tagd::HARD_TAG_REVISION,
				std::to_string(result->revision), tagd::OP_EQ, tagd::TYPE_INTEGER)));
		}
	}

	void test_ordinary_load_cannot_restore_managed_classification() {
		tagd::tagspace::memory target;
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&target, &cb);
		TS_ASSERT_EQUALS(d.execute(">> file:///tmp/stored.bin _type_of _file:stored;"), tagd::TS_MISUSE);
		TS_ASSERT(!target.exists(tagd::url("file:///tmp/stored.bin").hduri()));
	}

	void test_tag_id_byte_bounds_survive_dump_load() {
		tagd::tagspace::memory source, target;
		TS_ASSERT_EQUALS(source.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target.init(), tagd::TAGD_OK);
		std::string id;
		for (size_t i = 0; i < tagd::MAX_TAG_LEN / 2; ++i) {
			id += "é";
		}
		TS_ASSERT_EQUALS(id.size(), tagd::MAX_TAG_LEN);
		TS_ASSERT_EQUALS(source.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(source.put(tagd::abstract_tag(id + "x", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TS_ERR_MAX_TAG_LEN);
		std::ostringstream text;
		TS_ASSERT_EQUALS(source.dump(text), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&target, &cb);
		TS_ASSERT_EQUALS(d.execute(text.str()), tagd::TAGD_OK);
		TS_ASSERT(target.exists(id));
	}

	void test_addressed_tagdurls_preserve_active_selection() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);
		TS_ASSERT_EQUALS(d.execute("%% _tagspace _default = mem;"), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(d.execute("/mem/_entity;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), &cfg);
		TS_ASSERT_EQUALS(d.execute("/_default;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tag().id(), tagd::HARD_TAG_DEFAULT);
		TS_ASSERT_EQUALS(d.execute("/_config/;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.cmd(), TOK_CMD_QUERY);
		TS_ASSERT_EQUALS(d.tsp(), &cfg);

		for (const auto& path : {"//mem/dog;", "/mem//dog;", "/%2e%2e/dog;",
			"/mem%2Fother/dog;", "/bad%/dog;"}) {
			d.clear_errors();
			TS_ASSERT_EQUALS(d.execute(path), tagd::TAGL_ERR);
			TS_ASSERT_EQUALS(d.tsp(), &cfg);
		}
	}

	void test_tagdurl_autoload_is_live_and_failure_restores_selection() {
		char pattern[] = "/tmp/tagl-autoload-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		{
			tagd::tagspace::persistent ts;
			TS_ASSERT_EQUALS(ts.create("animals", root.string()), tagd::TAGD_OK);
		}
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);
		TS_ASSERT_EQUALS(d.execute("/animals/_entity;"), tagd::TS_NOT_FOUND);
		TS_ASSERT(!cfg.exists("animals"));
		TS_ASSERT_EQUALS(d.tsp(), &cfg);

		d.clear_errors();
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_TAGDURL_AUTOLOAD, "_true"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.execute("/animals/_entity;"), tagd::TAGD_OK);
		TS_ASSERT(cfg.exists("animals"));
		TS_ASSERT_EQUALS(d.tsp(), &cfg);
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_TAGDURL_AUTOLOAD, "_false"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.execute("/animals/_entity;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), &cfg);

		d.clear_errors();
		TS_ASSERT_EQUALS(d.execute("/animals/_entity?c=absent;"), tagd::TAGD_OK);
		TS_ASSERT(d.session_ptr()->context().empty());
	}

	void test_config_set_is_deferred_and_restricted() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.begin_config_load(), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg, false, true);

		TS_ASSERT_EQUALS(d.parseln("%% _config_id later; >> later _sub _config;;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.finish_config_load("", ""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.subject_id(), "later");

		d.configuration_loading(false);
		TS_ASSERT_EQUALS(d.parseln("%% _config_id later;;"), tagd::TS_MISUSE);
	}

	void test_selection_session_and_callback_share_target() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> memory;
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "", memory), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(memory.get(), &cb);
		d.configure(cfg);

		TS_ASSERT_EQUALS(d.parseln(">> japanese _sub _entity; %% _context japanese;;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.parseln("%% _use _config; >> profile _sub _config;;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), &cfg);
		TS_ASSERT(cfg.exists("profile"));
		TS_ASSERT(!memory->exists("profile"));
		TS_ASSERT_EQUALS(d.session_ptr()->context().back(), "japanese");

		TS_ASSERT_EQUALS(d.session_ptr()->push_context("profile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.parseln("%% !! _context;;"), tagd::TAGD_OK);
		TS_ASSERT(d.session_ptr()->context().empty());

		TS_ASSERT_EQUALS(d.parseln("%% _use _default;;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), memory.get());
	}

	void test_database_commands_and_failed_close_preserve_selection() {
		char pattern[] = "/tmp/tagl-switch-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);

		TS_ASSERT_EQUALS(d.execute("%% _tagspace animals = a; %% _use a; >> dog _sub _entity;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("dog"));
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TS_MISUSE);

		TS_ASSERT_EQUALS(d.execute("%% _use _default; %% !! _tagspace animals = a; %% !! _tagspace animals;"), tagd::TAGD_OK);
		TS_ASSERT(cfg.exists("animals"));
	}

	void test_tagl_sources_publish_after_success_and_keep_script_effects() {
		char pattern[] = "/tmp/tagl-sources-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		std::ofstream(root / "animals.tagl") << ">> dog _sub _entity;";
		std::ofstream(root / "switch.tagl") << ">> local _sub _entity; %% _use _default; >> effect _sub _entity;";
		std::ofstream(root / "failure.tagl") << "%% _use _default; >> retained _sub _entity; >> broken _sub nonexistent;";

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);

		d.path((root / "caller.tagl").string());
		TS_ASSERT_EQUALS(d.execute("%% _tagspace ./animals.tagl = a;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), &cfg);
		TS_ASSERT_EQUALS(d.execute("%% _use a;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("dog"));

		d.path((root / "caller.tagl").string());
		TS_ASSERT_EQUALS(d.execute("%% _tagspace ./switch.tagl;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("effect"));
		TS_ASSERT(!d.tsp()->exists("local"));
		TS_ASSERT_EQUALS(d.execute("%% _use switch;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("local"));

		d.path((root / "caller.tagl").string());
		TS_ASSERT_DIFFERS(d.execute("%% _use ./failure.tagl;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("retained"));
		TS_ASSERT(!cfg.exists("failure"));
	}

	void test_includes_share_selection_and_config_restrictions() {
		char pattern[] = "/tmp/tagl-includes-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		std::ofstream(root / "change.tagl") << "%% _use _default; >> included _sub _entity;";

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);
		d.path((root / "caller.tagl").string());
		TS_ASSERT_EQUALS(d.execute("%% _include change.tagl; >> after _sub _entity;"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("included"));
		TS_ASSERT(d.tsp()->exists("after"));

		TAGL::driver config_loader(&cfg, &cb);
		config_loader.configure(cfg, false, true);
		config_loader.path((root / "config.tagl").string());
		TS_ASSERT_EQUALS(config_loader.execute("%% _include change.tagl;"), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(config_loader.tsp(), &cfg);
	}

	void test_file_url_description_uses_normal_tagl() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);

		TS_ASSERT_EQUALS(d.execute(">> file:///tmp/tagd-description-only.db; "
			">> animals _sub _tagspace _from file:///tmp/tagd-description-only.db;"), tagd::TAGD_OK);
		TS_ASSERT(cfg.exists("animals"));
	}

	void test_http_selection_isolation_and_actual_target_protection() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> memory;
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "", memory), tagd::TAGD_OK);
		writer first_cb, second_cb;
		TAGL::driver first(&cfg, &first_cb), second(&cfg, &second_cb);
		first.configure(cfg, true);
		second.configure(cfg, true);

		TS_ASSERT_EQUALS(first.execute("%% _use _default;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.tsp(), memory.get());
		TS_ASSERT_EQUALS(second.tsp(), &cfg);

		TS_ASSERT_EQUALS(first.execute("%% _use _config; >> injected _sub _entity;"), tagd::TS_MISUSE);
		TS_ASSERT(first.http_forbidden());
		TS_ASSERT(!cfg.exists("injected"));
	}

	void test_http_body_cannot_invoke_local_filepile_commands() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg, true);
		TS_ASSERT_EQUALS(d.execute("%% _use _default; >> file:///not-acquired;"), tagd::TAGD_OK);
		for (const auto command : {">>+", "!!+"}) {
			d.clear_errors();
			TS_ASSERT_EQUALS(d.execute(std::string(command) + " file:///not-acquired;"),
				tagd::TS_NOT_IMPLEMENTED);
			TS_ASSERT(d.tsp()->exists(tagd::url("file:///not-acquired").hduri()));
		}
	}

	void test_http_cannot_include_or_close_shared_instances() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg, true);

		TS_ASSERT_EQUALS(d.execute("%% !! _tagspace _default;"), tagd::TS_MISUSE);
		TS_ASSERT(d.http_forbidden());

		d.clear_errors();
		TS_ASSERT_EQUALS(d.execute("%% _include \"/tmp/not-opened.tagl\";"), tagd::TS_MISUSE);
		TS_ASSERT(d.http_forbidden());
	}

	void test_failed_alias_publication_preserves_only_script_effects() {
		char pattern[] = "/tmp/tagl-publication-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		std::ofstream(root / "animals.tagl") << "%% _use _config; >> occupied _sub _entity;";

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);
		d.path((root / "caller.tagl").string());

		TS_ASSERT_EQUALS(d.execute("%% _tagspace ./animals.tagl = occupied;"), tagd::TS_MISUSE);
		TS_ASSERT(cfg.exists("occupied"));
		TS_ASSERT(!cfg.exists("animals"));
		TS_ASSERT_EQUALS(d.tsp(), &cfg);
	}

	void test_quoted_relative_sources_and_use_after_script_switch() {
		char pattern[] = "/tmp/tagl-paths-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		std::filesystem::create_directory(root / "with spaces");
		std::ofstream(root / "with spaces/animals.tagl")
			<< ">> candidate _sub _entity; %% _use _default; >> retained _sub _entity;";
		std::ofstream(root / "bad.tagl") << ">> bad _sub absent;";

		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg);

		d.path((root / "caller.tagl").string());
		TS_ASSERT_EQUALS(d.execute("%% _use \"./with spaces/animals.tagl\";"), tagd::TAGD_OK);
		TS_ASSERT(d.tsp()->exists("candidate"));
		TS_ASSERT(!d.tsp()->exists("retained"));

		auto* previous = d.tsp();
		d.path((root / "caller.tagl").string());
		TS_ASSERT_DIFFERS(d.execute("%% _use ./bad.tagl;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(d.tsp(), previous);
		TS_ASSERT(!cfg.exists("bad"));
	}

	void test_file_url_deletion_and_http_alias_method_policy() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver local(&cfg, &cb);
		local.configure(cfg);
		TS_ASSERT_EQUALS(local.execute(">> file:///tmp/deletable.db; !! file:///tmp/deletable.db;"), tagd::TAGD_OK);

		TAGL::driver request(&cfg, &cb);
		request.configure(cfg, true);
		request.http_aliases(false);
		TS_ASSERT_EQUALS(request.execute("%% _tagspace _default = denied;"), tagd::TS_MISUSE);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("denied", id), tagd::TS_NOT_FOUND);
	}

	void test_malformed_set_reports_and_recovers_at_next_execution() {
		for (const auto& input : {"%% _use foo, bar;", "%% _tagspace foo, bar;", "%% _use \"unterminated"}) {
			tagd::config cfg;
			TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
			writer cb;
			TAGL::driver d(&cfg, &cb);
			d.configure(cfg);

			TS_ASSERT_EQUALS(d.execute(input), tagd::TAGL_ERR);
			TS_ASSERT(d.has_errors());

			d.clear_errors();
			TS_ASSERT_EQUALS(d.execute(">> recovered _sub _entity;"), tagd::TAGD_OK);
			TS_ASSERT(cfg.exists("recovered"));
		}
	}

	void test_http_config_access_depends_on_initial_target_not_alias_spelling() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_alias("_config", "settings"), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> memory;
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "", memory), tagd::TAGD_OK);

		for (const auto& command : {"%% _use _config;", "%% _tagspace settings = leaked;"}) {
			writer cb;
			TAGL::driver d(memory.get(), &cb);
			d.configure(cfg, true);
			TS_ASSERT_EQUALS(d.execute(command), tagd::TS_MISUSE);
			TS_ASSERT(d.http_forbidden());
			TS_ASSERT_EQUALS(d.tsp(), memory.get());
			tagd::id_string id;
			TS_ASSERT_EQUALS(cfg.resolve_tagspace("leaked", id), tagd::TS_NOT_FOUND);
		}

		writer cb;
		TAGL::driver allowed(&cfg, &cb);
		allowed.configure(cfg, true);
		TS_ASSERT_EQUALS(allowed.execute("%% _use _default; %% _use settings;"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(allowed.tsp(), &cfg);
		TS_ASSERT_EQUALS(allowed.execute(">> forbidden _sub _entity;"), tagd::TS_MISUSE);
		TS_ASSERT(allowed.http_forbidden());
	}

	void test_http_client_error_is_not_an_authorization_denial() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		writer cb;
		TAGL::driver d(&cfg, &cb);
		d.configure(cfg, true);

		TS_ASSERT_EQUALS(d.execute("%% _tagspace _default = _bad;"), tagd::TS_MISUSE);
		TS_ASSERT(!d.http_forbidden());
	}

	void test_stream_refill_preserves_tokens_and_final_terminators() {
		// Exercise the scanner's refill boundary, not string-input scanning.
		for (size_t length : {TAGL::BUF_SZ - 1, TAGL::BUF_SZ, TAGL::BUF_SZ + 1, TAGL::BUF_SZ + 20}) {
			tagd::config cfg;
			TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
			writer cb;
			TAGL::driver d(&cfg, &cb);
			d.configure(cfg);

			std::string statement = ">> boundary _sub _entity _has _message = \"quoted value\";";
			std::string input(length - statement.size(), ' ');
			input += statement;
			auto* buffer = evbuffer_new();
			evbuffer_add(buffer, input.data(), input.size());
			TS_ASSERT_EQUALS(d.execute(buffer), tagd::TAGD_OK);
			evbuffer_free(buffer);

			tagd::abstract_tag result;
			TS_ASSERT_EQUALS(cfg.get(result, "boundary"), tagd::TAGD_OK);
			TS_ASSERT(result.related(HARD_TAG_HAS, HARD_TAG_MESSAGE, "quoted value"));
		}
	}
};
