#pragma once

#include "tagd.h"
#include <memory>
#include <string_view>

namespace tagd {

class sha256 : public errorable {
	class impl;
	std::unique_ptr<impl> _impl;
public:
	// Digest state has a single owner and is finalized exactly once.
	sha256();
	~sha256();
	sha256(const sha256&) = delete;
	sha256& operator=(const sha256&) = delete;
	sha256(sha256&&) = delete;
	sha256& operator=(sha256&&) = delete;
	[[nodiscard]] tagd::code update(std::string_view);
	[[nodiscard]] tagd::code finish(std::string&);
};

}
