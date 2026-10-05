#include "tagspace.h"
#include "tagdb/sqlite.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>

void TAGSPACE_SET_LOGGER(tagd::logger* logger) {
	TAGDB_SET_LOGGER(logger);
}

namespace tagd {

void tagspace_install_logger_validator() {
	tagdb::hard_tag::install_logger_validator();
}

class stored_tagspace::impl {
public:
	class backend : public tagdb::sqlite {
	public:
		void attach_errors(errorable& owner) {
			// Keep errors still shared by other callers when changing the error list.
			_errors.reset();
			owner.share_errors(*this);
		}
	} db;
	bool ready = false;
};

namespace {
namespace fs = std::filesystem;

namespace unicode {
constexpr uint32_t space = 0x20;
constexpr uint32_t next_line = 0x85;
constexpr uint32_t no_break_space = 0xa0;
constexpr uint32_t ogham_space_mark = 0x1680;
constexpr uint32_t en_quad = 0x2000;
constexpr uint32_t hair_space = 0x200a;
constexpr uint32_t line_separator = 0x2028;
constexpr uint32_t paragraph_separator = 0x2029;
constexpr uint32_t narrow_no_break_space = 0x202f;
constexpr uint32_t medium_mathematical_space = 0x205f;
constexpr uint32_t ideographic_space = 0x3000;
constexpr uint32_t delete_control = 0x7f;
constexpr uint32_t last_c1_control = 0x9f;
constexpr uint32_t first_surrogate = 0xd800;
constexpr uint32_t last_surrogate = 0xdfff;
constexpr uint32_t max_scalar = 0x10ffff;
}

namespace utf8 {
constexpr uint32_t first_non_ascii = 0x80;
constexpr uint32_t two_byte_lead_min = 0xc2;
constexpr uint32_t two_byte_lead_max = 0xdf;
constexpr uint32_t three_byte_lead_min = 0xe0;
constexpr uint32_t three_byte_lead_max = 0xef;
constexpr uint32_t four_byte_lead_min = 0xf0;
constexpr uint32_t four_byte_lead_max = 0xf4;
constexpr uint32_t two_byte_payload_mask = 0x1f;
constexpr uint32_t three_byte_payload_mask = 0x0f;
constexpr uint32_t four_byte_payload_mask = 0x07;
constexpr uint32_t two_byte_min_scalar = 0x80;
constexpr uint32_t three_byte_min_scalar = 0x800;
constexpr uint32_t four_byte_min_scalar = 0x10000;
constexpr uint32_t continuation_marker_mask = 0xc0;
constexpr uint32_t continuation_marker = 0x80;
constexpr uint32_t continuation_payload_mask = 0x3f;
constexpr unsigned continuation_payload_bits = 6;
}

bool unicode_space(uint32_t cp) {
	return cp == unicode::space || cp == unicode::next_line ||
		   cp == unicode::no_break_space || cp == unicode::ogham_space_mark ||
		   (cp >= unicode::en_quad && cp <= unicode::hair_space) ||
		   cp == unicode::line_separator ||
		   cp == unicode::paragraph_separator ||
		   cp == unicode::narrow_no_break_space ||
		   cp == unicode::medium_mathematical_space ||
		   cp == unicode::ideographic_space;
}

bool valid_name(const std::string& name) {
	/*
	 * A tagspace name becomes one directory name and one filename.
	 * Keep its original UTF-8 bytes, but reject path separators, controls,
	 * and leading or trailing whitespace before constructing either path.
	 */
	if (name.empty() || name == "." || name == "..")
		return false;

	for (size_t i = 0; i < name.size();) {
		const size_t start = i;
		uint32_t cp = static_cast<unsigned char>(name[i++]);
		unsigned extra = 0;
		uint32_t minimum = 0;

		if (cp >= utf8::two_byte_lead_min && cp <= utf8::two_byte_lead_max) {
			cp &= utf8::two_byte_payload_mask;
			extra = 1;
			minimum = utf8::two_byte_min_scalar;
		} else if (cp >= utf8::three_byte_lead_min &&
				   cp <= utf8::three_byte_lead_max) {
			cp &= utf8::three_byte_payload_mask;
			extra = 2;
			minimum = utf8::three_byte_min_scalar;
		} else if (cp >= utf8::four_byte_lead_min &&
				   cp <= utf8::four_byte_lead_max) {
			cp &= utf8::four_byte_payload_mask;
			extra = 3;
			minimum = utf8::four_byte_min_scalar;
		} else if (cp >= utf8::first_non_ascii) {
			return false;
		}

		if (extra > name.size() - i)
			return false;
		while (extra--) {
			unsigned char next = name[i++];
			if ((next & utf8::continuation_marker_mask) !=
				utf8::continuation_marker)
				return false;
			cp = (cp << utf8::continuation_payload_bits) |
				 (next & utf8::continuation_payload_mask);
		}

		/* Filenames need valid Unicode; the rank codec permits larger ordinals. */
		if (cp < minimum || cp > unicode::max_scalar ||
			(cp >= unicode::first_surrogate && cp <= unicode::last_surrogate))
			return false;
		if (cp < unicode::space ||
			(cp >= unicode::delete_control && cp <= unicode::last_c1_control) ||
			cp == '/' || cp == '\\')
			return false;
		if ((start == 0 || i == name.size()) && unicode_space(cp))
			return false;
	}
	return true;
}

std::string default_home() {
	if (const char* home = std::getenv("HOME"); home && *home)
		return (fs::path(home) / ".tagd").string();
	if (auto* pw = getpwuid(getuid()); pw && pw->pw_dir && *pw->pw_dir)
		return (fs::path(pw->pw_dir) / ".tagd").string();
	return {};
}

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
		for (auto it = directories.rbegin(); it != directories.rend(); ++it)
			fs::remove(*it, ec);
	}
};
}

stored_tagspace::stored_tagspace() : _impl(std::make_unique<impl>()) {}
// The destructor needs the full impl definition to delete the owned backend.
stored_tagspace::~stored_tagspace() = default;

bool stored_tagspace::prepare() const {
	auto* self = const_cast<stored_tagspace*>(this);
	if (!_impl->ready) {
		self->error(TS_MISUSE, "tagspace is not initialized");
		return false;
	}
	/*
	 * httagd shares a different error list for each request. Connect the backend
	 * to the current list before each call; do not copy errors from old requests.
	 */
	_impl->db.attach_errors(*self);
	_impl->db.report_errors = report_errors;
	_impl->db.code(_code);
	return true;
}

tagd::code stored_tagspace::finish(tagd::code rc) const {
	// The error list is shared, but each object has its own return code.
	const_cast<stored_tagspace*>(this)->code(_impl->db.code());
	return rc;
}

tagd::code stored_tagspace::initialize_memory() {
	if (_impl->ready)
		return error(TS_MISUSE, "tagspace is already initialized");
	clear_errors();
	_impl->db.attach_errors(*this);
	_impl->db.report_errors = report_errors;
	auto rc = _impl->db.init(":memory:");
	_impl->ready = rc == TAGD_OK;
	if (!_impl->ready)
		_impl->db.close();
	return code(rc);
}

tagd::code stored_tagspace::initialize(
	const std::string& name,
	const std::string& home,
	bool create
) {
	if (_impl->ready)
		return error(TS_MISUSE, "tagspace is already initialized");
	clear_errors();
	if (!valid_name(name))
		return error(TS_ERR, "invalid tagspace name");
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
	while (!fs::exists(parent, ec) && !ec && parent.has_relative_path())
		parent = parent.parent_path();
	if (ec)
		return error(TS_INTERNAL_ERR, ec.message());
	long limit = pathconf(parent.c_str(), _PC_NAME_MAX);
	if (limit < 0)
		limit = NAME_MAX;
	if (name.size() + 3 > static_cast<size_t>(limit))
		return error(TS_ERR, "tagspace name exceeds filesystem limit");

	const auto spaces = home_path / "tagspaces";
	const auto dir = spaces / name;
	const auto database = dir / (name + ".db");
	for (const auto& path : {spaces, dir, database}) {
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
			 path = path.parent_path())
			missing.push_back(path);
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
	_impl->db.attach_errors(*this);
	_impl->db.report_errors = report_errors;
	auto rc = _impl->db.init(database.string(), false);
	_impl->ready = rc == TAGD_OK;
	/*
	 * Close the backend before removing the new file on failure.
	 * Keep the new database only after initialization succeeds.
	 */
	if (!_impl->ready)
		_impl->db.close();
	else
		guard.committed = true;
	return code(rc);
}

tagd::code tagspace::persistent::init(const std::string& name, const std::string& home) {
	return initialize(name, home, false);
}

tagd::code tagspace::persistent::create(const std::string& name, const std::string& home) {
	return initialize(name, home, true);
}

tagd::code tagspace::persistent::init(const std::string& name) {
	return init(name, default_home());
}

tagd::code tagspace::persistent::create(const std::string& name) {
	return create(name, default_home());
}

tagd::code stored_tagspace::get(abstract_tag& t, id_view id, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.get(t, id, ssn, flags));
}

tagd::code stored_tagspace::get(url& t, id_view id, tagspace_session* ssn, flags_t flags) {
	// Keep the URL overload so the backend reads the URL fields too.
	if (!prepare())
		return code();
	return finish(_impl->db.get(t, id, ssn, flags));
}

tagd::code stored_tagspace::put(const abstract_tag& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.put(t, ssn, flags));
}

tagd::code stored_tagspace::put(const url& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.put(t, ssn, flags));
}

tagd::code stored_tagspace::put(const referent& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.put(t, ssn, flags));
}

tagd::code stored_tagspace::del(const abstract_tag& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.del(t, ssn, flags));
}

tagd::code stored_tagspace::del(const url& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.del(t, ssn, flags));
}

tagd::code stored_tagspace::del(const referent& t, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.del(t, ssn, flags));
}

tagd::code stored_tagspace::query(tag_set& result, const interrogator& q, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db.query(result, q, ssn, flags));
}

part_of_speech stored_tagspace::pos(id_view id, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return POS_UNKNOWN;
	auto result = _impl->db.pos(id, ssn, flags);
	finish(_impl->db.code());
	return result;
}

bool stored_tagspace::exists(id_view id, flags_t flags) const {
	if (!prepare())
		return false;
	bool result = _impl->db.exists(id, flags);
	finish(_impl->db.code());
	return result;
}

tagd::code stored_tagspace::dump(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db.dump(os));
}

tagd::code stored_tagspace::dump_grid(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db.dump_grid(os));
}

tagd::code stored_tagspace::dump_terms(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db.dump_terms(os));
}

tagd::code stored_tagspace::dump_search(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db.dump_search(os));
}

} // namespace tagd
