#pragma once

#include <memory>
#include <string>

namespace tagdb {

class tagdb;

enum class storage_kind { memory, file, remote };
enum class open_mode { existing, create };

// The selected backend interprets resource; remote addresses need not be filesystem paths.
struct storage_location {
	storage_kind kind;
	std::string resource = {};
	open_mode mode = open_mode::existing;
};

// The caller owns one independent backend and its cache for the opened tagspace.
std::unique_ptr<tagdb> make_backend();

// Named local storage and file discovery are interpreted by the selected backend.
std::string storage_directory(const std::string& home);
std::string storage_filename(const std::string& name);
std::string storage_path(const std::string& home, const std::string& name);
bool accepts_storage_file(const std::string& path);

// Shared name validation preserves TAGL identifiers used as named storage locations.
bool valid_storage_name(const std::string& name);

} // namespace tagdb
