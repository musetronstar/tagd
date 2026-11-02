#pragma once

#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include "tagspace-config.h"
#include "tagdb.h"
#include "filepile-files.h"
#include "filepile-records.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sqlite3.h>

namespace command_reads {
inline unsigned rows = 0;
inline unsigned ancestry_queries = 0;
}

extern "C" int __real_sqlite3_step(sqlite3_stmt*);
extern "C" int __wrap_sqlite3_step(sqlite3_stmt* statement) {
	if (!sqlite3_stmt_busy(statement) &&
		std::string_view(sqlite3_sql(statement)).starts_with("WITH RECURSIVE sources"))
		++command_reads::ancestry_queries;
	const auto rc = __real_sqlite3_step(statement);
	if (rc == SQLITE_ROW)
		++command_reads::rows;
	return rc;
}

class FilepileCommandTester : public CxxTest::TestSuite {
	std::filesystem::path home;
	std::unique_ptr<tagd::tagspace::persistent> space;
	std::string source;

	void write(const std::string& bytes) {
		std::ofstream(home / "source") << bytes;
	}

	std::string dump() {
		std::ostringstream output;
		TS_ASSERT_EQUALS(space->dump(output), tagd::TAGD_OK);
		return output.str();
	}

public:
	void test_classification_visits_ancestors_once_and_type_query_reuses_its_resolver() {
		auto db = tagdb::make_backend();
		TS_ASSERT_EQUALS(db->open({tagdb::storage_kind::memory}), tagd::TAGD_OK);
		tagd::abstract_tag parent("parent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		for (const auto target : {tagd::HARD_TAG_FILE_STORED, tagd::HARD_TAG_LINK_FILEPILE,
			tagd::HARD_TAG_FILEPILE_EVENT})
			TS_ASSERT_EQUALS(parent.type_relation(tagd::HARD_TAG_TYPE_OF, target), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db->put(parent, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(db->put(tagd::abstract_tag("child", tagd::HARD_TAG_SUB, "parent",
			tagd::POS_TAG), nullptr), tagd::TAGD_OK);
		tagd::filepile::record_roles roles;
		command_reads::ancestry_queries = 0;
		TS_ASSERT_EQUALS(tagd::filepile::classify(roles, *db, "child"), tagd::TAGD_OK);
		TS_ASSERT(roles.stored && roles.link && roles.event);
		TS_ASSERT_EQUALS(command_reads::ancestry_queries, 1u);
		TS_ASSERT_EQUALS(tagd::filepile::classify(roles, *db, "missing"), tagd::TAGD_OK);
		TS_ASSERT(!roles.stored && !roles.link && !roles.event);

		tagd::interrogator query(tagd::HARD_TAG_WHAT);
		TS_ASSERT_EQUALS(query.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_FILE_STORED),
			tagd::TAGD_OK);
		tagd::tag_set matches;
		command_reads::ancestry_queries = 0;
		TS_ASSERT_EQUALS(db->query(matches, query, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(matches.size(), 3u);
		TS_ASSERT_EQUALS(command_reads::ancestry_queries, 0u);
	}

	void setUp() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagd-command-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		TS_ASSERT(directory);
		home = directory;
		write("first content\n");
		source = tagd::file_url((home / "source").string());
		space = std::make_unique<tagd::tagspace::persistent>();
		TS_ASSERT_EQUALS(space->create("test", home.string()), tagd::TAGD_OK);
	}

	void tearDown() {
		TAGSPACE_SET_LOGGER(nullptr);
		space.reset();
		std::filesystem::remove_all(home);
	}

	void test_repeated_store_reads_only_the_source_history() {
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(source)), tagd::TAGD_OK);
		command_reads::rows = 0;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(source)), tagd::TS_DUPLICATE);
		const auto before = command_reads::rows;
		for (unsigned i = 0; i != 20; ++i) {
			const auto name = home / ("other" + std::to_string(i));
			std::ofstream(name) << "unrelated content";
			TS_ASSERT_EQUALS(space->store(result, tagd::url(tagd::file_url(name.string()))),
				tagd::TAGD_OK);
		}
		command_reads::rows = 0;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(source)), tagd::TS_DUPLICATE);
		// A fixed allowance covers cache state; unrelated links must not add per-link reads.
		TS_ASSERT_LESS_THAN(command_reads::rows, before + 20);
	}

	void test_plain_file_urls_do_not_probe_resource_paths() {
		const auto blocked = home / "blocked";
		std::filesystem::create_directory(blocked);
		TS_ASSERT_EQUALS(::chmod(blocked.c_str(), 0), 0);
		const auto rc = space->put(tagd::url(tagd::file_url((blocked / "file").string())));
		TS_ASSERT_EQUALS(::chmod(blocked.c_str(), 0700), 0);
		TS_ASSERT_EQUALS(rc, tagd::TAGD_OK);

		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		const auto path = home / "filepile" / stored.stored.path().substr(1);
		tagd::store_result rejected;
		TS_ASSERT_EQUALS(space->store(rejected, tagd::url(tagd::file_url(path.string()))), tagd::TS_MISUSE);
		std::filesystem::create_directory_symlink(home / "filepile", home / "alias");
		const auto relative = std::filesystem::path(path).lexically_relative(home / "filepile");
		tagd::url alias(tagd::file_url((home / "alias" / relative).string()));
		TS_ASSERT_EQUALS(space->put(alias), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->store(stored, alias), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->put(tagd::url(tagd::file_url(
			(home / "nonexistent/../filepile/new").string()))), tagd::TAGD_OK);
		std::filesystem::create_directory_symlink(home, home / "home-alias");
		space->filepile_home((home / "home-alias").string());
		for (const auto& root : {home, home / "home-alias"}) {
			TS_ASSERT_EQUALS(space->put(tagd::url(tagd::file_url(
				(root / "filepile/new").string()))), tagd::TAGD_OK);
		}
	}

	void test_unavailable_home_does_not_block_unrelated_url_identifiers() {
		const auto blocked = home / "blocked";
		std::filesystem::create_directory(blocked);
		TS_ASSERT_EQUALS(::chmod(blocked.c_str(), 0), 0);
		space->filepile_home((blocked / "nested").string());
		const auto rc = space->put(tagd::url("https://example.com/unrelated"));
		TS_ASSERT_EQUALS(::chmod(blocked.c_str(), 0700), 0);
		TS_ASSERT_EQUALS(rc, tagd::TAGD_OK);
	}

	void test_store_resolution_failures_preserve_the_supplied_source_url() {
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		std::filesystem::create_directory_symlink(home / "loop", home / "loop");
		std::filesystem::create_directory_symlink(home / "filepile", home / "alias");

		const std::string prefix = "file://" + home.string();
		for (const auto& input : {prefix + "/%6coop", prefix + "/%61lias" + stored.stored.path()}) {
			tagd::store_result rejected;
			const auto rc = space->store(rejected, tagd::url(input));
			TS_ASSERT_EQUALS(rc, input.ends_with("%6coop") ? tagd::TS_NOT_AVAILABLE : tagd::TS_MISUSE);
			std::ostringstream text;
			space->print_errors(text);
			TS_ASSERT_DIFFERS(text.str().find(input), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find(prefix + "/loop"), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find(prefix + "/alias"), std::string::npos);
		}
	}

	void test_unresolved_home_does_not_change_file_url_identity() {
		const auto loop = home / "loop";
		std::filesystem::create_directory_symlink(loop, loop);
		space->filepile_home(loop.string());
		TS_ASSERT_EQUALS(space->put(tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::url(tagd::file_url(
			(loop / "filepile/new").string()))), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(source)), tagd::TS_NOT_AVAILABLE);
	}

	void test_no_pos_cast_does_not_bypass_managed_record_guards() {
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		const auto before = dump();
		for (const auto& id : {stored.stored.hduri(), stored.link}) {
			tagd::abstract_tag forged(id, tagd::HARD_TAG_SUB, tagd::id_view{}, tagd::POS_REFERENT);
			TS_ASSERT_EQUALS(space->del(forged, nullptr, tagd::F_NO_POS_CAST),
				id == stored.link ? tagd::TS_DEPENDENCY : tagd::TS_MISUSE);
			if (id == stored.link) {
				TS_ASSERT_EQUALS(forged.relation(tagd::HARD_TAG_FROM, stored.source.hduri()), tagd::TAGD_OK);
			} else {
				TS_ASSERT_EQUALS(forged.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_HASH_SHA256, "forged"), tagd::TAGD_OK);
			}
			TS_ASSERT_EQUALS(space->put(forged, nullptr, tagd::F_NO_POS_CAST), tagd::TS_MISUSE);
		}
		TS_ASSERT_EQUALS(dump(), before);
	}

	void test_combined_duplicates_and_predicates_with_both_settings() {
		for (const tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_IGNORE_DUPLICATES)}) {
			const auto suffix = std::to_string(flags);
			const auto path = home / ("source" + suffix);
			std::ofstream(path) << "original " << suffix;
			tagd::url declaration(tagd::file_url(path.string()));
			TS_ASSERT_EQUALS(declaration.relation(tagd::HARD_TAG_HAS,
				tagd::HARD_TAG_MESSAGE, "first"), tagd::TAGD_OK);
			tagd::store_result first, result;
			TS_ASSERT_EQUALS(space->store(first, declaration, nullptr, flags), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(first.stored.super_object(), tagd::HARD_TAG_FILE);
			TS_ASSERT_EQUALS(first.stored.sub_relator(), tagd::HARD_TAG_SUB);
			TS_ASSERT(first.stored.type_relations.contains({tagd::HARD_TAG_TYPE_OF,
				tagd::HARD_TAG_FILE_STORED}));

			declaration.relations.clear();
			TS_ASSERT_EQUALS(declaration.relation(tagd::HARD_TAG_HAS,
				tagd::HARD_TAG_MESSAGE, "changed"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(space->store(result, declaration, nullptr, flags), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(result.link, first.link);
			tagd::url fetched;
			TS_ASSERT_EQUALS(space->get(fetched, declaration.id()), tagd::TAGD_OK);
			TS_ASSERT(fetched.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "changed"));

			std::ofstream(path) << "new bytes " << suffix;
			TS_ASSERT_EQUALS(space->store(result, declaration, nullptr, flags), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(result.revision, 2u);
			const auto before = dump();
			const auto duplicate = flags ? tagd::TAGD_OK : tagd::TS_DUPLICATE;
			TS_ASSERT_EQUALS(space->store(result, declaration, nullptr, flags), duplicate);
			TS_ASSERT_EQUALS(dump(), before);
			TS_ASSERT(std::filesystem::is_empty(home / "filepile/.staging"));
		}
	}

	void test_predeclared_source_and_validation_before_acquisition() {
		tagd::url declaration(source);
		TS_ASSERT_EQUALS(space->put(declaration), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, declaration), tagd::TAGD_OK);

		const auto before = dump();
		TS_ASSERT_EQUALS(declaration.relation(tagd::HARD_TAG_HAS, "undefined_object"), tagd::TAGD_OK);
		write("changed source bytes");
		TS_ASSERT_DIFFERS(space->store(result, declaration), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT(std::filesystem::is_empty(home / "filepile/.staging"));
		TS_ASSERT_EQUALS(space->store(result, tagd::abstract_tag("not_a_url")), tagd::TS_MISUSE);
		tagd::url missing(source + ".missing");
		TS_ASSERT_EQUALS(missing.relation(tagd::HARD_TAG_HAS, "undefined_object"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->store(result, missing), tagd::TS_OBJECT_UNK);
	}

	void test_indirect_stored_classification_is_protected() {
		// A private backend fixture represents legacy data, without adding a public bypass.
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(tagd::abstract_tag("custom_stored", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_FILE_STORED, tagd::POS_TAG), nullptr), tagd::TAGD_OK);
		const auto before = dump();
		tagd::url declaration(source);
		TS_ASSERT_EQUALS(declaration.type_relation(tagd::HARD_TAG_TYPE_OF, "custom_stored"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(declaration), tagd::TS_MISUSE);
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, declaration), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));
	}

	void test_managed_classification_and_url_guards_cannot_be_bypassed() {
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		const auto before = dump();
		for (const auto& object : {std::string(tagd::HARD_TAG_FILE_STORED), stored.stored.hduri()}) {
			tagd::abstract_tag type("forged", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
			TS_ASSERT_EQUALS(type.type_relation(tagd::HARD_TAG_TYPE_OF, object), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(space->put(type), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("forged", tagd::HARD_TAG_SUB, object, tagd::POS_TAG)), tagd::TS_MISUSE);
		}
		TS_ASSERT_EQUALS(space->put(tagd::url(stored.stored.id())), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->put(tagd::abstract_tag(stored.stored.hduri())), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(stored.stored.id())), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT_EQUALS(space->put(tagd::url(tagd::file_url(
			(home / "filepile/not-created.bin").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::url("file:///filepile/x")), tagd::TAGD_OK);
	}

	void test_filepile_scheme_guards_every_tagspace_without_a_stored_record() {
		tagd::tagspace::persistent direct;
		TS_ASSERT_EQUALS(direct.create_file((home / "direct.db").string()), tagd::TAGD_OK);
		tagd::tagspace::memory memory;
		TS_ASSERT_EQUALS(memory.init(), tagd::TAGD_OK);
		tagd::config config;
		TS_ASSERT_EQUALS(config.init(""), tagd::TAGD_OK);
		const tagd::url missing("filepile:///aa/aa/" + std::string(64, 'a') + ".bin");
		for (tagd::tagspace* ts : {static_cast<tagd::tagspace*>(space.get()),
			static_cast<tagd::tagspace*>(&direct), static_cast<tagd::tagspace*>(&memory),
			static_cast<tagd::tagspace*>(&config)}) {
			for (const auto& id : {missing.id(), missing.hduri(), std::string("filepile:///bad"),
				std::string("FILEPILE:///x"), std::string("filepile:x"),
				"filepile:///aa/aa/" + std::string(64, 'a') + ".PNG",
				"filepile://host/aa/aa/" + std::string(64, 'a') + ".bin",
				std::string("hd:!!!/bad!!!!!!filepile"), std::string("HD:!!!/bad!!!!!!FILEPILE")}) {
				tagd::replace_outcome outcome;
				tagd::abstract_tag subject(id);
				TS_ASSERT_EQUALS(ts->put(subject), tagd::TS_MISUSE);
				TS_ASSERT(!ts->exists(id));
				TS_ASSERT_EQUALS(ts->replace(outcome, subject), tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(ts->del(subject), tagd::TS_MISUSE);
				TS_ASSERT(!ts->exists(id));
				tagd::referent alias(id, tagd::HARD_TAG_ENTITY, tagd::HARD_TAG_ENTITY);
				TS_ASSERT_EQUALS(ts->put(alias), tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(ts->del(alias), tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(ts->put(static_cast<const tagd::abstract_tag&>(alias)), tagd::TS_MISUSE);
				TS_ASSERT_EQUALS(ts->del(static_cast<const tagd::abstract_tag&>(alias)), tagd::TS_MISUSE);
				tagd::abstract_tag child("forged", tagd::HARD_TAG_SUB, id, tagd::POS_TAG);
				TS_ASSERT_EQUALS(ts->put(child), tagd::TS_MISUSE);
				tagd::abstract_tag type("forged", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
				TS_ASSERT_EQUALS(type.type_relation(tagd::HARD_TAG_TYPE_OF, id), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(ts->put(type), tagd::TS_MISUSE);
				TS_ASSERT(!ts->exists("forged"));
			}
		}
	}

	void test_unstored_filepile_referent_cannot_bypass_the_scheme_guard() {
		const tagd::url missing("filepile:///aa/aa/" + std::string(64, 'a') + ".bin");
		// A private damaged-data fixture has a URL and referent, but no STORE or B facts.
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(missing, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("context", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::referent("copy", missing.hduri(), "context")), tagd::TAGD_OK);
		auto session = space->get_session();
		TS_ASSERT_EQUALS(session.push_context("context"), tagd::TAGD_OK);
		const auto before = dump();
		tagd::replace_outcome outcome;
		for (const bool with_home : {true, false}) {
			if (!with_home)
				space->filepile_home("");
			TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("copy"), &session), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(space->replace(outcome, tagd::abstract_tag("copy"), &session), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(space->del(tagd::abstract_tag("copy"), &session), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(dump(), before);
		}
	}

	void test_referents_cannot_write_or_classify_through_stored_content() {
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("context", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::referent("copy", stored.stored.hduri(), "context")), tagd::TAGD_OK);
		auto session = space->get_session();
		TS_ASSERT_EQUALS(session.push_context("context"), tagd::TAGD_OK);
		const auto before = dump();
		tagd::abstract_tag subject("copy");
		TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "note"), tagd::TAGD_OK);
		tagd::replace_outcome outcome;
		TS_ASSERT_EQUALS(space->put(subject, &session), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->replace(outcome, subject, &session), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->del(subject, &session), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("forged", tagd::HARD_TAG_SUB,
			"copy", tagd::POS_TAG), &session), tagd::TS_MISUSE);
		tagd::abstract_tag type("forged", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(type.type_relation(tagd::HARD_TAG_TYPE_OF, "copy"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(type, &session), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(dump(), before);
	}

	void test_generated_fields_are_protected_and_descriptions_are_editable() {
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(tagd::abstract_tag("forged_link", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_LINK_FILEPILE, tagd::POS_TAG)), tagd::TS_MISUSE);
		for (const auto object : {tagd::HARD_TAG_REVISION, tagd::HARD_TAG_MTIME, tagd::HARD_TAG_STORED_TIME}) {
			tagd::abstract_tag change(stored.link);
			TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, object, "bad"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "mixed"), tagd::TAGD_OK);
			const auto before = dump();
			TS_ASSERT_EQUALS(space->put(change), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(space->del(change), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(dump(), before);
		}
		tagd::abstract_tag description(stored.link);
		TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "description"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(description), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->del(description), tagd::TAGD_OK);
		tagd::url component(source);
		TS_ASSERT_EQUALS(component.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_PATH, "/forged"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(component), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->del(component), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->del(tagd::abstract_tag(stored.link)), tagd::TS_DEPENDENCY);
		TS_ASSERT_EQUALS(space->del(tagd::url(stored.stored.id())), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(space->del(tagd::url(source)), tagd::TS_DEPENDENCY);
	}

	void test_events_include_literal_details_and_respect_logging_filters() {
		std::ostringstream output;
		tagd::logger log(output);
		TAGSPACE_SET_LOGGER(&log);
		auto session = space->get_session();
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source), &session), tagd::TAGD_OK);
		const auto text = output.str();
		TS_ASSERT_DIFFERS(text.find("_event:filepile_store"), std::string::npos);
		TS_ASSERT_DIFFERS(text.find("_has _message"), std::string::npos);
		TS_ASSERT_DIFFERS(text.find(stored.link), std::string::npos);
		TS_ASSERT_DIFFERS(text.find("TAGD_OK"), std::string::npos);
		TS_ASSERT_EQUALS(dump().find("ev:"), std::string::npos);
		output.str("");
		log.level(tagd::HARD_TAG_ROLE_TAGSPACE, tagd::log_level::EMERGENCY);
		write("new bytes");
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source), &session), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(output.str().find("_event:filepile_store"), std::string::npos);
	}
	void test_store_rejects_ambiguous_legacy_associations() {
		tagd::store_result first;
		TS_ASSERT_EQUALS(space->store(first, tagd::url(source)), tagd::TAGD_OK);
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		tagd::url other("file:///other-source");
		TS_ASSERT_EQUALS(fixture->put(other, nullptr), tagd::TAGD_OK);
		tagd::abstract_tag damage(first.link);
		TS_ASSERT_EQUALS(damage.relation(tagd::HARD_TAG_FROM, other.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(damage, nullptr), tagd::TAGD_OK);
		const auto before = dump();
		write("different content");
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(source)), tagd::TS_ERR);
		TS_ASSERT_EQUALS(dump(), before);
	}

};
