#include "tagspace.h"
#include "tagdb.h"
#include <cstdlib>
#include <filesystem>
#include <pwd.h>
#include <unistd.h>

static tagd::logger* tagspace_logger = nullptr;

void TAGSPACE_SET_LOGGER(tagd::logger* logger) {
	tagspace_logger = logger;
	TAGDB_SET_LOGGER(logger);
}

bool TAGSPACE_LOG_ENABLED(tagd::log_level level) {
	return tagspace_logger && level <= tagspace_logger->level(std::string(tagd::HARD_TAG_ROLE_TAGSPACE));
}

void TAGSPACE_LOG(tagd::log_level level, const std::string& text) {
	if (tagspace_logger)
		tagspace_logger->log(tagd::HARD_TAG_ROLE_TAGSPACE, level, text);
}

namespace tagd {

void tagspace_install_logger_validator() {
	tagdb::hard_tag::install_logger_validator();
}

// The backend defines the storage layout; source identity
// checks compare explicit _from sources against this derived path.
std::string tagspaces_directory(const std::string& home) {
	return tagdb::storage_directory(home);
}

std::string tagspace_database_path(const std::string& home, const std::string& id) {
	return tagdb::storage_path(home, id);
}

class stored_tagspace::impl {
public:
	std::unique_ptr<tagdb::tagdb> db;
	explicit impl(tagdb::relation_semantics semantics) : db(tagdb::make_backend(semantics)) {
	}
	bool ready = false;
};

namespace {
namespace fs = std::filesystem;

std::string default_home() {
	if (const char* home = std::getenv("HOME"); home && *home)
		return (fs::path(home) / ".tagd").string();
	if (auto* pw = getpwuid(getuid()); pw && pw->pw_dir && *pw->pw_dir)
		return (fs::path(pw->pw_dir) / ".tagd").string();
	return {};
}

}

// Leading-underscore names are reserved for application tagspaces such as _config and _default.
bool valid_tagspace_name(const std::string& name) {
	return tagdb::valid_storage_name(name) && name.front() != '_';
}

std::string default_tagd_home() {
	return default_home();
}

stored_tagspace::stored_tagspace() : stored_tagspace(tagdb::relation_semantics::assertions) {
}
stored_tagspace::stored_tagspace(tagdb::relation_semantics semantics)
	: _impl(std::make_unique<impl>(semantics)) {
}

// The destructor needs the full impl definition to delete the owned backend.
stored_tagspace::~stored_tagspace() = default;

tagdb::tagdb& stored_tagspace::backend() {
	return *_impl->db;
}

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
	_impl->db->attach_errors(*self);
	_impl->db->report_errors = report_errors;
	_impl->db->code(_code);
	return true;
}

tagd::code stored_tagspace::finish(tagd::code rc) const {
	// The error list is shared, but each object has its own return code.
	const_cast<stored_tagspace*>(this)->code(_impl->db->code());
	return rc;
}

void stored_tagspace::filepile_home(std::string home, std::string reason) {
	_home_reason = std::move(reason);
	if (home == _filepile_home)
		return;

	_filepile_home = std::move(home);
	_filepile_canonical_home.clear();
	_filepile_configured_root.clear();
	_filepile_home_error.clear();
	if (_filepile_home.empty())
		return;

	// Cache both spellings at configuration time; identifier-only PUT does no filesystem lookup.
	std::error_code ec;
	const auto configured = fs::absolute(_filepile_home, ec);
	if (!ec) {
		_filepile_configured_root = (configured / "filepile").lexically_normal().string();
		const auto canonical = fs::weakly_canonical(configured, ec);
		if (!ec)
			_filepile_canonical_home = canonical.string();
	}
	if (ec)
		_filepile_home_error = "cannot resolve filepile home: " + ec.message();
}

tagd::code stored_tagspace::initialize_location(const tagdb::storage_location& location) {
	if (_impl->ready)
		return error(TS_MISUSE, "tagspace is already initialized");

	clear_errors();
	_impl->db->attach_errors(*this);
	_impl->db->report_errors = report_errors;
	auto rc = _impl->db->open(location);
	_persistent = location.kind != tagdb::storage_kind::memory;
	_impl->ready = rc == TAGD_OK;
	if (!_impl->ready)
		_impl->db->close();

	return code(rc);
}

tagd::code stored_tagspace::initialize_memory() {
	return initialize_location({tagdb::storage_kind::memory});
}

tagd::code stored_tagspace::initialize_file(const std::string& path, bool create) {
	return initialize_location({tagdb::storage_kind::file, path,
		create ? tagdb::open_mode::create : tagdb::open_mode::existing});
}

tagd::code stored_tagspace::initialize(const std::string& name,
	const std::string& home, bool create) {
	if (_impl->ready)
		return error(TS_MISUSE, "tagspace is already initialized");

	clear_errors();
	_impl->db->attach_errors(*this);
	_impl->db->report_errors = report_errors;
	_persistent = true;
	filepile_home(home);
	auto rc = _impl->db->open_named(name, home,
		create ? tagdb::open_mode::create : tagdb::open_mode::existing);
	_impl->ready = rc == TAGD_OK;
	if (!_impl->ready)
		_impl->db->close();

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

tagd::code stored_tagspace::lookup_rank(rank& r, id_view id) const {
	if (!prepare())
		return code();
	return finish(_impl->db->lookup_rank(r, id));
}

tagd::code stored_tagspace::order_predicates(predicate_set& values) const {
	if (!prepare())
		return code();
	return finish(_impl->db->order_predicates(values));
}

tagd::code stored_tagspace::resolve_referents(abstract_tag& result,
	const abstract_tag& input, tagspace_session* ssn) {
	if (!prepare())
		return code();
	return finish(_impl->db->resolve_referents(result, input, ssn));
}

tagd::code stored_tagspace::get(abstract_tag& t, id_view id, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db->get(t, id, ssn, flags));
}

tagd::code stored_tagspace::get(url& t, id_view id, tagspace_session* ssn, flags_t flags) {
	// Keep the URL overload so the backend reads the URL fields too.
	if (!prepare())
		return code();
	return finish(_impl->db->get(t, id, ssn, flags));
}

tagd::code stored_tagspace::put(const abstract_tag& t, tagspace_session* ssn, flags_t flags) {
	const auto rc = guard_mutation(t, false, ssn, flags);
	if (rc != TAGD_OK) {
		if (ssn)
			ssn->code(rc);
		return code(rc);
	}
	return finish(_impl->db->put(t, ssn, flags));
}

tagd::code stored_tagspace::put(const url& t, tagspace_session* ssn, flags_t flags) {
	return put(static_cast<const abstract_tag&>(t), ssn, flags);
}

tagd::code stored_tagspace::put(const referent& t, tagspace_session* ssn, flags_t flags) {
	return put(static_cast<const abstract_tag&>(t), ssn, flags);
}

tagd::code stored_tagspace::del(const abstract_tag& t, tagspace_session* ssn, flags_t flags) {
	const auto rc = guard_mutation(t, true, ssn, flags);
	if (rc != TAGD_OK) {
		if (ssn)
			ssn->code(rc);
		return code(rc);
	}
	return finish(_impl->db->del(t, ssn, flags));
}

tagd::code stored_tagspace::del(const url& t, tagspace_session* ssn, flags_t flags) {
	return del(static_cast<const abstract_tag&>(t), ssn, flags);
}

tagd::code stored_tagspace::del(const referent& t, tagspace_session* ssn, flags_t flags) {
	return del(static_cast<const abstract_tag&>(t), ssn, flags);
}

tagd::code stored_tagspace::query(tag_set& result, const interrogator& q, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return code();
	return finish(_impl->db->query(result, q, ssn, flags));
}

part_of_speech stored_tagspace::pos(id_view id, tagspace_session* ssn, flags_t flags) {
	if (!prepare())
		return POS_UNKNOWN;
	auto result = _impl->db->pos(id, ssn, flags);
	finish(_impl->db->code());
	return result;
}

bool stored_tagspace::exists(id_view id, flags_t flags) const {
	if (!prepare())
		return false;
	bool result = _impl->db->exists(id, flags);
	finish(_impl->db->code());
	return result;
}

tagd::code stored_tagspace::dump(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db->dump(os));
}

tagd::code stored_tagspace::dump_grid(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db->dump_grid(os));
}

tagd::code stored_tagspace::dump_terms(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db->dump_terms(os));
}

tagd::code stored_tagspace::dump_search(std::ostream& os) const {
	if (!prepare())
		return code();
	return finish(_impl->db->dump_search(os));
}

} // namespace tagd
