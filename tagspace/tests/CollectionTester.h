#pragma once
#include "tagspace-config.h"
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>

class CollectionTester : public CxxTest::TestSuite {
	std::filesystem::path root;
public:
	void test_register_only_does_not_open_database() {
		char pattern[] = "/tmp/tagd-register-only-XXXXXX";
		std::filesystem::path root(mkdtemp(pattern));
		struct cleanup {
			std::filesystem::path path;
			~cleanup() { std::filesystem::remove_all(path); }
		} remove{root};
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		const auto file = tagd::tagspace_database_path(root.string(), "animals");
		std::filesystem::create_directories(std::filesystem::path(file).parent_path());
		std::ofstream(file) << "not a database";

		std::shared_ptr<tagd::tagspace> result;
		TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "", result, nullptr, {},
			tagd::tagspace_access::local, {}, nullptr, tagd::source_action::register_only), tagd::TAGD_OK);
		TS_ASSERT(!result);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("animals", id), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(cfg.open_tagspace("animals", "", result), tagd::TAGD_OK);
	}

	void setUp() {
		char pattern[] = "/tmp/tagspace-collection-XXXXXX";
		root = mkdtemp(pattern);
	}

	void tearDown() {
		std::filesystem::remove_all(root);
	}

	void test_descriptions_sources_and_borrowed_lifetime() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);

		std::shared_ptr<tagd::tagspace> first, second;
		TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "a", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_tagspace("a", "", second), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.get(), second.get());
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TS_MISUSE);

		first.reset();
		second.reset();
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TAGD_OK);
		TS_ASSERT(cfg.exists("animals"));

		TS_ASSERT_EQUALS(cfg.open_tagspace("a", "", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(tagd::abstract_tag("animals")), tagd::TS_MISUSE);
	}

	void test_default_layout_registration_keeps_sources_derived_from_home() {
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		// An absolute path in this configuration would have been synthesized by registration.
		struct restore_directory {
			std::filesystem::path path = std::filesystem::current_path();
			~restore_directory() {
				std::filesystem::current_path(path);
			}
		} restore;
		std::filesystem::current_path(root.parent_path());
		const auto configured_home = root.filename().string();
		for (const auto access : {tagd::tagspace_access::local, tagd::tagspace_access::named_only}) {
			tagd::config cfg;
			TS_ASSERT_EQUALS(cfg.init(configured_home), tagd::TAGD_OK);
			std::shared_ptr<tagd::tagspace> opened;
			TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "", opened, nullptr, {}, access), tagd::TAGD_OK);
			tagd::abstract_tag description;
			TS_ASSERT_EQUALS(cfg.get(description, "animals"), tagd::TAGD_OK);
			for (const auto& p : description.relations) {
				TS_ASSERT_DIFFERS(p.relator, tagd::HARD_TAG_FROM);
			}
			std::ostringstream text;
			TS_ASSERT_EQUALS(cfg.dump(text), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
			TS_ASSERT(!cfg.exists(tagd::url(tagd::file_url(
				tagd::tagspace_database_path(root.string(), "animals"))).hduri()));
			TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_HOME, (root / "different").string()), tagd::TS_MISUSE);
			TS_ASSERT_EQUALS(cfg.home(), configured_home);
		}
	}

	void test_explicit_location_keeps_its_declared_source() {
		const auto path = root / "explicit.db";
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create_file(path.string()), tagd::TAGD_OK);
		}
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace(path.string(), "", opened), tagd::TAGD_OK);
		tagd::abstract_tag description;
		TS_ASSERT_EQUALS(cfg.get(description, "explicit"), tagd::TAGD_OK);
		const tagd::url source(tagd::file_url(path.string()));
		TS_ASSERT(description.related(tagd::HARD_TAG_FROM, source.hduri()));
		TS_ASSERT(cfg.exists(source.hduri()));
	}

	void test_missing_and_recursive_sources_report_only_tagspace_ids() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		for (const auto& target : {std::string("absent"), (root / "absent.db").string()}) {
			TS_ASSERT_EQUALS(cfg.open_tagspace(target, "", opened), tagd::TS_NOT_FOUND);
			std::ostringstream text;
			cfg.print_errors(text);
			TS_ASSERT_DIFFERS(text.str().find("no such tagspace source: absent"), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
		}
		const auto path = root / "recursive.tagl";
		std::ofstream(path) << "%% _tagspace recursive.tagl;";
		const auto loader = [&](const std::string& source, std::shared_ptr<tagd::tagspace>) {
			std::shared_ptr<tagd::tagspace> nested;
			return cfg.open_tagspace(source, "", nested);
		};
		TS_ASSERT_EQUALS(cfg.open_tagspace(path.string(), "", opened, nullptr, {},
			tagd::tagspace_access::local, loader), tagd::TS_MISUSE);
		std::ostringstream text;
		cfg.print_errors(text);
		TS_ASSERT_DIFFERS(text.str().find("recursive source load: recursive"), std::string::npos);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
	}

	void test_database_open_failures_never_name_the_storage_path() {
		const auto path = root / "private.db";
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create_file(path.string()), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(::chmod(path.c_str(), 0), 0);
		tagd::tagspace::persistent unreadable;
		const auto rc = unreadable.open_file(path.string());
		TS_ASSERT_EQUALS(::chmod(path.c_str(), 0600), 0);
		TS_ASSERT_DIFFERS(rc, tagd::TAGD_OK);
		std::ostringstream text;
		unreadable.print_errors(text);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
		tagd::tagspace::persistent missing;
		TS_ASSERT_EQUALS(missing.open_file((root / "absent.db").string()), tagd::TS_NOT_FOUND);
		text.str("");
		missing.print_errors(text);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
	}

	void test_session_borrow_prevents_close() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "", opened), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(opened->put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);

		{
			auto ssn = opened->get_session();
			auto copy = ssn;
			opened.reset();
			auto rc = cfg.close_tagspace("animals");
			TS_ASSERT_EQUALS(rc, tagd::TS_MISUSE);
			if (rc == tagd::TS_MISUSE)
				TS_ASSERT_EQUALS(copy.push_context("japanese"), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TAGD_OK);
	}

	void test_source_first_reuse_and_collisions() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		tagd::tagspace::persistent db;
		TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		auto path = root / "tagspaces/animals/animals.db";
		tagd::url source("file://" + path.string());
		TS_ASSERT_EQUALS(cfg.put(source), tagd::TAGD_OK);
		tagd::abstract_tag description("zoology", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(description), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);

		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace(path.string(), "z", opened), tagd::TAGD_OK);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("z", id), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "zoology");

		tagd::abstract_tag duplicate("other", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(duplicate.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(duplicate), tagd::TS_MISUSE);
		TS_ASSERT(!cfg.exists("other"));
	}

	void test_reserved_targets_and_supporting_vocabulary() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);

		std::shared_ptr<tagd::tagspace> first, second;
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "", first), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_tagspace("_config", "", second), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(second.get(), &cfg);
		TS_ASSERT_EQUALS(cfg.close_tagspace("_default"), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "_bad", second), tagd::TS_MISUSE);
	}

	void test_listing_does_not_register_or_open() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_alias("animals", "a"), tagd::TAGD_OK);
		std::filesystem::create_directories(root / "tagspaces/disk_only");
		std::shared_ptr<tagd::tagspace> active;
		TS_ASSERT_EQUALS(cfg.open_tagspace("_default", "", active), tagd::TAGD_OK);

		std::ostringstream before, after, listing;
		TS_ASSERT_EQUALS(cfg.dump(before), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.list_tagspaces(listing, active.get()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(before.str(), after.str());

		TS_ASSERT_DIFFERS(listing.str().find("* _default"), std::string::npos);
		TS_ASSERT_DIFFERS(listing.str().find("_config"), std::string::npos);
		TS_ASSERT_DIFFERS(listing.str().find("animals\tregistered/closed\ta"), std::string::npos);
		TS_ASSERT_DIFFERS(listing.str().find("disk_only\tunregistered/unverified"), std::string::npos);
	}

	void test_stem_collision_does_not_reuse_an_open_different_source() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> first, second;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", first), tagd::TAGD_OK);
		std::filesystem::create_directories(root / "backup");
		std::filesystem::copy_file(root / "tagspaces/animals/animals.db", root / "backup/animals.db");

		TS_ASSERT_EQUALS(cfg.open_tagspace((root / "backup/animals.db").string(), "b", second), tagd::TS_MISUSE);
		TS_ASSERT(!second);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("b", id), tagd::TS_NOT_FOUND);
	}

	void test_close_retains_symlink_spelling_without_filesystem_access() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", opened), tagd::TAGD_OK);
		auto source = root / "tagspaces/animals/animals.db";
		auto link = root / "link.db";
		std::filesystem::create_symlink(source, link);
		TS_ASSERT_EQUALS(cfg.open_tagspace(link.string(), "a", opened), tagd::TAGD_OK);

		opened.reset();
		std::filesystem::remove(link);
		std::filesystem::remove(source);
		TS_ASSERT_EQUALS(cfg.close_tagspace(link.string()), tagd::TAGD_OK);
		std::ostringstream listing;
		TS_ASSERT_EQUALS(cfg.list_tagspaces(listing, nullptr), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(listing.str().find("animals\tregistered/closed"), std::string::npos);

		TS_ASSERT_EQUALS(cfg.close_tagspace(link.string(), "a"), tagd::TAGD_OK);
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("a", id), tagd::TS_NOT_FOUND);
	}

	void test_home_change_cannot_relocate_open_descriptions() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "", opened), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(cfg.set(tagd::HARD_TAG_CONFIG_HOME, (root / "different").string()), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.home(), root.string());
	}

	void test_reserved_alias_collision_in_both_insertion_orders() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_alias("_default", "a"), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(cfg.put(tagd::referent("a", "japanese", "japanese")), tagd::TS_MISUSE);
		TS_ASSERT_DIFFERS(cfg.put(tagd::abstract_tag("a", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
	}

	void test_canonical_names_reject_foreign_referents() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(cfg.put(tagd::referent("animals", "japanese", "japanese")), tagd::TS_MISUSE);
	}

	void test_source_edits_after_close_invalidate_retained_identity() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", opened), tagd::TAGD_OK);
		auto source = root / "tagspaces/animals/animals.db";
		auto replacement = root / "replacement.db";
		std::filesystem::copy_file(source, replacement);
		tagd::url old_url("file://" + source.string()), new_url("file://" + replacement.string());
		TS_ASSERT_EQUALS(cfg.put(new_url), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(old_url), tagd::TAGD_OK);
		tagd::abstract_tag declared("animals");
		TS_ASSERT_EQUALS(declared.relation(tagd::HARD_TAG_FROM, old_url.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(declared), tagd::TAGD_OK);

		tagd::abstract_tag remove("animals");
		TS_ASSERT_EQUALS(remove.relation(tagd::HARD_TAG_FROM, old_url.hduri()), tagd::TAGD_OK);
		opened.reset();
		TS_ASSERT_EQUALS(cfg.close_tagspace("animals"), tagd::TAGD_OK);
		// The removed explicit source equals the home-derived source, so source identity is unchanged.
		TS_ASSERT_EQUALS(cfg.del(remove), tagd::TAGD_OK);

		tagd::abstract_tag change("animals");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_FROM, new_url.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(change), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_tagspace("animals", "a", opened), tagd::TAGD_OK);

		opened.reset();
		TS_ASSERT_EQUALS(cfg.close_tagspace(source.string()), tagd::TAGD_OK);
		std::ostringstream listing;
		TS_ASSERT_EQUALS(cfg.list_tagspaces(listing, nullptr), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(listing.str().find("animals\tregistered/open"), std::string::npos);

		TS_ASSERT_EQUALS(cfg.close_tagspace(replacement.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.close_tagspace(replacement.string(), "a"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.del(tagd::abstract_tag("animals")), tagd::TAGD_OK);
		TS_ASSERT(std::filesystem::exists(source));
		TS_ASSERT(std::filesystem::exists(replacement));
	}

	void test_descendants_have_local_sources_and_annotations() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		tagd::abstract_tag child("birds", HARD_TAG_SUB, "animals", tagd::POS_TAG);
		TS_ASSERT_EQUALS(child.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, "birds"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(child), tagd::TAGD_OK);

		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace("birds", "", opened), tagd::TS_NOT_FOUND);
		TS_ASSERT(cfg.exists("birds"));
	}

	void test_path_encoding_and_missing_source_collision() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", opened), tagd::TAGD_OK);
		std::filesystem::create_directories(root / "a%20b");
		std::filesystem::copy_file(root / "tagspaces/animals/animals.db", root / "a%20b/日本語.db");

		std::shared_ptr<tagd::tagspace> literal, encoded;
		TS_ASSERT_EQUALS(cfg.open_tagspace((root / "a%20b/日本語.db").string(), "", literal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.open_tagspace("file://" + root.string() + "/a%2520b/日本語.db", "", encoded), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(literal.get(), encoded.get());

		TS_ASSERT_EQUALS(cfg.open_tagspace("file:///tmp/bad%ZZ.db", "", encoded), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.open_tagspace("file:///tmp/bad%00.db", "", encoded), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(cfg.open_tagspace("/tmp/unsupported.DB", "", encoded), tagd::TS_MISUSE);
	}

	void test_open_symlink_source_survives_annotation_after_disappearance() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		auto link = root / "link.db";
		std::filesystem::create_symlink(root / "tagspaces/animals/animals.db", link);
		tagd::url source("file://" + link.string());
		TS_ASSERT_EQUALS(cfg.put(source), tagd::TAGD_OK);
		tagd::abstract_tag description("zoo", HARD_TAG_SUB, tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(description), tagd::TAGD_OK);

		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace("zoo", "", opened), tagd::TAGD_OK);
		std::filesystem::remove(link);
		std::shared_ptr<tagd::tagspace> same;
		TS_ASSERT_EQUALS(cfg.open_tagspace("zoo", "", same), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(same.get(), opened.get());

		same.reset();
		tagd::abstract_tag annotation("zoo");
		TS_ASSERT_EQUALS(annotation.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, "still open"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(annotation), tagd::TAGD_OK);

		opened.reset();
		TS_ASSERT_EQUALS(cfg.close_tagspace(link.string()), tagd::TAGD_OK);
	}

	void test_missing_sources_that_later_alias_fail_before_opening() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		for (const auto& id : {"a", "b"}) {
			tagd::url source("file://" + (root / (std::string(id) + ".db")).string());
			TS_ASSERT_EQUALS(cfg.put(source), tagd::TAGD_OK);
			tagd::abstract_tag description(id, HARD_TAG_SUB, tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG);
			TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_FROM, source.hduri()), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(cfg.put(description), tagd::TAGD_OK);
		}

		{
			tagd::tagspace::persistent db;
			TS_ASSERT_EQUALS(db.create("animals", root.string()), tagd::TAGD_OK);
		}
		std::filesystem::copy_file(root / "tagspaces/animals/animals.db", root / "a.db");
		std::filesystem::create_symlink(root / "a.db", root / "b.db");

		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.open_tagspace("a", "", opened), tagd::TS_MISUSE);
		TS_ASSERT(!opened);
	}

	void test_context_aliases_fallback_and_qualified_unset() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("animals", HARD_TAG_SUB,
			tagd::HARD_TAG_TAGSPACE, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::abstract_tag("japanese", HARD_TAG_SUB,
			HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.put(tagd::referent("a", "animals", "japanese")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.bind_alias("animals", "a"), tagd::TS_AMBIGUOUS);
		TS_ASSERT_EQUALS(cfg.bind_alias("animals", "b"), tagd::TAGD_OK);

		auto ssn = cfg.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("japanese"), tagd::TAGD_OK);
		auto before = ssn.context();
		tagd::id_string id;
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("b", id), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "animals");
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("b", id, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "animals");
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("a", id, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "animals");
		TS_ASSERT_EQUALS(ssn.context(), before);

		TS_ASSERT_EQUALS(cfg.close_tagspace("animals", "b", &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("a", id, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(id, "animals");
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("a", id), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(cfg.resolve_tagspace("b", id, &ssn), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(ssn.context(), before);
	}

	void test_relative_parent_and_os_home_locations_reuse_source() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened, other;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", opened), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(cfg.open_tagspace("../animals/./animals.db", "", other, nullptr,
			(root / "tagspaces/animals").string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(opened.get(), other.get());

		const char* previous = std::getenv("HOME");
		std::optional<std::string> saved = previous ? std::optional<std::string>(previous) : std::nullopt;
		setenv("HOME", root.c_str(), 1);
		auto rc = cfg.open_tagspace("~/tagspaces/animals/animals.db", "", other);
		if (saved)
			setenv("HOME", saved->c_str(), 1);
		else
			unsetenv("HOME");
		TS_ASSERT_EQUALS(rc, tagd::TAGD_OK);
		TS_ASSERT_EQUALS(opened.get(), other.get());
	}

	void test_close_through_previously_unused_symlink_spelling() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(root.string()), tagd::TAGD_OK);
		std::shared_ptr<tagd::tagspace> opened;
		TS_ASSERT_EQUALS(cfg.create_tagspace("animals", opened), tagd::TAGD_OK);
		opened.reset();
		std::filesystem::create_directory_symlink(root / "tagspaces/animals", root / "link");

		TS_ASSERT_EQUALS(cfg.close_tagspace((root / "link/animals.db").string()), tagd::TAGD_OK);
		std::ostringstream listing;
		TS_ASSERT_EQUALS(cfg.list_tagspaces(listing, nullptr), tagd::TAGD_OK);
		TS_ASSERT_DIFFERS(listing.str().find("animals\tregistered/closed"), std::string::npos);
	}

	void test_nonowning_borrow_does_not_install_shared_ownership() {
		tagd::config cfg;
		TS_ASSERT_EQUALS(cfg.init(""), tagd::TAGD_OK);

		auto borrowed = cfg.borrow(&cfg);
		TS_ASSERT_EQUALS(borrowed.get(), &cfg);
		TS_ASSERT(cfg.weak_from_this().expired());
	}
};
