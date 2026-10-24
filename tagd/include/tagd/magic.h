#pragma once

#include "tagd.h"
#include <memory>
#include <string_view>

namespace tagd {

inline constexpr size_t filepile_sample_limit = 64 * 1024;

struct magic_result {
	std::string media_type;
	std::string charset;
	std::string description;
	size_t bytes_examined = 0;
	bool identity_known = false;
	bool empty = false;
};

bool valid_media_type(std::string_view);
std::string canonical_extension(std::string_view);

class magic : public errorable {
	class impl;
	std::unique_ptr<impl> _impl;
public:
	// The explicit system database prevents MAGIC or user files from changing detection.
	explicit magic(const std::string& database = "/usr/share/misc/magic.mgc");
	~magic();
	magic(const magic&) = delete;
	magic& operator=(const magic&) = delete;
	magic(magic&&) = delete;
	magic& operator=(magic&&) = delete;
	[[nodiscard]] tagd::code detect(magic_result&, std::string_view sample);
};

}
