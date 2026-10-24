#include "tagd/magic.h"
#include <magic.h>
#include <algorithm>
#include "media-types.gperf.h"

namespace tagd {

bool valid_media_type(std::string_view value) {
	const auto slash = value.find('/');
	if (slash == 0 || slash == value.npos || slash + 1 == value.size()) {
		return false;
	}
	for (size_t i = 0; i < value.size(); ++i) {
		const unsigned char c = value[i];
		if (i == slash || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || std::string_view("!#$%&'*+-.^_`|~").find(c) != value.npos) {
			continue;
		}
		return false;
	}
	return true;
}

std::string canonical_extension(std::string_view media) {
	std::string key(media);
	for (char& c : key) {
		if (c >= 'A' && c <= 'Z') {
			c += 'a' - 'A';
		}
	}
	const auto* entry = media_type_lookup::find(key.c_str(), key.size());
	return entry ? entry->extension : "bin";
}

class magic::impl {
public:
	magic_t cookie = magic_open(MAGIC_MIME);
	bool ready = false;
	~impl() {
		if (cookie) {
			magic_close(cookie);
		}
	}
};

magic::magic(const std::string& database) : _impl(std::make_unique<impl>()) {
	if (!_impl->cookie) {
		error(TS_INTERNAL_ERR, "magic_open failed");
		return;
	}
	if (database.empty() || magic_load(_impl->cookie, database.c_str()) != 0) {
		const char* detail = magic_error(_impl->cookie);
		error(TS_INTERNAL_ERR, detail ? std::string(detail) : "magic database load failed");
		return;
	}
	// Bound recursion and ELF work as well as input bytes; no decompression flags are enabled.
	const std::pair<int, size_t> limits[] = {
		{MAGIC_PARAM_INDIR_MAX, 15}, {MAGIC_PARAM_NAME_MAX, 30},
		{MAGIC_PARAM_REGEX_MAX, 8192}, {MAGIC_PARAM_ELF_NOTES_MAX, 256},
		{MAGIC_PARAM_ELF_PHNUM_MAX, 128}, {MAGIC_PARAM_ELF_SHNUM_MAX, 32768}
	};
	for (const auto& [parameter, limit] : limits) {
		if (magic_setparam(_impl->cookie, parameter, &limit) != 0) {
			error(TS_INTERNAL_ERR, "cannot set libmagic resource limits");
			return;
		}
	}
	_impl->ready = true;
}

magic::~magic() = default;

code magic::detect(magic_result& result, std::string_view sample) {
	result = {};
	if (!_impl->ready) {
		return code();
	}
	clear_errors();
	// Evaluation may supply larger samples; libmagic never acquires additional bytes itself.
	const size_t bound = std::max(size_t{1}, sample.size());
	if (magic_setparam(_impl->cookie, MAGIC_PARAM_BYTES_MAX, &bound) != 0
		|| magic_setparam(_impl->cookie, MAGIC_PARAM_ENCODING_MAX, &bound) != 0) {
		return error(TS_INTERNAL_ERR, "cannot bound libmagic input");
	}
	if (sample.empty()) {
		result.media_type = "application/octet-stream";
		result.empty = true;
		return TAGD_OK;
	}
	const char* description = magic_buffer(_impl->cookie, sample.data(), sample.size());
	if (!description) {
		const char* detail = magic_error(_impl->cookie);
		return error(TS_INTERNAL_ERR, detail ? std::string(detail) : "magic_buffer failed");
	}
	// libmagic owns its result buffer; copy before another library call can invalidate it.
	magic_result detected;
	detected.description = description;
	const auto separator = detected.description.find(';');
	detected.media_type = detected.description.substr(0, separator);
	if (!valid_media_type(detected.media_type)) {
		return error(TS_INTERNAL_ERR, "libmagic returned an invalid media type");
	}
	if (separator != std::string::npos) {
		const auto charset = detected.description.find("charset=", separator);
		if (charset != std::string::npos) {
			detected.charset = detected.description.substr(charset + 8);
		}
	}
	detected.bytes_examined = sample.size();
	detected.identity_known = detected.media_type != "application/octet-stream";
	result = std::move(detected);
	return TAGD_OK;
}

}
