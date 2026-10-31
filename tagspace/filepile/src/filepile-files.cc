#include "filepile-files.h"
#include "tagd/sha256.h"
#include "tagd/magic.h"
#include <openssl/rand.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <limits>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace tagd::filepile {
namespace {

tagd::code system_failure(errorable& errors, const std::string& operation, const std::string& path,
	tagd::code status = TS_INTERNAL_ERR) {
	const int saved = errno;
	return errors.error(status, operation + ": " + path + ": " + std::strerror(saved));
}

class descriptor {
	int _fd = -1;
public:
	descriptor() = default;
	explicit descriptor(int fd) : _fd(fd) {}
	~descriptor() {
		if (_fd >= 0)
			::close(_fd);
	}
	descriptor(const descriptor&) = delete;
	descriptor& operator=(const descriptor&) = delete;
	descriptor(descriptor&& other) noexcept : _fd(other._fd) {
		other._fd = -1;
	}
	descriptor& operator=(descriptor&& other) noexcept {
		if (_fd >= 0)
			::close(_fd);
		_fd = other._fd;
		other._fd = -1;
		return *this;
	}
	int get() const {
		return _fd;
	}
};

constexpr int read_flags = O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC;
constexpr int directory_flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;

tagd::code sync(errorable& errors, int fd, const std::string& path) {
	int rc;
	do {
		rc = ::fsync(fd);
	} while (rc < 0 && errno == EINTR);
	if (rc < 0)
		return system_failure(errors, "fsync", path);
	return TAGD_OK;
}

tagd::code directory(errorable& errors, descriptor& result, int parent, const std::string& name, bool create) {
	// Managed components must not redirect content outside the selected root.
	if (create && ::mkdirat(parent, name.c_str(), 0700) < 0 && errno != EEXIST)
		return system_failure(errors, "mkdirat", name);
	int fd = ::openat(parent, name.c_str(), directory_flags);
	if (fd < 0)
		return system_failure(errors, "open directory", name, errno == ENOENT ? TS_NOT_FOUND :
			(errno == ELOOP || errno == ENOTDIR) ? TS_ERR : TS_INTERNAL_ERR);
	result = descriptor(fd);
	return TAGD_OK;
}

tagd::code stat_file(errorable& errors, int fd, struct stat& s, const std::string& path) {
	if (::fstat(fd, &s) < 0)
		return system_failure(errors, "fstat", path);
	if (!S_ISREG(s.st_mode) || s.st_size < 0 ||
		static_cast<uintmax_t>(s.st_size) > std::numeric_limits<uint64_t>::max())
		return errors.error(TS_ERR, "filepile requires a regular file with a representable size: " + path);
	return TAGD_OK;
}

bool same_snapshot(const struct stat& a, const struct stat& b) {
	return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size &&
		a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
		a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

tagd::code read_bytes(errorable& errors, ssize_t& n, int fd, char* bytes, size_t size, const std::string& path) {
	do {
		n = ::read(fd, bytes, size);
	} while (n < 0 && errno == EINTR);
	if (n < 0)
		return system_failure(errors, "read", path);
	return TAGD_OK;
}

tagd::code write_bytes(errorable& errors, int fd, const char* bytes, size_t size, const std::string& path) {
	while (size) {
		const auto n = ::write(fd, bytes, size);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			return system_failure(errors, "write", path);
		if (!n)
			return errors.error(TS_INTERNAL_ERR, "write made no progress: " + path);
		bytes += n;
		size -= n;
	}
	return TAGD_OK;
}

tagd::code hash_check(errorable& errors, tagd::code rc) {
	if (rc != TAGD_OK)
		return errors.error(rc, "filepile SHA-256 operation failed");
	return TAGD_OK;
}

int hex(unsigned char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

bool valid_unicode(const std::string& bytes) {
	// The rank UTF-8 codec permits ordinals outside Unicode; filenames must not.
	for (size_t i = 0; i < bytes.size();) {
		uint32_t c = static_cast<unsigned char>(bytes[i++]);
		unsigned count = 0;
		uint32_t minimum = 0;
		if (c >= 0xc2 && c <= 0xdf) {
			count = 1;
			c &= 0x1f;
			minimum = 0x80;
		} else if (c >= 0xe0 && c <= 0xef) {
			count = 2;
			c &= 0x0f;
			minimum = 0x800;
		} else if (c >= 0xf0 && c <= 0xf4) {
			count = 3;
			c &= 0x07;
			minimum = 0x10000;
		} else if (c >= 0x80) {
			return false;
		}
		if (count > bytes.size() - i) return false;
		while (count--) {
			const auto next = static_cast<unsigned char>(bytes[i++]);
			if ((next & 0xc0) != 0x80) return false;
			c = (c << 6) | (next & 0x3f);
		}
		if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
	}
	return true;
}

} // namespace

tagd::code canonical_source(errorable& errors, url& result, std::string& path, const url& input) {
	if (input.code() != TAGD_OK)
		return errors.error(input.code(), "invalid source URL: " + input.id());
	std::string raw = input.id();
	const auto colon = raw.find(':');
	std::string scheme = raw.substr(0, colon);
	for (char& c : scheme) {
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	// TODO(filepile-http): Add httagd-client acquisition behind this interface.
	if (scheme != "file")
		return errors.error(TS_NOT_IMPLEMENTED, "filepile source scheme is not implemented: " + scheme);
	const auto invalid = [&]() {
		return errors.error(TS_ERR, "invalid local file URL for filepile: " + raw);
	};
	if (raw.substr(colon + 1, 2) != "//") return invalid();
	for (unsigned char c : raw) {
		if (c <= 0x20 || c >= 0x7f || c == '\\' || c == '?' || c == '#') return invalid();
	}
	const auto slash = raw.find('/', colon + 3);
	if (slash == std::string::npos) return invalid();
	std::string host = raw.substr(colon + 3, slash - colon - 3);
	for (char& c : host) {
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	if (!host.empty() && host != "localhost") return invalid();

	path.clear();
	for (size_t start = slash + 1; start <= raw.size();) {
		auto end = raw.find('/', start);
		if (end == std::string::npos) end = raw.size();
		if (end == start) return invalid();
		std::string segment;
		for (size_t i = start; i < end; ++i) {
			unsigned char c = raw[i];
			if (c == '%') {
				if (end - i < 3 || hex(raw[i + 1]) < 0 || hex(raw[i + 2]) < 0) return invalid();
				c = hex(raw[i + 1]) * 16 + hex(raw[i + 2]);
				i += 2;
			}
			if (c < 0x20 || c == 0x7f || c == '/' || c == '\\') return invalid();
			segment += c;
		}
		if (segment == "." || segment == ".." || !valid_unicode(segment)) return invalid();
		if (start == slash + 1 && segment.size() >= 2 &&
			((segment[0] >= 'A' && segment[0] <= 'Z') ||
			 (segment[0] >= 'a' && segment[0] <= 'z')) &&
			(segment[1] == ':' || segment[1] == '|')) return invalid();
		path += '/' + segment;
		start = end + 1;
	}
	result = url(file_url(path));
	if (result.code() != TAGD_OK)
		return errors.error(result.code(), "canonical source URL exceeds URL limits: " + raw);
	return TAGD_OK;
}

class files::impl {
public:
	std::string path, root, stage_name;
	descriptor source, home, pile, staging, stage;
	bool initialized = false;

	tagd::code cleanup(errorable& errors) {
		if (!stage_name.empty()) {
			if (::unlinkat(staging.get(), stage_name.c_str(), 0) < 0)
				return system_failure(errors, "unlink staging", root + "/.staging/" + stage_name);
			stage_name.clear();
		}
		return TAGD_OK;
	}
	~impl() {
		// Explicit cleanup reports errors; destruction still attempts owned-name removal.
		if (!stage_name.empty())
			::unlinkat(staging.get(), stage_name.c_str(), 0);
	}
};

files::files() : _impl(std::make_unique<impl>()) {}

tagd::code files::init(const std::string& canonical_home, const url& input) {
	if (_impl->source.get() >= 0)
		return error(TS_MISUSE, "filepile files already initialized");

	auto rc = canonical_source(*this, source, _impl->path, input);
	if (rc != TAGD_OK)
		return rc;
	int fd = ::open(_impl->path.c_str(), read_flags);
	if (fd < 0)
		return system_failure(*this, "open source", _impl->path, errno == ENOENT ? TS_NOT_FOUND : TS_INTERNAL_ERR);
	_impl->source = descriptor(fd);
	rc = stat_file(*this, fd, initial, _impl->path);
	if (rc != TAGD_OK)
		return rc;

	// Tagspace fixes the home at configuration; resolving it again could select a different pile.
	_impl->root = (std::filesystem::path(canonical_home) / "filepile").string();
	rc = directory(*this, _impl->home, AT_FDCWD, canonical_home, false);
	if (rc != TAGD_OK)
		return rc;
	rc = directory(*this, _impl->pile, _impl->home.get(), "filepile", true);
	if (rc != TAGD_OK)
		return rc;
	rc = directory(*this, _impl->staging, _impl->pile.get(), ".staging", true);
	if (rc != TAGD_OK)
		return rc;
	_impl->initialized = true;
	return code(TAGD_OK);
}

files::~files() = default;

tagd::code files::capture() {
	if (!_impl->initialized)
		return error(TS_MISUSE, "filepile files require successful initialization");
	tagd::code rc;

	if (_impl->stage.get() >= 0)
		return error(TS_MISUSE, "filepile source has already been captured");
	// TODO(tagd-io): Staging, publication, and cleanup are single-process operations.
	std::array<unsigned char, 16> random{};
	if (RAND_bytes(random.data(), random.size()) != 1)
		return error(TS_INTERNAL_ERR, "cannot generate filepile staging name");
	constexpr char hex_digits[] = "0123456789abcdef";
	std::string name = "store-";
	for (auto c : random) {
		name += hex_digits[c >> 4];
		name += hex_digits[c & 15];
	}
	int fd = ::openat(_impl->staging.get(), name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		return system_failure(*this, "create staging", _impl->root + "/.staging/" + name);
	_impl->stage = descriptor(fd);
	_impl->stage_name = name;

	sha256 hash;
	std::array<char, filepile_sample_limit> buffer{};
	uint64_t remaining = initial.st_size;
	while (remaining) {
		ssize_t n;
		rc = read_bytes(*this, n, _impl->source.get(), buffer.data(),
			std::min<uint64_t>(remaining, buffer.size()), _impl->path);
		if (rc != TAGD_OK)
			return rc;
		if (!n)
			return error(TS_ERR, "source shrank during filepile storage: " + source.id());
		rc = write_bytes(*this, fd, buffer.data(), n, _impl->root + "/.staging/" + name);
		if (rc != TAGD_OK)
			return rc;
		rc = hash_check(*this, hash.update({buffer.data(), static_cast<size_t>(n)}));
		if (rc != TAGD_OK)
			return rc;
		sample.append(buffer.data(), std::min<size_t>(n, filepile_sample_limit - sample.size()));
		remaining -= n;
	}
	ssize_t extra;
	rc = read_bytes(*this, extra, _impl->source.get(), buffer.data(), 1, _impl->path);
	if (rc != TAGD_OK)
		return rc;
	if (extra)
		return error(TS_ERR, "source grew during filepile storage: " + source.id());
	rc = hash_check(*this, hash.finish(digest));
	if (rc != TAGD_OK)
		return rc;
	rc = validate_source();
	if (rc != TAGD_OK)
		return rc;
	return TAGD_OK;
}

tagd::code files::validate_source() {
	if (!_impl->initialized)
		return error(TS_MISUSE, "filepile files require successful initialization");
	tagd::code rc;

	struct stat after{};
	rc = stat_file(*this, _impl->source.get(), after, _impl->path);
	if (rc != TAGD_OK)
		return rc;
	if (!same_snapshot(initial, after))
		return error(TS_ERR, "source changed during filepile storage: " + source.id());
	if (::stat(_impl->path.c_str(), &after) < 0)
		return system_failure(*this, "stat source path", _impl->path, errno == ENOENT ? TS_NOT_FOUND : TS_INTERNAL_ERR);
	if (initial.st_dev != after.st_dev || initial.st_ino != after.st_ino)
		return error(TS_ERR, "source path changed during filepile storage: " + source.id());
	return TAGD_OK;
}

std::string files::destination(const std::string& name) const {
	return _impl->root + '/' + name.substr(0, 2) + '/' + name.substr(2, 2) + '/' + name;
}

tagd::code files::discover(std::string& match) {
	if (!_impl->initialized)
		return error(TS_MISUSE, "filepile files require successful initialization");
	if (digest.size() != 64)
		return error(TS_MISUSE, "filepile discovery requires captured content");
	match.clear();
	descriptor first, second;
	// A missing shard means no object, not a failed store; suppress expected diagnostics.
	errorable lookup;
	auto rc = directory(lookup, first, _impl->pile.get(), digest.substr(0, 2), false);
	if (rc == TAGD_OK)
		rc = directory(lookup, second, first.get(), digest.substr(2, 2), false);
	if (rc == TS_NOT_FOUND)
		return TAGD_OK;
	if (rc != TAGD_OK) {
		copy_errors(lookup);
		return code(rc);
	}
	// fdopendir consumes its descriptor; keep the checked directory separately owned.
	const int scan_fd = ::openat(second.get(), ".", directory_flags);
	if (scan_fd < 0)
		return system_failure(*this, "scan filepile shard", destination(digest));
	DIR* raw = ::fdopendir(scan_fd);
	if (!raw) {
		const auto rc = system_failure(*this, "scan filepile shard", destination(digest));
		::close(scan_fd);
		return rc;
	}
	const auto close_directory = [](DIR* p) { ::closedir(p); };
	std::unique_ptr<DIR, decltype(close_directory)> entries(raw, close_directory);
	for (;;) {
		errno = 0;
		auto* entry = ::readdir(entries.get());
		if (!entry) {
			if (errno) return system_failure(*this, "readdir", destination(digest));
			break;
		}
		std::string name = entry->d_name;
		if (!name.starts_with(digest + '.') || name.size() <= digest.size() + 1)
			continue;
		if (!match.empty())
			return error(TS_AMBIGUOUS, "multiple filepile objects: " + destination(match) + ", " + destination(name));
		match = name;
	}
	return TAGD_OK;
}

tagd::code files::verify(const std::string& name, const std::string& hash, bool durable) {
	if (!_impl->initialized)
		return error(TS_MISUSE, "filepile files require successful initialization");
	tagd::code rc;

	// TODO(filepile-integrity): Full verification remains required before every reuse.
	const auto path = destination(name);
	descriptor first;
	rc = directory(*this, first, _impl->pile.get(), name.substr(0, 2), false);
	if (rc != TAGD_OK)
		return rc;
	descriptor second;
	rc = directory(*this, second, first.get(), name.substr(2, 2), false);
	if (rc != TAGD_OK)
		return rc;
	const int fd = ::openat(second.get(), name.c_str(), read_flags | O_NOFOLLOW);
	if (fd < 0)
		return system_failure(*this, "open stored file", path, errno == ENOENT ? TS_NOT_FOUND :
			errno == ELOOP ? TS_ERR : TS_INTERNAL_ERR);
	descriptor content(fd);
	rc = stat_file(*this, fd, stored, path);
	if (rc != TAGD_OK)
		return rc;
	sha256 actual;
	std::array<char, filepile_sample_limit> bytes{};
	uint64_t remaining = stored.st_size;
	while (remaining) {
		ssize_t n;
		rc = read_bytes(*this, n, fd, bytes.data(), std::min<uint64_t>(remaining, bytes.size()), path);
		if (rc != TAGD_OK)
			return rc;
		if (!n)
			return error(TS_ERR, "stored file changed during verification: " + path);
		rc = hash_check(*this, actual.update({bytes.data(), static_cast<size_t>(n)}));
		if (rc != TAGD_OK)
			return rc;
		remaining -= n;
	}
	struct stat after{};
	rc = stat_file(*this, fd, after, path);
	if (rc != TAGD_OK)
		return rc;
	std::string value;
	rc = hash_check(*this, actual.finish(value));
	if (rc != TAGD_OK)
		return rc;
	ssize_t extra;
	rc = read_bytes(*this, extra, fd, bytes.data(), 1, path);
	if (rc != TAGD_OK)
		return rc;
	if (extra || !same_snapshot(stored, after) || value != hash)
		return error(TS_ERR, "stored file failed SHA-256 verification: " + path);
	if (durable) {
		// Reused orphans may come from a failed sync; hashing alone promises no durability.
		rc = sync(*this, fd, path);
		if (rc != TAGD_OK)
			return rc;
		rc = sync(*this, second.get(), path);
		if (rc != TAGD_OK)
			return rc;
		rc = sync(*this, first.get(), path);
		if (rc != TAGD_OK)
			return rc;
		rc = sync(*this, _impl->pile.get(), _impl->root);
		if (rc != TAGD_OK)
			return rc;
		rc = sync(*this, _impl->home.get(), _impl->root);
		if (rc != TAGD_OK)
			return rc;
	}
	return TAGD_OK;
}

tagd::code files::publish(const std::string& name) {
	if (!_impl->initialized)
		return error(TS_MISUSE, "filepile files require successful initialization");
	tagd::code rc;

	// TODO(tagd-io): No-overwrite publication does not coordinate overlapping stores.
	rc = validate_source();
	if (rc != TAGD_OK)
		return rc;
	descriptor first;
	rc = directory(*this, first, _impl->pile.get(), digest.substr(0, 2), true);
	if (rc != TAGD_OK)
		return rc;
	descriptor second;
	rc = directory(*this, second, first.get(), digest.substr(2, 2), true);
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, _impl->stage.get(), _impl->root + "/.staging/" + _impl->stage_name);
	if (rc != TAGD_OK)
		return rc;
	// Hard-link publication cannot replace an existing content address.
	if (::linkat(_impl->staging.get(), _impl->stage_name.c_str(), second.get(), name.c_str(), 0) < 0) {
		if (errno != EEXIST)
			return system_failure(*this, "linkat", destination(name));
		rc = verify(name, digest, true);
		if (rc != TAGD_OK)
			return rc;
	} else {
		rc = stat_file(*this, _impl->stage.get(), stored, destination(name));
		if (rc != TAGD_OK)
			return rc;
	}
	rc = cleanup();
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, _impl->staging.get(), _impl->root + "/.staging");
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, second.get(), destination(name));
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, first.get(), destination(name));
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, _impl->pile.get(), _impl->root);
	if (rc != TAGD_OK)
		return rc;
	rc = sync(*this, _impl->home.get(), _impl->root);
	if (rc != TAGD_OK)
		return rc;
	return TAGD_OK;
}

tagd::code files::cleanup() {
	// TODO(cooperative-cancellation): Route future cancellation through owned staging cleanup.
	return _impl->cleanup(*this);
}

tagd::code stored_address(errorable& errors, std::string& name,
	const std::string& canonical_home, const url& stored) {
	std::string path;
	url canonical;
	const auto rc = canonical_source(errors, canonical, path, stored);
	if (rc != TAGD_OK)
		return rc;
	name = std::filesystem::path(path).filename().string();
	if (name.size() <= digest_length + 1 || name[digest_length] != '.' ||
		name.substr(0, digest_length).find_first_not_of("0123456789abcdef") != std::string::npos)
		return errors.error(TS_ERR, "invalid stored filepile address: " + stored.id());

	// Validate the digest before slicing its directory prefixes, including for malformed input.
	const auto expected = std::filesystem::path(canonical_home) / "filepile"
		/ name.substr(0, 2) / name.substr(2, 2) / name;
	if (path != expected)
		return errors.error(TS_ERR, "stored filepile address is outside the selected home: " + stored.id());
	return TAGD_OK;
}

// Cleanup uses directory descriptors so a managed component cannot redirect unlink outside filepile.
tagd::code remove_content(errorable& errors, cleanup_result& result,
	const std::string& canonical_home, const url& stored) {
	std::string path;
	url canonical;
	auto rc = canonical_source(errors, canonical, path, stored);
	if (rc != TAGD_OK)
		return rc;
	std::string name;
	rc = stored_address(errors, name, canonical_home, stored);
	if (rc != TAGD_OK)
		return rc;

	auto failure = [&]() {
		if (errno == ENOENT) {
			result.outcome = cleanup_outcome::already_missing;
			return TAGD_OK;
		}
		return system_failure(errors, "remove stored content", path, TS_NOT_AVAILABLE);
	};
	descriptor parent(::open(canonical_home.c_str(), directory_flags));
	if (parent.get() < 0)
		return failure();
	for (const auto& part : {std::string("filepile"), name.substr(0, 2), name.substr(2, 2)}) {
		descriptor child(::openat(parent.get(), part.c_str(), directory_flags));
		if (child.get() < 0)
			return failure();
		parent = std::move(child);
	}
	struct stat status{};
	if (::fstatat(parent.get(), name.c_str(), &status, AT_SYMLINK_NOFOLLOW) < 0)
		return failure();
	if (!S_ISREG(status.st_mode))
		return errors.error(TS_NOT_AVAILABLE, "stored content is not a regular file: " + path);
	if (::unlinkat(parent.get(), name.c_str(), 0) < 0)
		return failure();

	// A durability failure after unlink must not claim that the bytes remain present.
	result.outcome = cleanup_outcome::removed;
	if (::fsync(parent.get()) < 0)
		return system_failure(errors, "sync content deletion", path, TS_NOT_AVAILABLE);
	return TAGD_OK;
}

} // namespace tagd::filepile
