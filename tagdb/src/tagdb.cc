#include <cassert>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "tagdb.h"

bool TAGDB_TRACE_ON = false;

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
