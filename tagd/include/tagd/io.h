#pragma once

namespace tagd {

// functions for input, output and file operations
struct io {
	// concatenates apnd to dir, adding a '/' separator if needed
	static std::string concat_dir(const std::string& dir, const std::string& apnd);

	// file exists, where dir is also a file
	static bool file_exists(const std::string& path);

	// must be a dir
	static bool dir_exists(const std::string& path);
};

}
