#include "tagdb/sqlite.h"
#include <filesystem>
#include <cstring>
#include <climits>
#include <fcntl.h>
#include <unistd.h>

namespace tagdb {
namespace fs = std::filesystem;

namespace {
/*
 * Rollback owns only the new database and newly created empty directories.
 * Never recursively remove a directory: it may predate this attempt or gain
 * unrelated contents while initialization is running.
 */
struct creation_guard {
	fs::path database;
	std::vector<fs::path> directories;
	bool committed = false;
	~creation_guard() {
		if (committed)
			return;
		std::error_code ec;
		if (!database.empty())
			fs::remove(database, ec);
		for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
			fs::remove(*it, ec);
		}
	}
};
}

std::unique_ptr<tagdb> make_backend(relation_semantics semantics) {
	return std::make_unique<sqlite>(semantics);
}

std::string storage_directory(const std::string& home) {
	return (fs::path(home) / "tagspaces").string();
}

std::string storage_filename(const std::string& name) {
	return name + ".db";
}

std::string storage_path(const std::string& home, const std::string& name) {
	return (fs::path(storage_directory(home)) / name / storage_filename(name)).string();
}

bool accepts_storage_file(const std::string& path) {
	return fs::path(path).extension() == ".db";
}

tagd::code sqlite::resolve_referents(tagd::abstract_tag& result,
	const tagd::abstract_tag& input, session* ssn) {
	reset(ssn);
	auto resolved = ssn ? decode_referents(input, ssn) : input;
	if (code() == tagd::TAGD_OK)
		result = std::move(resolved);

	return code();
}

tagd::code sqlite::open(const storage_location& location) {
	if (_db)
		return error(tagd::TS_MISUSE, "storage is already initialized");

	clear_errors();
	if (location.kind == storage_kind::memory)
		return init(":memory:");
	if (location.kind != storage_kind::file)
		return error(tagd::TS_NOT_IMPLEMENTED, "backend does not support remote storage");

	const auto& path = location.resource;
	if (path.empty() || path.find('\0') != std::string::npos)
		return error(tagd::TS_MISUSE, "invalid database path");

	std::error_code ec;
	const bool create = location.mode == open_mode::create;
	creation_guard guard;
	if (create) {
		// Existing sidecars are not owned by this attempt and must never be claimed.
		for (const char* suffix : {"-journal", "-wal", "-shm"}) {
			auto status = std::filesystem::symlink_status(path + suffix, ec);
			if (ec == std::errc::no_such_file_or_directory)
				ec.clear();
			if (ec || std::filesystem::exists(status))
				return error(tagd::TS_ERR, "existing or inaccessible database sidecar");
		}

		// Exclusive reservation preserves create-only semantics without creating parents.
		const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
		if (fd < 0)
			return error(errno == EEXIST ? tagd::TS_DUPLICATE : tagd::TS_ERR,
				"create database: " + std::string(std::strerror(errno)));
		::close(fd);
		guard.database = path;
	} else if (!std::filesystem::is_regular_file(path, ec)) {
		return error(tagd::TS_NOT_FOUND, "no database file: " + path);
	}

	// A removal racing with open must not cause implicit database creation.
	auto rc = init(path, false);
	if (rc != tagd::TAGD_OK)
		close();
	guard.committed = rc == tagd::TAGD_OK;

	return rc;
}

tagd::code sqlite::open_named(
	const std::string& name, const std::string& home, open_mode mode
) {
	using namespace tagd;
	const bool create = mode == open_mode::create;

	if (_db)
		return error(TS_MISUSE, "tagspace is already initialized");

	clear_errors();
	if (!valid_storage_name(name))
		return error(TS_ERR, "invalid tagspace name");

	// Application-owned tagspaces must never resolve to persistent user storage.
	if (name.front() == '_')
		return error(TS_ERR, "reserved tagspace name");
	if (home.empty() || home.find('\0') != std::string::npos)
		return error(TS_ERR, "invalid tagd home");

	std::error_code ec;
	fs::path home_path(home);
	/*
	 * Query the nearest existing parent before using the name in a path.
	 * NAME_MAX counts bytes, including the database suffix, not Unicode scalars.
	 */
	auto parent = fs::absolute(home_path, ec);
	if (ec)
		return error(TS_INTERNAL_ERR, ec.message());
	while (!fs::exists(parent, ec) && !ec && parent.has_relative_path()) {
		parent = parent.parent_path();
	}
	if (ec)
		return error(TS_INTERNAL_ERR, ec.message());
	long limit = pathconf(parent.c_str(), _PC_NAME_MAX);
	if (limit < 0)
		limit = NAME_MAX;
	if (storage_filename(name).size() > static_cast<size_t>(limit))
		return error(TS_ERR, "tagspace name exceeds filesystem limit");

	const fs::path tagspaces(storage_directory(home));
	const fs::path database(storage_path(home, name));
	const auto dir = database.parent_path();
	for (const auto& path : {tagspaces, dir, database}) {
		auto status = fs::symlink_status(path, ec);
		if (ec == std::errc::no_such_file_or_directory)
			ec.clear();
		if (ec)
			return error(TS_INTERNAL_ERR, ec.message());
		if (fs::is_symlink(status))
			return error(TS_ERR, "tagspace storage path is a symbolic link");
	}
	const bool present = fs::exists(database, ec);
	if (ec)
		return error(TS_INTERNAL_ERR, ec.message());
	/*
	 * The database, not its containing directory, determines existence; a
	 * manually removed store may be recreated beside unrelated directory contents.
	 */
	if (create && present)
		return error(TS_DUPLICATE, "tagspace exists: " + name);
	if (!create && !present)
		return error(TS_NOT_FOUND, "no such tagspace: " + name);
	if (!create && !fs::is_regular_file(database, ec))
		return error(TS_ERR, "invalid tagspace storage: " + name);

	creation_guard guard;
	if (create) {
		/*
		 * Existing sidecars may belong to a damaged or separately opened store;
		 * never claim them as files owned by this creation attempt.
		 */
		for (const char* suffix : {"-journal", "-wal", "-shm"}) {
			auto status = fs::symlink_status(database.string() + suffix, ec);
			if (ec == std::errc::no_such_file_or_directory)
				ec.clear();
			if (ec)
				return error(TS_INTERNAL_ERR, ec.message());
			if (fs::exists(status))
				return error(TS_ERR, "existing tagspace storage sidecar");
		}
		std::vector<fs::path> missing;
		for (auto path = dir; !path.empty() && !fs::exists(path, ec) && !ec;
			 path = path.parent_path()) {
			missing.push_back(path);
		}
		if (ec)
			return error(TS_INTERNAL_ERR, ec.message());
		for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
			if (fs::create_directory(*it, ec))
				guard.directories.push_back(*it);
			if (ec)
				return error(
					TS_INTERNAL_ERR,
					"create tagspace directory: " + ec.message()
				);
		}
		/*
		 * Exclusive reservation enforces create-only even between competing
		 * creators. Backend open is then forbidden from implicitly creating.
		 */
		const int fd = ::open(
			database.c_str(),
			O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC,
			0600
		);
		if (fd < 0)
			return error(
				errno == EEXIST ? TS_DUPLICATE : TS_INTERNAL_ERR,
				errno == EEXIST
					? "tagspace exists: " + name
					: "create tagspace: " + std::string(std::strerror(errno))
			);
		guard.database = database;
		::close(fd);
	}
	auto rc = init(database.string(), false);
	const bool ready = rc == TAGD_OK;
	/*
	 * Close the backend before removing the new file on failure.
	 * Keep the new database only after initialization succeeds.
	 */
	if (!ready)
		close();
	else
		guard.committed = true;
	return code(rc);
}

} // namespace tagdb
