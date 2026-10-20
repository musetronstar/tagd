#pragma once

#include "tagspace-config.h"
#include <cxxtest/TestSuite.h>
#include <sstream>

class ConfigTester : public CxxTest::TestSuite {
public:
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
		tagd::tagspace& space = cfg;
		for (const auto& ref : {
			tagd::referent("設定", "default_config", "日本語"),
			tagd::referent("持つ", tagd::HARD_TAG_HAS, "日本語"),
			tagd::referent("ポート", tagd::HARD_TAG_CONFIG_BIND_PORT, "日本語"),
			tagd::referent("表示", tagd::HARD_TAG_CONFIG_PROMPT, "日本語")}) {
			TS_ASSERT_EQUALS(space.put(ref), tagd::TAGD_OK);
		}
		auto ssn = cfg.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("日本語"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.pos("持つ", &ssn), tagd::POS_RELATOR);
		tagd::abstract_tag change("設定");
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "8080"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(space.put(change, &ssn), tagd::TS_DUPLICATE);
		TS_ASSERT_DIFFERS(space.put(change, &ssn, tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);
		tagd::abstract_tag result;
		TS_ASSERT_EQUALS(space.get(result, "設定", &ssn), tagd::TAGD_OK);
		TS_ASSERT(result.related("持つ", "ポート", "8080"));
		TS_ASSERT_EQUALS(space.get(result, "default_config", &ssn,
			tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);
		TS_ASSERT(result.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_CONFIG_BIND_PORT, "8080"));
		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "70000"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(change, &ssn), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ssn.code(), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		cfg.start_runtime();
		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "ポート", "9090"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(change, &ssn), tagd::TS_MISUSE);
		TS_ASSERT_DIFFERS(space.del(tagd::abstract_tag("設定"), &ssn), tagd::TAGD_OK);
		change.relations.clear();
		TS_ASSERT_EQUALS(change.relation("持つ", "表示", "こんにちは> "), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space.put(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.prompt(), "こんにちは> ");
		TS_ASSERT_EQUALS(space.del(change, &ssn), tagd::TAGD_OK);
		TS_ASSERT(cfg.prompt().empty());
		TS_ASSERT_EQUALS(space.del(tagd::referent("表示", tagd::HARD_TAG_CONFIG_PROMPT,
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
		tagd::tagspace* space = &cfg;
		TS_ASSERT_EQUALS(space->put(change), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_port(), 8080);
		TS_ASSERT_EQUALS(space->put(change), tagd::TS_DUPLICATE);
		tagd::abstract_tag loaded;
		TS_ASSERT_EQUALS(space->get(loaded, "default_config"), tagd::TAGD_OK);
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

};
