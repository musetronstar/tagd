#pragma once
#include <cxxtest/TestSuite.h>
#include "filepile-files.h"
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <sstream>
#include <dirent.h>

// Linker wrappers exercise real filesystem failures without a production injection API.
namespace filepile_fault {
inline int read_error = 0, write_error = 0, sync_error = 0, link_error = 0, unlink_error = 0;
inline bool zero_write = false;
inline bool interrupted_read = false, interrupted_write = false, short_io = false;
inline unsigned sync_count = 0;
inline bool scan_error = false;
}
extern "C" {
struct dirent* __real_readdir(DIR*);
struct dirent* __wrap_readdir(DIR* directory) {
	if (filepile_fault::scan_error) {
		errno = EACCES;
		return nullptr;
	}
	return __real_readdir(directory);
}
ssize_t __real_read(int, void*, size_t);
ssize_t __real_write(int, const void*, size_t);
int __real_fsync(int);
int __real_linkat(int, const char*, int, const char*, int);
int __real_unlinkat(int, const char*, int);
ssize_t __wrap_read(int fd, void* p, size_t n) {
	if (filepile_fault::read_error) {
		errno = filepile_fault::read_error;
		return -1;
	}
	if (filepile_fault::interrupted_read) {
		filepile_fault::interrupted_read = false;
		errno = EINTR;
		return -1;
	}
	return __real_read(fd, p, filepile_fault::short_io ? std::min<size_t>(n, 3) : n);
}
ssize_t __wrap_write(int fd, const void* p, size_t n) {
	if (filepile_fault::zero_write)
		return 0;
	if (filepile_fault::interrupted_write) {
		filepile_fault::interrupted_write = false;
		errno = EINTR;
		return -1;
	}
	if (filepile_fault::write_error) {
		errno = filepile_fault::write_error;
		return -1;
	}
	return __real_write(fd, p, filepile_fault::short_io ? std::min<size_t>(n, 2) : n);
}
int __wrap_fsync(int fd) {
	++filepile_fault::sync_count;
	if (filepile_fault::sync_error) {
		errno = filepile_fault::sync_error;
		return -1;
	}
	return __real_fsync(fd);
}
int __wrap_linkat(int a, const char* b, int c, const char* d, int e) {
	if (filepile_fault::link_error) {
		errno = filepile_fault::link_error;
		return -1;
	}
	return __real_linkat(a, b, c, d, e);
}
int __wrap_unlinkat(int a, const char* b, int c) {
	if (filepile_fault::unlink_error) {
		errno = filepile_fault::unlink_error;
		return -1;
	}
	return __real_unlinkat(a, b, c);
}
}

class FilepileFilesTester : public CxxTest::TestSuite {
	std::filesystem::path root;
	std::string found;
	tagd::errorable diagnostics;
	void write(const std::string& text) {
		std::ofstream(root / "source", std::ios::binary) << text;
	}
public:
	void setUp() {
		auto pattern = (std::filesystem::temp_directory_path() / "tagd-filepile-XXXXXX").string();
		const auto* directory = mkdtemp(pattern.data());
		if (!directory)
			throw std::runtime_error("cannot create test directory");
		root = directory;
	}
	void tearDown() {
		filepile_fault::write_error = filepile_fault::sync_error = 0;
		filepile_fault::link_error = filepile_fault::unlink_error = 0;
		filepile_fault::short_io = filepile_fault::zero_write = false;
		filepile_fault::read_error = 0;
		filepile_fault::scan_error = false;
		std::filesystem::remove_all(root);
	}
	void test_stored_address_and_removal_use_the_pile_root() {
		write("addressed content");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url(tagd::file_url(
			(root / "source").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		const auto name = f.digest + ".txt";
		const tagd::url stored("filepile:///" + f.digest.substr(0, 2) + '/'
			+ f.digest.substr(2, 2) + '/' + name);
		std::string parsed;
		TS_ASSERT_EQUALS(tagd::filepile::stored_address(diagnostics, parsed, stored), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(parsed, name);
		TS_ASSERT_EQUALS(f.publish(name), tagd::TAGD_OK);
		tagd::cleanup_result removed;
		TS_ASSERT_EQUALS(tagd::filepile::remove_content(diagnostics, removed, root.string(), stored),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(removed.outcome, tagd::cleanup_outcome::removed);
		TS_ASSERT(!std::filesystem::exists(f.destination(name)));
		for (const auto& wrong : {tagd::file_url(f.destination(name)), "file://" + stored.path()}) {
			TS_ASSERT_EQUALS(tagd::filepile::stored_address(diagnostics, parsed, tagd::url(wrong)),
				tagd::TS_ERR);
		}
	}

	void test_diagnostics_name_source_ids_and_internal_roles() {
		write("abc");
		const tagd::url missing(tagd::file_url((root / "missing # %").string()));
		tagd::filepile::files absent;
		TS_ASSERT_EQUALS(absent.init(root.string(), missing), tagd::TS_NOT_FOUND);
		std::ostringstream text;
		absent.print_errors(text);
		TS_ASSERT_DIFFERS(text.str().find("open source: " + missing.id()), std::string::npos);
		TS_ASSERT_EQUALS(text.str().find((root / "missing # %").string()), std::string::npos);

		tagd::filepile::files staging;
		TS_ASSERT_EQUALS(staging.init(root.string(), tagd::url(tagd::file_url(
			(root / "source").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(::chmod((root / "filepile/.staging").c_str(), 0), 0);
		const auto stage_rc = staging.capture();
		TS_ASSERT_EQUALS(::chmod((root / "filepile/.staging").c_str(), 0700), 0);
		TS_ASSERT_EQUALS(stage_rc, tagd::TS_INTERNAL_ERR);
		text.str("");
		staging.print_errors(text);
		TS_ASSERT_DIFFERS(text.str().find("create staging:"), std::string::npos);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
		TS_ASSERT_EQUALS(text.str().find(".staging"), std::string::npos);

		const auto loop = root / "loop";
		std::filesystem::create_directory_symlink(loop, loop);
		tagd::filepile::files invalid_home;
		TS_ASSERT_DIFFERS(invalid_home.init(loop.string(), tagd::url(tagd::file_url(
			(root / "source").string()))), tagd::TAGD_OK);
		text.str("");
		invalid_home.print_errors(text);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
	}

	void test_shard_scan_failures_have_no_partial_content_identity() {
		write("abc");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url(tagd::file_url(
			(root / "source").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		const auto shard = root / "filepile" / f.digest.substr(0, 2) / f.digest.substr(2, 2);
		std::filesystem::create_directories(shard);
		for (const bool readdir_failure : {false, true}) {
			f.clear_errors();
			if (readdir_failure)
				filepile_fault::scan_error = true;
			else
				TS_ASSERT_EQUALS(::chmod(shard.c_str(), 0), 0);
			const auto rc = f.discover(found);
			filepile_fault::scan_error = false;
			TS_ASSERT_EQUALS(::chmod(shard.c_str(), 0700), 0);
			TS_ASSERT_DIFFERS(rc, tagd::TAGD_OK);
			std::ostringstream text;
			f.print_errors(text);
			TS_ASSERT_DIFFERS(text.str().find("scan filepile shard:"), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find(f.digest), std::string::npos);
			TS_ASSERT_EQUALS(text.str().find("filepile:///"), std::string::npos);
		}
	}

	void test_initialization_failure_is_explicit() {
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.capture(), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file:///nonexistent/tagd-source")),
			tagd::TS_NOT_FOUND);
		TS_ASSERT(f.has_errors());
		TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TS_MISUSE);
	}
	void test_canonical_source_validation() {
		tagd::url canonical;
		std::string path;
		for (const auto& input : {"file://LOCALHOST/tmp/a%20b", "FILE:///tmp/a", "file:///tmp/%7Ex"}) {
			TS_ASSERT_EQUALS(tagd::filepile::canonical_source(diagnostics, canonical, path, tagd::url(input)), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(canonical.host(), "");
		}
		for (const auto& input : {"file:///tmp/%2e%2e/a", "file:///tmp/a%2fb", "file:///tmp/a%00",
			"file:///tmp/a?x", "file://host/tmp/a", "file:///tmp//a", "file:///C:/a",
			"file:///tmp/a b", "file:///tmp/%FF", "file:///tmp/a\\b", "file:///tmp/%zz"}) {
			TS_ASSERT_DIFFERS(tagd::filepile::canonical_source(diagnostics, canonical, path, tagd::url(input)), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(tagd::filepile::canonical_source(diagnostics, canonical, path, tagd::url("file:///tmp/%252e")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(path, "/tmp/%2e");
		TS_ASSERT_EQUALS(canonical.id(), "file:///tmp/%252e");
	}
	void test_copy_publication_and_symlinks() {
		write("abc");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.validate_source(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
		TS_ASSERT_EQUALS(f.sample, "abc");
		const auto name = f.digest + ".txt";
		TS_ASSERT((f.discover(found) == tagd::TAGD_OK && found.empty()));
		TS_ASSERT_EQUALS(f.publish(name), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.discover(found), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found, name);
		TS_ASSERT_EQUALS(f.verify(name, f.digest), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
		TS_ASSERT(std::filesystem::is_empty(root / "filepile/.staging"));
		const auto final = root / "filepile" / f.digest.substr(0, 2) / f.digest.substr(2, 2) / name;
		std::filesystem::remove(final);
		std::filesystem::create_symlink(root / "source", final);
		TS_ASSERT_EQUALS(f.verify(name, f.digest), tagd::TS_ERR);
		std::filesystem::remove(final);
		mkfifo(final.c_str(), 0600);
		TS_ASSERT_EQUALS(f.verify(name, f.digest), tagd::TS_ERR);
	}
	void test_source_changes_and_ambiguous_objects() {
		write("abc");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		write("abcd");
		TS_ASSERT_EQUALS(f.validate_source(), tagd::TS_ERR);
		TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
		write("abc");
		tagd::filepile::files g;
		TS_ASSERT_EQUALS(g.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(g.capture(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(g.publish(g.digest + ".txt"), tagd::TAGD_OK);
		const auto shard = root / "filepile" / g.digest.substr(0, 2) / g.digest.substr(2, 2);
		std::ofstream(shard / (g.digest + ".bin")) << "abc";
		TS_ASSERT_EQUALS(g.discover(found), tagd::TS_AMBIGUOUS);
		std::ostringstream text;
		g.print_errors(text);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
		TS_ASSERT_DIFFERS(text.str().find(tagd::filepile_url(g.digest + ".txt")), std::string::npos);
		TS_ASSERT_DIFFERS(text.str().find(tagd::filepile_url(g.digest + ".bin")), std::string::npos);
	}
	void test_invalid_shard_name_is_not_reported_as_a_content_identity() {
		write("abc");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url(tagd::file_url(
			(root / "source").string()))), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		const auto shard = root / "filepile" / f.digest.substr(0, 2) / f.digest.substr(2, 2);
		std::filesystem::create_directories(shard);
		std::ofstream(shard / (f.digest + ".txt.alternate")) << "abc";
		TS_ASSERT_EQUALS(f.discover(found), tagd::TS_ERR);
		std::ostringstream text;
		f.print_errors(text);
		TS_ASSERT_EQUALS(text.str().find(root.string()), std::string::npos);
		TS_ASSERT_EQUALS(text.str().find("filepile:///"), std::string::npos);
	}

	void test_short_io_eintr_and_durability_failures() {
		write("a meaningful prefix of bytes");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
		filepile_fault::short_io = true;
		filepile_fault::interrupted_read = filepile_fault::interrupted_write = true;
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.sample, "a meaningful prefix of bytes");
		const auto name = f.digest + ".txt";
		filepile_fault::sync_error = EIO;
		TS_ASSERT_EQUALS(f.publish(name), tagd::TS_INTERNAL_ERR);
		TS_ASSERT((f.discover(found) == tagd::TAGD_OK && found.empty()));
		filepile_fault::sync_error = 0;
		filepile_fault::link_error = EIO;
		TS_ASSERT_EQUALS(f.publish(name), tagd::TS_INTERNAL_ERR);
		TS_ASSERT((f.discover(found) == tagd::TAGD_OK && found.empty()));
		filepile_fault::link_error = 0;
		filepile_fault::unlink_error = EACCES;
		TS_ASSERT_EQUALS(f.publish(name), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(f.discover(found), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found, name);
		filepile_fault::unlink_error = 0;
		TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.verify(name, f.digest, true), tagd::TAGD_OK);
		filepile_fault::sync_count = 0;
		{
			tagd::filepile::files reuse;
			TS_ASSERT_EQUALS(reuse.init(root.string(), f.source), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(reuse.capture(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(reuse.verify(name, f.digest), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(reuse.cleanup(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(filepile_fault::sync_count, 0U);
			TS_ASSERT_EQUALS(reuse.verify(name, f.digest, true), tagd::TAGD_OK);
			TS_ASSERT(filepile_fault::sync_count > 0);
		}
	}
	void test_failed_copy_cleans_owned_staging_and_path_replacement() {
		write("abc");
		for (int error : {ENOSPC, EDQUOT}) {
			tagd::filepile::files f;
			TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
			filepile_fault::write_error = error;
			TS_ASSERT_DIFFERS(f.capture(), tagd::TAGD_OK);
			filepile_fault::write_error = 0;
			TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
			TS_ASSERT(std::filesystem::is_empty(root / "filepile/.staging"));
		}
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
		std::filesystem::rename(root / "source", root / "old");
		write("abc");
		TS_ASSERT_EQUALS(f.validate_source(), tagd::TS_ERR);
	}
	void test_growth_truncation_zero_progress_and_read_failure() {
		for (const auto& replacement : {"a", "abcdef"}) {
			write("abc");
			tagd::filepile::files f;
			TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
			write(replacement);
			TS_ASSERT_DIFFERS(f.capture(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
			TS_ASSERT(std::filesystem::is_empty(root / "filepile/.staging"));
		}
		for (bool read_failure : {false, true}) {
			write("abc");
			tagd::filepile::files f;
			TS_ASSERT_EQUALS(f.init(root.string(), tagd::url("file://" + (root / "source").string())), tagd::TAGD_OK);
			filepile_fault::zero_write = !read_failure;
			filepile_fault::read_error = read_failure ? EIO : 0;
			TS_ASSERT_DIFFERS(f.capture(), tagd::TAGD_OK);
			filepile_fault::zero_write = false;
			filepile_fault::read_error = 0;
			filepile_fault::scan_error = false;
			TS_ASSERT_EQUALS(f.cleanup(), tagd::TAGD_OK);
		}
	}
	void test_maximum_canonical_url_pipeline_and_idempotence() {
		std::string input = "file:///";
		for (unsigned i = 0; i < 700; ++i) {
			input += "%21";
		}
		input += "abcd";
		TS_ASSERT_EQUALS(input.size(), 2112U);
		tagd::url canonical, repeated;
		std::string path;
		TS_ASSERT_EQUALS(tagd::filepile::canonical_source(diagnostics, canonical, path, tagd::url(input)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(tagd::filepile::canonical_source(diagnostics, repeated, path, canonical), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(canonical.id(), repeated.id());
		const auto link = "link:" + canonical.hduri() + "#001";
		TS_ASSERT(link.size() <= tagd::MAX_TAG_LEN);
		TS_ASSERT_EQUALS(link.substr(5, link.rfind('#') - 5), canonical.hduri());
		const auto last_link = "link:" + canonical.hduri() + "#999";
		TS_ASSERT(last_link.size() <= tagd::MAX_TAG_LEN);
		const tagd::url decoded(last_link.substr(5, last_link.rfind('#') - 5));
		TS_ASSERT_EQUALS(decoded.code(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(decoded.id(), canonical.id());
		TS_ASSERT_EQUALS(tagd::url(input + "a").code(), tagd::URL_MAX_LEN);
	}
	void test_restrictive_modes_and_all_managed_components() {
		// Control the test process umask so owner access does not depend on the runner.
		struct restore_mask {
			mode_t previous = ::umask(0077);
			~restore_mask() { ::umask(previous); }
		} mask;

		write("abc");
		const auto source = tagd::url("file://" + (root / "source").string());
		std::string hash;
		{
			tagd::filepile::files f;
			TS_ASSERT_EQUALS(f.init(root.string(), source), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
			hash = f.digest;
			TS_ASSERT_EQUALS(f.publish(hash + ".txt"), tagd::TAGD_OK);
			struct stat state{};
			TS_ASSERT_EQUALS(::stat(f.destination(hash + ".txt").c_str(), &state), 0);
			TS_ASSERT_EQUALS(state.st_mode & 0777, 0600);
			TS_ASSERT_EQUALS(::stat((root / "filepile").c_str(), &state), 0);
			TS_ASSERT_EQUALS(state.st_mode & 0777, 0700);
		}
		std::filesystem::create_directory(root / "outside");
		for (const auto& component : {std::string(".staging"), hash.substr(0, 2),
			hash.substr(0, 2) + '/' + hash.substr(2, 2)}) {
			std::filesystem::remove_all(root / "filepile");
			std::filesystem::create_directories((root / "filepile" / component).parent_path());
			std::filesystem::create_directory_symlink(root / "outside", root / "filepile" / component);
			tagd::filepile::files f;
			auto rc = f.init(root.string(), source);
			if (rc == tagd::TAGD_OK) {
				TS_ASSERT_EQUALS(f.capture(), tagd::TAGD_OK);
				rc = f.discover(found);
			}
			TS_ASSERT_EQUALS(rc, tagd::TS_ERR);
			TS_ASSERT(std::filesystem::is_empty(root / "outside"));
		}
	}
	void test_managed_directory_symlink() {
		write("abc");
		std::filesystem::create_directory(root / "outside");
		std::filesystem::create_symlink(root / "outside", root / "filepile");
		tagd::filepile::files f;
		TS_ASSERT_EQUALS(f.init(root.string(),
			tagd::url("file://" + (root / "source").string())), tagd::TS_ERR);
		TS_ASSERT(std::filesystem::is_empty(root / "outside"));
	}
};
