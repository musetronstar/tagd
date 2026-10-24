#pragma once
#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include "tagd/url.h"
#include "filepile-files.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sqlite3.h>

namespace semantic_fault {
inline bool commit = false;
inline bool rollback = false;
inline bool premature_eof = false, cleanup_failure = false;
inline size_t stored_bytes_read = 0;
}
extern "C" int __real_sqlite3_exec(sqlite3*, const char*, int(*)(void*, int, char**, char**), void*, char**);
extern "C" int __wrap_sqlite3_exec(sqlite3* db, const char* sql,
	int(*callback)(void*, int, char**, char**), void* data, char** error) {
	if ((semantic_fault::commit && std::string_view(sql) == "COMMIT") ||
		(semantic_fault::rollback && std::string_view(sql) == "ROLLBACK"))
		return SQLITE_IOERR;
	return __real_sqlite3_exec(db, sql, callback, data, error);
}

extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
	if (semantic_fault::premature_eof)
		return 0;
	const auto n = __real_read(fd, buffer, size);
	char path[4096];
	const auto length = ::readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), path, sizeof(path));
	if (n > 0 && length > 0 && std::string_view(path, length).find("/filepile/") != std::string_view::npos)
		semantic_fault::stored_bytes_read += n;
	return n;
}

extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* path, int flags) {
	if (semantic_fault::cleanup_failure) {
		errno = EACCES;
		return -1;
	}
	return __real_unlinkat(fd, path, flags);
}

class FilepileTester : public CxxTest::TestSuite {
	std::filesystem::path home;
	std::string source;
	void write(const std::string& bytes) {
		std::ofstream(home / "source", std::ios::binary) << bytes;
	}
	std::string dump(tagd::tagspace& ts) {
		std::ostringstream out;
		TS_ASSERT_EQUALS(ts.dump(out), tagd::TAGD_OK);
		return out.str();
	}
public:
	void setUp() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagd-store-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		if (!directory)
			throw std::runtime_error("cannot create test directory");
		home = directory;
		source = tagd::filepile::path_url((home / "source").string());
		write("some text content\n");
	}
	void tearDown() {
		semantic_fault::commit = semantic_fault::rollback = false;
		semantic_fault::premature_eof = semantic_fault::cleanup_failure = false;
		std::filesystem::remove_all(home);
	}
	void test_cleanup_failure_preserves_initiating_status_and_both_diagnostics() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		auto session = ts.get_session();
		tagd::store_result result;

		semantic_fault::premature_eof = semantic_fault::cleanup_failure = true;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source), &session), tagd::TS_ERR);
		semantic_fault::premature_eof = semantic_fault::cleanup_failure = false;

		TS_ASSERT_EQUALS(ts.code(), tagd::TS_ERR);
		TS_ASSERT_EQUALS(session.code(), tagd::TS_ERR);
		std::ostringstream errors;
		ts.print_errors(errors);
		TS_ASSERT(errors.str().find("source shrank") != std::string::npos);
		TS_ASSERT(errors.str().find("unlink staging") != std::string::npos);
		TS_ASSERT(result.link.empty());
	}
	void test_reused_object_is_hashed_once_per_store() {
		const std::string bytes(128 * 1024, 'x');
		write(bytes);
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);

		std::filesystem::copy_file(home / "source", home / "copy");
		semantic_fault::stored_bytes_read = 0;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(tagd::filepile::path_url((home / "copy").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(semantic_fault::stored_bytes_read, bytes.size());

		const std::string changed = "changed content";
		write(changed);
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		write(bytes);
		semantic_fault::stored_bytes_read = 0;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		// The previous revision is still verified; incoming stored bytes are read only once.
		TS_ASSERT_EQUALS(semantic_fault::stored_bytes_read, bytes.size() + changed.size());
	}

	void test_revisions_duplicate_and_metadata_preservation() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::store_result first, next;
		TS_ASSERT_EQUALS(ts.store(first, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.revision, 1U);
		TS_ASSERT_EQUALS(first.link, "link:" + first.source.hduri() + "#001");
		TS_ASSERT_EQUALS(first.stored.super_object(), tagd::HARD_TAG_FILE_STORED);
		const auto before = dump(ts);
		TS_ASSERT_EQUALS(ts.store(next, tagd::url(source)), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(next.outcome, tagd::store_outcome::unchanged);
		TS_ASSERT_EQUALS(next.link, first.link);
		TS_ASSERT_EQUALS(dump(ts), before);
		ts.clear_errors();
		TS_ASSERT_EQUALS(ts.store(next, tagd::url(source), nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
		TS_ASSERT(!ts.has_errors());
		write("changed bytes\n");
		TS_ASSERT_EQUALS(ts.store(next, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(next.revision, 2U);
		TS_ASSERT_DIFFERS(next.stored.id(), first.stored.id());
		write("some text content\n");
		TS_ASSERT_EQUALS(ts.store(next, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(next.revision, 3U);
		TS_ASSERT_EQUALS(next.stored.id(), first.stored.id());
	}
	void test_shared_home_independent_histories_and_renames() {
		tagd::tagspace::persistent a, b;
		TS_ASSERT_EQUALS(a.create("a", home.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(b.create("b", home.string()), tagd::TAGD_OK);
		tagd::store_result one, two;
		TS_ASSERT_EQUALS(a.store(one, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(b.store(two, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(one.stored.id(), two.stored.id());
		TS_ASSERT_EQUALS(two.revision, 1U);
		std::filesystem::rename(home / "source", home / "renamed");
		TS_ASSERT_EQUALS(a.store(two, tagd::url(tagd::filepile::path_url((home / "renamed").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(one.stored.id(), two.stored.id());
		TS_ASSERT_DIFFERS(one.link, two.link);
		TS_ASSERT_EQUALS(two.revision, 1U);
		TS_ASSERT(a.exists(one.source.hduri()));
		TS_ASSERT(a.exists(one.link));
	}
	void test_eligibility_and_unsupported_fetcher_leave_no_mutations() {
		tagd::tagspace::memory memory;
		TS_ASSERT_EQUALS(memory.init(), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(memory.store(result, tagd::url(source)), tagd::TS_MISUSE);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.store(result, tagd::url("https://example.com/file")), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT(!std::filesystem::exists(home / "filepile"));
		tagd::tagspace::persistent direct;
		TS_ASSERT_EQUALS(direct.open_file(tagd::tagspace_database_path(home.string(), "test")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.store(result, tagd::url(source)), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(result.revision, 0U);
	}
	void test_missing_and_corrupt_objects_are_not_repaired() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		tagd::url canonical;
		std::string path;
		TS_ASSERT_EQUALS(tagd::filepile::canonical_source(ts, canonical, path, result.stored), tagd::TAGD_OK);
		const auto before = dump(ts);
		std::ofstream(path) << "corrupt";
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source), nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TS_ERR);
		TS_ASSERT_EQUALS(result.revision, 0U);
		TS_ASSERT_EQUALS(dump(ts), before);
		std::filesystem::remove(path);
		write("different content");
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TS_NOT_FOUND);
		TS_ASSERT(!std::filesystem::exists(path));
		TS_ASSERT_EQUALS(dump(ts), before);
	}
	void test_returning_to_missing_recorded_content_does_not_recreate_it() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::store_result one, two;
		TS_ASSERT_EQUALS(ts.store(one, tagd::url(source)), tagd::TAGD_OK);

		write("second content");
		TS_ASSERT_EQUALS(ts.store(two, tagd::url(source)), tagd::TAGD_OK);
		tagd::url canonical;
		std::string path;
		TS_ASSERT_EQUALS(tagd::filepile::canonical_source(ts, canonical, path, one.stored), tagd::TAGD_OK);
		std::filesystem::rename(path, path + ".alternate");

		write("some text content\n");
		const auto before = dump(ts);
		TS_ASSERT_EQUALS(ts.store(two, tagd::url(source)), tagd::TS_NOT_FOUND);
		TS_ASSERT(!std::filesystem::exists(path));
		TS_ASSERT_EQUALS(dump(ts), before);
	}
	void test_semantic_failure_retains_complete_content_and_rolls_back_all_tags() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		const auto before = dump(ts);
		semantic_fault::commit = true;
		tagd::store_result result;
		TS_ASSERT_DIFFERS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.outcome, tagd::store_outcome::none);
		semantic_fault::commit = false;
		TS_ASSERT_EQUALS(dump(ts), before);
		TS_ASSERT(!ts.exists(tagd::url(source).hduri()));
		unsigned files = 0;
		for (const auto& item : std::filesystem::recursive_directory_iterator(home / "filepile")) {
			if (item.is_regular_file()) ++files;
		}
		TS_ASSERT_EQUALS(files, 1U);
		ts.clear_errors();
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.revision, 1U);
	}
	void test_unknown_commit_outcome_blocks_further_writes() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		semantic_fault::commit = semantic_fault::rollback = true;
		tagd::store_result result;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(result.revision, 0U);
		semantic_fault::commit = semantic_fault::rollback = false;
		semantic_fault::premature_eof = semantic_fault::cleanup_failure = false;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("blocked", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TS_INTERNAL_ERR);
	}
	void test_revision_bound_and_preserving_an_existing_extension() {
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::filepile::files orphan;
		TS_ASSERT_EQUALS(orphan.init(home.string(), tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(orphan.capture(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(orphan.publish(orphan.digest + ".old"), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(result.stored.id().ends_with(".old"));
		for (unsigned revision : {9, 10, 99, 100, 999}) {
			std::string digits = std::to_string(revision);
			tagd::abstract_tag link("link:" + result.source.hduri() + '#' +
				std::string(3 - digits.size(), '0') + digits, tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_LINK_FILEPILE, tagd::POS_TAG);
			TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_FROM, result.source.hduri()), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_TO, result.stored.hduri()), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(link.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_REVISION, digits), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.put(link), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.store(result, tagd::url(source), nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(result.revision, revision);
			TS_ASSERT_EQUALS(result.link, link.id());
		}
		const auto before = dump(ts);
		write("new content");
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TS_ERR);
		TS_ASSERT_EQUALS(dump(ts), before);
	}
	void test_home_alias_encoding_symlink_identity_and_metadata() {
		std::filesystem::create_directory(home / "space # %");
		std::filesystem::create_directory_symlink(home / "space # %", home / "alias");
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", (home / "alias").string()), tagd::TAGD_OK);
		tagd::store_result first, second;
		TS_ASSERT_EQUALS(ts.store(first, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT(first.stored.id().find("space%20%23%20%25/filepile/") != std::string::npos);
		std::filesystem::create_symlink(home / "source", home / "symlink");
		TS_ASSERT_EQUALS(ts.store(second, tagd::url(tagd::filepile::path_url((home / "symlink").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(first.stored.id(), second.stored.id());
		TS_ASSERT_DIFFERS(first.source.id(), second.source.id());
		TS_ASSERT_EQUALS(second.revision, 1U);
		tagd::url stored;
		TS_ASSERT_EQUALS(ts.get(stored, first.stored.id()), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(stored.super_object(), tagd::HARD_TAG_FILE_STORED);
		size_t metadata_count = 0;
		for (const auto& p : stored.relations) {
			if (p.relator == tagd::HARD_TAG_HAS &&
				(p.object == tagd::HARD_TAG_HASH_SHA256 || p.object == tagd::HARD_TAG_FILE_SIZE ||
				 p.object == tagd::HARD_TAG_MEDIA_TYPE || p.object == tagd::HARD_TAG_MTIME))
				++metadata_count;
		}
		TS_ASSERT_EQUALS(metadata_count, 4U);
		const auto before = dump(ts);
		TS_ASSERT(before.find("_file_size = 18") != std::string::npos);
		TS_ASSERT(before.find("_stored_time = \"") != std::string::npos);
		TS_ASSERT(before.find("_media_type = \"text/plain\"") != std::string::npos);
	}
	void test_empty_warning_is_separate_from_status() {
		write("");
		tagd::tagspace::persistent ts;
		TS_ASSERT_EQUALS(ts.create("test", home.string()), tagd::TAGD_OK);
		tagd::store_result result;
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(result.warnings.size(), 1U);
		TS_ASSERT(!ts.has_errors());
		TS_ASSERT_EQUALS(ts.store(result, tagd::url(source)), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(result.warnings.size(), 1U);
	}
};
