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
