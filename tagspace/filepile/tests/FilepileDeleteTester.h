#pragma once

#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include "tagdb.h"
#include "filepile-files.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sqlite3.h>

namespace delete_fault {
inline bool commit = false, rollback = false, attempted_commit = false;
inline int unlink_after = -1;
inline unsigned unlinks = 0;
inline bool sync = false;
inline unsigned storage_preparations = 0;
inline bool classification_failure = false;
}

extern "C" int __real_sqlite3_step(sqlite3_stmt*);
extern "C" int __wrap_sqlite3_step(sqlite3_stmt* statement) {
	if (delete_fault::classification_failure &&
		std::string_view(sqlite3_sql(statement)).starts_with("WITH RECURSIVE sources"))
		return SQLITE_ERROR;
	return __real_sqlite3_step(statement);
}

extern "C" int __real_sqlite3_exec(sqlite3*, const char*, int(*)(void*, int, char**, char**), void*, char**);
extern "C" int __wrap_sqlite3_exec(sqlite3* db, const char* sql,
	int(*callback)(void*, int, char**, char**), void* data, char** error) {
	if (std::string_view(sql) == "PRAGMA synchronous=EXTRA")
		++delete_fault::storage_preparations;
	if (std::string_view(sql) == "COMMIT")
		delete_fault::attempted_commit = true;
	if ((delete_fault::commit && std::string_view(sql) == "COMMIT") ||
		(delete_fault::rollback && delete_fault::attempted_commit && std::string_view(sql) == "ROLLBACK"))
		return SQLITE_IOERR;
	return __real_sqlite3_exec(db, sql, callback, data, error);
}

extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* path, int flags) {
	if (delete_fault::unlink_after >= 0 &&
		delete_fault::unlinks++ >= unsigned(delete_fault::unlink_after)) {
		errno = EACCES;
		return -1;
	}
	return __real_unlinkat(fd, path, flags);
}

extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
	if (delete_fault::sync) {
		errno = EIO;
		return -1;
	}
	return __real_fsync(fd);
}

class FilepileDeleteTester : public CxxTest::TestSuite {
	std::filesystem::path home;
	std::unique_ptr<tagd::tagspace::persistent> space;
	std::string source;

	tagd::store_result store(const std::string& bytes, const std::string& name = "source") {
		std::ofstream(home / name) << bytes;
		tagd::store_result result;
		TS_ASSERT_EQUALS(space->store(result, tagd::url(tagd::filepile::path_url(
			(home / name).string()))), tagd::TAGD_OK);
		return result;
	}

	std::string path(const tagd::store_result& result) {
		std::string value;
		TS_ASSERT(tagd::uri_decode_checked(value, result.stored.path()));
		return value;
	}

	std::string dump() {
		std::ostringstream output;
		TS_ASSERT_EQUALS(space->dump(output), tagd::TAGD_OK);
		return output.str();
	}

public:
	void setUp() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagd-delete-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		TS_ASSERT(directory);
		home = directory;
		source = tagd::filepile::path_url((home / "source").string());
		space = std::make_unique<tagd::tagspace::persistent>();
		TS_ASSERT_EQUALS(space->create("test", home.string()), tagd::TAGD_OK);
	}

	void tearDown() {
		delete_fault::commit = delete_fault::rollback = delete_fault::attempted_commit = false;
		delete_fault::unlink_after = -1;
		delete_fault::unlinks = 0;
		delete_fault::sync = false;
		delete_fault::classification_failure = false;
		TAGSPACE_SET_LOGGER(nullptr);
		space.reset();
		std::filesystem::remove_all(home);
	}

	void test_delete_does_not_validate_unrelated_damaged_associations() {
		const auto target = store("target");
		const auto other = store("unrelated", "other");
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		tagd::abstract_tag damage(other.link);
		TS_ASSERT_EQUALS(damage.relation(tagd::HARD_TAG_TO, other.stored.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->del(damage, nullptr), tagd::TAGD_OK);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(!space->exists(target.link));
		TS_ASSERT(space->exists(other.link));
		TS_ASSERT(std::filesystem::exists(path(other)));
	}

	void test_classification_failure_preserves_the_backend_error() {
		const auto stored = store("first");
		delete_fault::classification_failure = true;
		tagd::filepile_delete_result result;
		const auto rc = space->delete_storage(result, tagd::url(source));
		delete_fault::classification_failure = false;
		TS_ASSERT_EQUALS(rc, tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(space->code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(!result.committed);
		TS_ASSERT(space->exists(stored.link));
		TS_ASSERT(std::filesystem::exists(path(stored)));
	}

	void test_storage_and_deletion_keep_the_home_resolved_at_configuration() {
		const auto alias = home / "alias", other = home / "other-home";
		std::filesystem::create_directory(other);
		std::filesystem::create_directory_symlink(home, alias);
		space->filepile_home(alias.string());
		const auto first = store("first");
		std::filesystem::remove(alias);
		std::filesystem::create_directory_symlink(other, alias);
		const auto second = store("second");
		TS_ASSERT(std::filesystem::path(path(second)).string().starts_with((home / "filepile").string()));
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(result.committed);
		TS_ASSERT(!std::filesystem::exists(path(first)));
		TS_ASSERT(!std::filesystem::exists(path(second)));
		TS_ASSERT(!std::filesystem::exists(other / "filepile"));
	}

	void test_short_stored_address_returns_an_error_without_removing_anything() {
		for (const auto* name : {"a", "ab", "abc"}) {
			tagd::cleanup_result result;
			const auto stored = tagd::url(tagd::filepile::path_url((home / "filepile" / name).string()));
			TS_ASSERT_EQUALS(tagd::filepile::remove_content(*space, result, home.string(), stored), tagd::TS_ERR);
			TS_ASSERT_EQUALS(result.outcome, tagd::cleanup_outcome::not_attempted);
		}
	}

	void test_source_cascade_preserves_shared_content_and_literal_audit_mentions() {
		const auto first = store("first");
		const auto shared = store("first", "other");
		const auto second = store("second");
		auto session = space->get_session();
		tagd::event audit(session, "test", tagd::id_string(tagd::HARD_TAG_FILEPILE_STORE_EVENT));
		TS_ASSERT_EQUALS(audit.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE,
			first.source.hduri() + " " + first.link), tagd::TAGD_OK);
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(audit, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(audit), tagd::TS_MISUSE);
		tagd::abstract_tag reparent(audit.id(), tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(space->put(reparent), tagd::TS_MISUSE);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(result.committed);
		TS_ASSERT(!space->exists(first.source.hduri()));
		TS_ASSERT(!space->exists(first.link));
		TS_ASSERT(!space->exists(second.link));
		TS_ASSERT(space->exists(shared.link));
		TS_ASSERT(space->exists(first.stored.hduri()));
		TS_ASSERT(!space->exists(second.stored.hduri()));
		TS_ASSERT(space->exists(audit.id()));
		TS_ASSERT(std::filesystem::exists(path(first)));
		TS_ASSERT(!std::filesystem::exists(path(second)));
		TS_ASSERT(std::filesystem::exists(home / "source"));
		TS_ASSERT_EQUALS(result.content.size(), 2u);
	}

	void test_stored_cascade_preserves_sources_and_reuses_only_highest_revision() {
		const auto first = store("first");
		const auto second = store("second");
		const auto third = store("third");
		const auto shared = store("third", "other");
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(first.stored.id())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(store("fourth").revision, 4u);
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(third.stored.id())), tagd::TAGD_OK);
		TS_ASSERT(space->exists(first.source.hduri()));
		TS_ASSERT(space->exists(shared.source.hduri()));
		TS_ASSERT(!space->exists(shared.link));
		const auto fourth = store("fourth", "other");
		TS_ASSERT_EQUALS(fourth.revision, 1u);
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(fourth.stored.id())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(store("fifth").revision, 3u);
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(store("restart").revision, 1u);
		TS_ASSERT(!std::filesystem::exists(path(second)));
	}

	void test_predicate_delete_and_nonmanaged_fallback_preserve_semantics() {
		const auto stored = store("first");
		tagd::abstract_tag description(stored.link);
		TS_ASSERT_EQUALS(description.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "user"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(description), tagd::TAGD_OK);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, description), tagd::TAGD_OK);
		TS_ASSERT(space->exists(stored.link));
		TS_ASSERT(std::filesystem::exists(path(stored)));
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::abstract_tag(stored.link)), tagd::TS_MISUSE);
		tagd::url unowned("https://example.com/unowned");
		TS_ASSERT_EQUALS(space->put(unowned), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->delete_storage(result, unowned), tagd::TAGD_OK);
		TS_ASSERT(!space->exists(unowned.hduri()));
		tagd::tagspace::memory memory;
		TS_ASSERT_EQUALS(memory.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(memory.put(unowned), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(memory.delete_storage(result, unowned), tagd::TAGD_OK);
	}

	void test_unrelated_dependencies_block_the_entire_cascade() {
		const auto stored = store("first");
		for (const auto& target : {stored.source.hduri(), stored.link, stored.stored.hduri()}) {
			tagd::abstract_tag dependent("dependent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
			TS_ASSERT_EQUALS(dependent.relation(tagd::HARD_TAG_HAS, target), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(space->put(dependent), tagd::TAGD_OK);
			const auto before = dump();
			tagd::filepile_delete_result result;
			TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_DEPENDENCY);
			TS_ASSERT(!result.committed);
			TS_ASSERT_EQUALS(dump(), before);
			TS_ASSERT(std::filesystem::exists(path(stored)));
			TS_ASSERT_EQUALS(space->del(tagd::abstract_tag("dependent")), tagd::TAGD_OK);
		}
	}

	void test_missing_content_warns_and_logs_without_failing() {
		const auto stored = store("first");
		std::filesystem::remove(path(stored));
		TS_ASSERT_EQUALS(space->del(tagd::url(stored.stored.id())), tagd::TS_DEPENDENCY);
		TS_ASSERT_EQUALS(space->del(tagd::abstract_tag(stored.link)), tagd::TS_DEPENDENCY);
		std::ostringstream output;
		tagd::logger logger(output);
		TAGSPACE_SET_LOGGER(&logger);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.warnings.size(), 1u);
		TS_ASSERT_EQUALS(result.warnings.front().code(), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(result.content.front().outcome, tagd::cleanup_outcome::already_missing);
		TS_ASSERT_DIFFERS(output.str().find("already missing"), std::string::npos);
		TS_ASSERT_DIFFERS(output.str().find("_event:filepile_delete"), std::string::npos);
		TS_ASSERT_DIFFERS(output.str().find(stored.link), std::string::npos);
		TS_ASSERT_EQUALS(dump().find("ev:"), std::string::npos);
	}

	void test_cleanup_failure_is_partial_completion_after_semantic_commit() {
		const auto first = store("first");
		const auto second = store("second");
		delete_fault::unlink_after = 1;
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(result.committed);
		TS_ASSERT(!space->exists(first.link));
		TS_ASSERT(!space->exists(second.link));
		TS_ASSERT_EQUALS(unsigned(std::filesystem::exists(path(first))) +
			unsigned(std::filesystem::exists(path(second))), 1u);
		TS_ASSERT_EQUALS(result.content.size(), 2u);
		unsigned removed = 0, failed = 0;
		for (const auto& content : result.content) {
			removed += content.outcome == tagd::cleanup_outcome::removed;
			failed += content.outcome == tagd::cleanup_outcome::failed;
		}
		TS_ASSERT_EQUALS(removed, 1u);
		TS_ASSERT_EQUALS(failed, 1u);
		std::ostringstream diagnostic;
		space->print_errors(diagnostic);
		TS_ASSERT_DIFFERS(diagnostic.str().find("deletion committed"), std::string::npos);
	}

	void test_database_commit_failure_and_unknown_outcome_never_remove_bytes() {
		const auto stored = store("first");
		const auto before = dump();
		delete_fault::commit = true;
		tagd::filepile_delete_result result;
		TS_ASSERT_DIFFERS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(!result.committed);
		TS_ASSERT(!result.commit_unknown);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT(std::filesystem::exists(path(stored)));
		delete_fault::attempted_commit = false;
		delete_fault::rollback = true;
		TS_ASSERT_DIFFERS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(result.commit_unknown);
		TS_ASSERT(std::filesystem::exists(path(stored)));
	}

	void test_required_database_absence_or_lock_fails_before_changes() {
		const auto stored = store("first");
		const auto database = tagd::tagspace_database_path(home.string(), "test");
		sqlite3* other = nullptr;
		TS_ASSERT_EQUALS(sqlite3_open(database.c_str(), &other), SQLITE_OK);
		TS_ASSERT_EQUALS(sqlite3_exec(other, "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr), SQLITE_OK);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(std::filesystem::exists(path(stored)));
		TS_ASSERT_EQUALS(sqlite3_exec(other, "ROLLBACK", nullptr, nullptr, nullptr), SQLITE_OK);
		sqlite3_close(other);
		std::filesystem::rename(database, database + ".saved");
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_NOT_FOUND);
		TS_ASSERT(std::filesystem::exists(path(stored)));
		std::filesystem::rename(database + ".saved", database);
		TS_ASSERT(space->exists(stored.link));
	}

	void test_unreadable_database_is_not_an_empty_reference_set() {
		const auto stored = store("first");
		const auto database = tagd::tagspace_database_path(home.string(), "test");
		TS_ASSERT_EQUALS(::chmod(database.c_str(), 0), 0);
		tagd::filepile_delete_result result;
		const auto rc = space->delete_storage(result, tagd::url(source));
		TS_ASSERT_EQUALS(::chmod(database.c_str(), 0600), 0);
		TS_ASSERT_EQUALS(rc, tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(!result.committed);
		TS_ASSERT(space->exists(stored.link));
		TS_ASSERT(std::filesystem::exists(path(stored)));
		std::ostringstream diagnostic;
		space->print_errors(diagnostic);
		TS_ASSERT_DIFFERS(diagnostic.str().find(database), std::string::npos);
	}

	void test_missing_home_with_an_external_database_warns_after_commit() {
		const auto stored = store("first");
		// Moving the DB out leaves accessible reference data when the content home disappears.
		const auto database = tagd::tagspace_database_path(home.string(), "test");
		const auto external = home.string() + ".db";
		space.reset();
		std::filesystem::rename(database, external);
		std::filesystem::remove_all(home);
		space = std::make_unique<tagd::tagspace::persistent>();
		TS_ASSERT_EQUALS(space->open_file(external), tagd::TAGD_OK);
		space->filepile_home(home.string());
		tagd::filepile_delete_result result;
		const auto rc = space->delete_storage(result, tagd::url(source));
		space.reset();
		std::filesystem::remove(external);
		TS_ASSERT_EQUALS(rc, tagd::TAGD_OK);
		TS_ASSERT(result.committed);
		TS_ASSERT_EQUALS(result.warnings.size(), 1u);
	}

	void test_dual_roles_and_malformed_links_are_rejected_without_cleanup() {
		const auto first = store("first");
		const auto second = store("second", "other");
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		// Only a private legacy-data fixture may construct an otherwise forbidden dual role.
		tagd::abstract_tag link("link:" + first.stored.hduri() + "#001", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_LINK_FILEPILE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_FROM, first.stored.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_TO, second.stored.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_REVISION,
			"1", tagd::OP_EQ, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(link, nullptr), tagd::TAGD_OK);
		const auto before = dump();
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(first.stored.id())), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT(std::filesystem::exists(path(first)));

		tagd::abstract_tag damage(first.link);
		TS_ASSERT_EQUALS(damage.relation(tagd::HARD_TAG_TO, first.stored.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->del(damage, nullptr), tagd::TAGD_OK);
		const auto damaged = dump();
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_ERR);
		TS_ASSERT_EQUALS(dump(), damaged);
		TS_ASSERT(std::filesystem::exists(path(first)));
	}

	void test_deleted_revision_999_does_not_exhaust_the_source_forever() {
		const auto first = store("first");
		auto fixture = tagdb::make_backend();
		TS_ASSERT_EQUALS(fixture->open({tagdb::storage_kind::file,
			tagd::tagspace_database_path(home.string(), "test")}), tagd::TAGD_OK);
		tagd::abstract_tag last("link:" + first.source.hduri() + "#999", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_LINK_FILEPILE, tagd::POS_TAG);
		TS_ASSERT_EQUALS(last.relation(tagd::HARD_TAG_FROM, first.source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(last.relation(tagd::HARD_TAG_TO, first.stored.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(last.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_REVISION,
			"999", tagd::OP_EQ, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(fixture->put(last, nullptr), tagd::TAGD_OK);
		std::ofstream(home / "source") << "next";
		tagd::store_result stored;
		TS_ASSERT_EQUALS(space->store(stored, tagd::url(source)), tagd::TS_ERR);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(first.stored.id())), tagd::TAGD_OK);
		TS_ASSERT(space->exists(first.source.hduri()));
		TS_ASSERT_EQUALS(store("next").revision, 1u);
	}

	void test_type_dependencies_and_managed_symlinks_do_not_authorize_cleanup() {
		const auto stored = store("first");
		tagd::abstract_tag dependent("dependent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(dependent.type_relation(tagd::HARD_TAG_TYPE_OF, stored.source.hduri()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(space->put(dependent), tagd::TAGD_OK);
		const auto before = dump();
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_DEPENDENCY);
		TS_ASSERT_EQUALS(dump(), before);
		TS_ASSERT_EQUALS(space->del(tagd::abstract_tag("dependent")), tagd::TAGD_OK);
		std::filesystem::remove(path(stored));
		std::filesystem::create_symlink(home / "source", path(stored));
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(result.committed);
		TS_ASSERT_EQUALS(result.content.front().outcome, tagd::cleanup_outcome::failed);
		TS_ASSERT(std::filesystem::is_symlink(path(stored)));
		TS_ASSERT(std::filesystem::exists(home / "source"));
	}

	void test_unlink_sync_failure_reports_removed_bytes_and_failed_durability() {
		const auto stored = store("first");
		delete_fault::sync = true;
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(result.committed);
		TS_ASSERT_EQUALS(result.content.front().outcome, tagd::cleanup_outcome::removed);
		TS_ASSERT_EQUALS(result.content.front().code, tagd::TS_NOT_AVAILABLE);
		TS_ASSERT(!std::filesystem::exists(path(stored)));
	}

	void test_nonmanaged_delete_preserves_identifier_and_durability_policy() {
		for (const auto* id : {"file://localhost/nonexistent", "file:///nonexistent?query",
			"https://example.com/plain"}) {
			tagd::url plain(id);
			TS_ASSERT_EQUALS(space->put(plain), tagd::TAGD_OK);
			delete_fault::storage_preparations = 0;
			tagd::filepile_delete_result result;
			TS_ASSERT_EQUALS(space->delete_storage(result, plain), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(delete_fault::storage_preparations, 0u);
			TS_ASSERT(!space->exists(plain.hduri()));
		}
	}

	void test_nonmanaged_fallback_does_not_require_a_filepile_home() {
		const auto stored = store("first");
		tagd::url unowned("https://example.com/unowned");
		TS_ASSERT_EQUALS(space->put(unowned), tagd::TAGD_OK);
		tagd::tagspace::persistent direct;
		TS_ASSERT_EQUALS(direct.open_file(tagd::tagspace_database_path(home.string(), "test")), tagd::TAGD_OK);
		tagd::filepile_delete_result result;
		TS_ASSERT_EQUALS(direct.delete_storage(result, unowned), tagd::TAGD_OK);
		TS_ASSERT(std::filesystem::exists(path(stored)));
		TS_ASSERT(space->exists(stored.link));
	}

	void test_missing_warning_obeys_the_normal_logger_level() {
		std::ostringstream messages;
		tagd::logger logger(messages);
		TAGSPACE_SET_LOGGER(&logger);
		for (const auto level : {tagd::log_level::WARNING, tagd::log_level::ERROR}) {
			logger.level(tagd::HARD_TAG_ROLE_TAGSPACE, level);
			const auto stored = store("first");
			std::filesystem::remove(path(stored));
			messages.str("");
			tagd::filepile_delete_result result;
			TS_ASSERT_EQUALS(space->delete_storage(result, tagd::url(source)), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(result.warnings.size(), 1u);
			TS_ASSERT_EQUALS(messages.str().find("_event:filepile_delete"), std::string::npos);
			TS_ASSERT_EQUALS(messages.str().find("stored content already missing") == std::string::npos,
				level == tagd::log_level::ERROR);
		}
	}

};
