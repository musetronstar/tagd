#pragma once

#include "tagd/url.h"
#include <sys/stat.h>
#include <memory>

namespace tagd::filepile {

[[nodiscard]] tagd::code canonical_source(errorable&, url&, std::string& path, const url&);
std::string path_url(const std::string&);

// One store owns these descriptors and staging name; init failure still permits cleanup.
class files : public errorable {
	class impl;
	std::unique_ptr<impl> _impl;
public:
	url source;
	std::string digest;
	std::string sample;
	struct stat initial{};
	struct stat stored{};

	files();
	[[nodiscard]] tagd::code init(const std::string& home, const url&);
	~files();
	files(const files&) = delete;
	files& operator=(const files&) = delete;
	[[nodiscard]] tagd::code capture();
	[[nodiscard]] tagd::code validate_source();
	[[nodiscard]] tagd::code discover(std::string&);
	std::string destination(const std::string& name) const;
	[[nodiscard]] tagd::code publish(const std::string& name);
	[[nodiscard]] tagd::code verify(const std::string& name, const std::string& hash, bool durable = false);
	[[nodiscard]] tagd::code cleanup();
};

} // namespace tagd::filepile
