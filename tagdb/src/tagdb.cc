#include <cassert>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "tagdb.h"

static tagd::logger *TAGDB_LOGGER = nullptr;

void TAGDB_SET_LOGGER(tagd::logger *log) {
	TAGDB_LOGGER = log;
}

bool TAGDB_LOG_ENABLED(const std::string& role, tagd::log_level lvl) {
	if (TAGDB_LOGGER == nullptr)
		return false;

	return static_cast<int>(lvl) <= static_cast<int>(TAGDB_LOGGER->level(role));
}

void TAGDB_LOG(const std::string& role, tagd::log_level lvl, const std::string& msg) {
	if (TAGDB_LOGGER == nullptr)
		return;

	if (role == HARD_TAG_ROLE_TAGDB && lvl == tagd::log_level::DEBUG) {
		std::stringstream ss(msg);
		std::string line;

		while (std::getline(ss, line)) {
			TAGDB_LOGGER->log(role, lvl, std::string("-- ").append(line));
		}

		if (!msg.empty() && msg.back() == '\n')
			TAGDB_LOGGER->log(role, lvl, "-- ");

		return;
	}

	TAGDB_LOGGER->log(role, lvl, msg);
}

void TAGDB_LOG_EVENT(tagd::session *ssn, tagd::log_level lvl, const std::string& event_type_tag) {
	if (TAGDB_LOGGER == nullptr)
		return;

	if (!TAGDB_LOG_ENABLED(HARD_TAG_ROLE_TAGDB, lvl))
		return;

	tagd::session fallback_ssn;
	tagd::session& event_ssn = ssn ? *ssn : fallback_ssn;
	tagd::event ev(event_ssn, "tagdb", event_type_tag);
	TAGDB_LOGGER->log(HARD_TAG_ROLE_TAGDB, lvl, ev);
}

namespace tagdb {

tagd::code session::push_context(const tagd::id_type& id) {
	if (id.empty())
		return this->error(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_EMPTY));
	else if (id == HARD_TAG_ENTITY)
		return this->error(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_ENTITY));

	tagd::abstract_tag t;
	if (_tdb->exists(id)) {
		_context.push_back(id);
		return tagd::TAGD_OK;
	}

	return this->ferror(tagd::TS_INTERNAL_ERR, "push_context failed: %s", id.c_str());
}

const tagd::id_vec& session::context() const { return _context; }

// though returning a tagd code is irrelevent here, it is useful
// for derived classes to return a code
tagd::code session::pop_context() { _context.pop_back(); return tagd::TAGD_OK; }
tagd::code session::clear_context() { _context.clear(); return tagd::TAGD_OK; }

void session::print_context() {
	size_t i = 0, sz = this->context().size();
	for (auto id : this->context()) {
		TAGD_COUT << id;
		if (++i != sz)
			TAGD_COUT << ", ";
		else
			TAGD_COUT << std::endl;
	}
}

std::string util::user_db() {
	struct passwd *pw = getpwuid(getuid());
	std::string str(pw->pw_dir);  // home dir
	if (str.empty()) return str;

	str.append("/.tagdb.db");
	return str;
}

} // namespace tagdb
