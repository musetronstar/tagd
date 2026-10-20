#pragma once

#include "tagspace-config.h"
#include <cxxtest/TestSuite.h>
#include <sstream>

class ConfigTester : public CxxTest::TestSuite {
public:
	void test_file_profiles_are_selected_after_loading() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.begin_config_load(), tagd::TAGD_OK);
		tagd::abstract_tag base("base", tagd::HARD_TAG_SUB, tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(base.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8081"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(base), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("server", tagd::HARD_TAG_SUB,
			"base", tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.finish_config_load("", ""), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.finish_config_load("server", ""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.subject_id(), "server");
		TS_ASSERT_EQUALS(cfg.bind_port(), 8081);
	}

	void test_invalid_file_selection_cannot_be_hidden_by_cli() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.begin_config_load(), tagd::TAGD_OK);
		cfg.file_profile("missing", {});
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("server", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.finish_config_load("server", ""), tagd::TS_NOT_FOUND);
	}

	void test_sole_profile_and_empty_file() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.begin_config_load(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.finish_config_load("", ""), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("server", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.finish_config_load("", ""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.subject_id(), "server");
	}

	void test_file_default_inherits_without_masking_parent() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.begin_config_load(), tagd::TAGD_OK);
		tagd::abstract_tag base("base", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(base.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8082"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(base), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("default_config", tagd::HARD_TAG_SUB,
			"base", tagd::POS_TAG)), tagd::TAGD_OK);
		cfg.file_profile("default_config", {});
		TS_ASSERT_EQUALS(cfg.finish_config_load("", ""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8082);
	}

	void test_bare_config_assertions_and_modifier_deletion() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.select("default_config", ""), tagd::TAGD_OK);
		TS_ASSERT(cfg.home().empty());

		tagd::abstract_tag bare("default_config");
		TS_ASSERT_EQUALS(bare.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(bare), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);

		tagd::abstract_tag clear("default_config");
		TS_ASSERT_EQUALS(clear.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "2112"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(clear), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);

		cfg.start_runtime();
		TS_ASSERT_EQUALS(cfg.put(bare), tagd::TS_DUPLICATE);
	}

	void test_referents_resolve_before_configuration_validation() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("日本語", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG)), tagd::TAGD_OK);
		tagd::tagspace& ts = cfg;
		for (const auto& ref : {
			tagd::referent("設定", "default_config", "日本語"),
			tagd::referent("持つ", tagd::HARD_TAG_HAS, "日本語"),
			tagd::referent("ポート", tagd::HARD_TAG_CONFIG_BIND_PORT, "日本語"),
			tagd::referent("表示", tagd::HARD_TAG_CONFIG_PROMPT, "日本語")}) {
			TS_ASSERT_EQUALS(ts.put(ref), tagd::TAGD_OK);
		}

		auto ssn = cfg.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("日本語"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.pos("持つ", &ssn), tagd::POS_RELATOR);

		tagd::abstract_tag change("設定");
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "8080"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(ts.put(change, &ssn), tagd::TS_DUPLICATE);
		TS_ASSERT_DIFFERS(ts.put(change, &ssn, tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);

		tagd::abstract_tag result;
		TS_ASSERT_EQUALS(ts.get(result, "設定", &ssn), tagd::TAGD_OK);
		TS_ASSERT(result.related("持つ", "ポート", "8080"));
		TS_ASSERT_EQUALS(ts.get(result, "default_config", &ssn,
			tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);
		TS_ASSERT(result.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"));

		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "70000"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change, &ssn), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ssn.code(), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);

		cfg.start_runtime();
		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "9090"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change, &ssn), tagd::TS_MISUSE);
		TS_ASSERT_DIFFERS(ts.del(tagd::abstract_tag("設定"), &ssn), tagd::TAGD_OK);

		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "表示", "こんにちは> "), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.prompt(), "こんにちは> ");
		TS_ASSERT_EQUALS(ts.del(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT(cfg.prompt().empty());
		TS_ASSERT_EQUALS(ts.del(tagd::referent("表示", tagd::HARD_TAG_CONFIG_PROMPT,
			"日本語"), &ssn), tagd::TAGD_OK);
		TS_ASSERT(cfg.prompt().empty());
	}

	void test_referent_reads_and_deletes_preserve_inheritance() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("context", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("child", tagd::HARD_TAG_SUB,
			"default_config", tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::referent("selected", "child", "context")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::referent("prompt", tagd::HARD_TAG_CONFIG_PROMPT,
			"context")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.select("child", "/tmp/tagd-home"), tagd::TAGD_OK);
		auto ssn = cfg.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

		tagd::abstract_tag change("selected");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, "prompt", "local"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT(cfg.prompt().empty());

		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, "prompt"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.prompt(), "tagd> ");

		tagd::abstract_tag result;
		TS_ASSERT_EQUALS(cfg.get(result, "selected", &ssn,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT(result.related(tagd::HARD_TAG_HAS, "prompt", "tagd> "));

		tagd::interrogator q;
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, "prompt", "tagd> "), tagd::TAGD_OK);
		tagd::tag_set matches;
		TS_ASSERT_EQUALS(cfg.query(matches, q, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(matches.size(), 1u);
		TS_ASSERT_EQUALS(matches.begin()->id(), "default_config");

		TS_ASSERT_EQUALS(cfg.put(tagd::referent("another", "selected", "context"),
			&ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.get(result, "another", &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.super_object(), "default_config");
		TS_ASSERT_DIFFERS(cfg.put(tagd::referent("missing", "unknown", "context")), tagd::TAGD_OK);
	}

	void test_selected_profile_inherits_and_protects_effective_runtime_values() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		tagd::abstract_tag child("development", tagd::HARD_TAG_SUB,
			tagd::config::CONFIG_SUBJECT, tagd::POS_TAG);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_PROMPT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(child), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(cfg.select("development", "/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.subject_id(), "development");
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(cfg.home(), "/tmp/tagd-home");
		TS_ASSERT(cfg.prompt().empty());
		tagd::abstract_tag direct;
		TS_ASSERT_EQUALS(cfg.get(direct, "development"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.relations.size(), 2);

		tagd::abstract_tag other("other", tagd::HARD_TAG_SUB,
			tagd::config::CONFIG_SUBJECT, tagd::POS_TAG);
		TS_ASSERT_EQUALS(other.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_ADDR, "127.0.0.1"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(other), tagd::TAGD_OK);

		cfg.start_runtime();
		tagd::abstract_tag move("development", tagd::HARD_TAG_SUB, "other", tagd::POS_TAG);
		TS_ASSERT_DIFFERS(cfg.put(move), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.get(direct, "development"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.super_object(), tagd::config::CONFIG_SUBJECT);

		other.relations.clear();
		TS_ASSERT_EQUALS(other.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "7070"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(other), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);

		tagd::abstract_tag parent(tagd::config::CONFIG_SUBJECT);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "9090"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(parent), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);

		tagd::abstract_tag removal("development");
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.del(removal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);

		parent.relations.clear();
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_ADDR, "127.0.0.1"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.put(parent), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_addr(), "localhost");

		removal.relations.clear();
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_PROMPT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(removal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.prompt(), "tagd> ");

		parent.relations.clear();
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_LOG_LEVEL, "debug"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(parent), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.log().level(tagd::HARD_TAG_ROLE_TAGSH), tagd::log_level::DEBUG);
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "warning"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.log().level(tagd::HARD_TAG_ROLE_TAGSH), tagd::log_level::WARNING);

		removal.relations.clear();
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_LOG_LEVEL), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(removal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.log().level(tagd::HARD_TAG_ROLE_TAGSH), tagd::log_level::DEBUG);

		TS_ASSERT_DIFFERS(cfg.del(tagd::abstract_tag("development")), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.del(tagd::abstract_tag(tagd::config::CONFIG_SUBJECT)), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.select(tagd::config::CONFIG_SUBJECT, "/tmp/tagd-home"), tagd::TAGD_OK);
	}

	void test_independent_profile_defaults_and_runtime_moves() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.select("unknown", "/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.select(tagd::HARD_TAG_CONFIG, "/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.subject_id(), tagd::config::CONFIG_SUBJECT);
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_BIND_PORT, "9090"), tagd::TAGD_OK);

		tagd::abstract_tag sibling("independent", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(cfg.put(sibling), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.select("independent", "/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);
		TS_ASSERT_EQUALS(cfg.home(), "/tmp/tagd-home");

		cfg.start_runtime();
		// A local default is required when this independent profile has no ancestor value.
		tagd::abstract_tag removal("independent");
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "2112"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.del(removal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);
		TS_ASSERT_DIFFERS(cfg.select("unknown", "/tmp/tagd-home"), tagd::TAGD_OK);
	}

	void test_defaults_and_validated_updates_share_storage() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);

		tagd::abstract_tag change("default_config");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"), tagd::TAGD_OK);
		tagd::tagspace* ts = &cfg;
		TS_ASSERT_EQUALS(ts->put(change), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(ts->put(change), tagd::TS_DUPLICATE);

		tagd::abstract_tag loaded;
		TS_ASSERT_EQUALS(ts->get(loaded, "default_config"), tagd::TAGD_OK);
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"));
	}

	void test_dump_preserves_values_whose_objects_are_hard_tags() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);

		std::ostringstream output;
		TS_ASSERT_EQUALS(cfg.dump(output), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(output.str().find("_config:bind_port = 2112"), std::string::npos);
		TS_ASSERT_DIFFERS(output.str().find("_config:prompt = "), std::string::npos);
	}

	void test_invalid_update_preserves_all_settings() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);

		tagd::abstract_tag change("default_config");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "65536"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_PROMPT, "changed> "), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.put(change), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);
		TS_ASSERT_EQUALS(cfg.prompt(), "tagd> ");

		TS_ASSERT_DIFFERS(cfg.del(tagd::abstract_tag("default_config")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);
	}

	void test_runtime_lock_and_volatile_instances() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		cfg.start_runtime();

		tagd::abstract_tag port("default_config");
		TS_ASSERT_EQUALS(port.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.put(port), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 2112);

		tagd::abstract_tag prompt("default_config");
		TS_ASSERT_EQUALS(prompt.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_PROMPT, "live> "), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(prompt), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.prompt(), "live> ");

		tagd::config fresh;
		TS_ASSERT_EQUALS(fresh.init("/tmp/tagd-home"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fresh.prompt(), "tagd> ");
	}

	void test_extension_values_and_required_value_deletion() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		tagd::abstract_tag extension("greeting", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(cfg.put(extension), tagd::TAGD_OK);

		tagd::abstract_tag change("default_config");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, "greeting", "hello"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(change), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(change), tagd::TAGD_OK);

		tagd::abstract_tag required("default_config");
		TS_ASSERT_EQUALS(required.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_CONFIG_PROMPT, "tagd> "), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(required), tagd::TAGD_OK);
		TS_ASSERT(cfg.prompt().empty());
	}

	void test_logger_updates_replace_levels_and_preserve_streams() {
		tagd::tagspace_install_logger_validator();
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-home"), tagd::TAGD_OK);
		std::ostringstream output;
		cfg.log().stream(output);

		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL,
			"error,_role:tagsh:debug"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.log().level(tagd::HARD_TAG_ROLE_TAGSH), tagd::log_level::DEBUG);
		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_LOG_LEVEL, "warning"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.log().level(tagd::HARD_TAG_ROLE_TAGSH), tagd::log_level::WARNING);
		TS_ASSERT_EQUALS(&cfg.log().stream(), &output);
	}

	void test_profile_use_inheritance_and_frozen_target() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init("/tmp/tagd-profiles"), tagd::TAGD_OK);
		for (const auto* id : {"animals", "plants"}) {
			TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag(id, HARD_TAG_SUB,
				tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		}

		tagd::abstract_tag base("base", HARD_TAG_SUB, tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(base.relation(tagd::HARD_TAG_USE, "animals", "a"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(base), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("child", HARD_TAG_SUB,
			"base", tagd::POS_TAG)), tagd::TAGD_OK);

		tagd::abstract_tag conflict("child");
		TS_ASSERT_EQUALS(conflict.relation(tagd::HARD_TAG_USE, "plants"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(conflict), tagd::TS_MISUSE);
		cfg.clear_errors();

		TS_ASSERT_EQUALS(cfg.select("child", "/tmp/tagd-profiles"), tagd::TAGD_OK);
		tagd::id_string target;
		TS_ASSERT_EQUALS(cfg.startup_target(target), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target, "animals");
		tagd::id_string resolved;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("a", resolved), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(resolved, "animals");

		TS_ASSERT_EQUALS(cfg.freeze_target(), tagd::TAGD_OK);
		tagd::abstract_tag remove("base");
		TS_ASSERT_EQUALS(remove.relation(tagd::HARD_TAG_USE, "animals"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(remove), tagd::TS_MISUSE);
		cfg.clear_errors();
		TS_ASSERT_EQUALS(cfg.put(base), tagd::TS_DUPLICATE);
	}

	void test_descendant_first_use_and_reparenting_are_validated() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("parent", HARD_TAG_SUB,
			tagd::HARD_TAG_CONFIG, tagd::POS_TAG)), tagd::TAGD_OK);
		tagd::abstract_tag child("child", HARD_TAG_SUB, "parent", tagd::POS_TAG);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_USE, tagd::HARD_TAG_DEFAULT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(child), tagd::TAGD_OK);

		tagd::abstract_tag conflict("parent");
		TS_ASSERT_EQUALS(conflict.relation(tagd::HARD_TAG_USE, tagd::HARD_TAG_DEFAULT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(conflict), tagd::TS_MISUSE);
		cfg.clear_errors();

		tagd::abstract_tag sibling("sibling", HARD_TAG_SUB, tagd::HARD_TAG_CONFIG, tagd::POS_TAG);
		TS_ASSERT_EQUALS(sibling.relation(tagd::HARD_TAG_USE, tagd::HARD_TAG_CONFIG), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(sibling), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("parent", HARD_TAG_SUB,
			"sibling", tagd::POS_TAG)), tagd::TS_MISUSE);
	}

	void test_reserved_target_alias_and_runtime_modifier_are_startup_only() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		tagd::abstract_tag use("default_config");
		TS_ASSERT_EQUALS(use.relation(tagd::HARD_TAG_USE, tagd::HARD_TAG_DEFAULT, "mem"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(use), tagd::TAGD_OK);

		tagd::id_string target;
		TS_ASSERT_EQUALS(cfg.startup_target(target), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("mem", target), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(target, tagd::HARD_TAG_DEFAULT);

		use.relations.clear();
		TS_ASSERT_EQUALS(use.relation(tagd::HARD_TAG_USE, tagd::HARD_TAG_DEFAULT, "later"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(use), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("later", target), tagd::TS_NOT_FOUND);
		TS_ASSERT_DIFFERS(cfg.put(tagd::abstract_tag("mem", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
	}
};
