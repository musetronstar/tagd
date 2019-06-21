#include <cassert>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "tagdb.h"

namespace tagdb {

tagd::code tagdb::push_context(const tagd::id_type& id) {
	if (id.empty())
		return this->error(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_EMPTY));
	else if (id == HARD_TAG_ENTITY)
		return this->error(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_ENTITY));

	tagd::abstract_tag t;
	if (this->exists(id)) {
		_context.push_back(id);
		return tagd::TAGD_OK;
	}

	return this->ferror(tagd::TS_INTERNAL_ERR, "push_context failed: %s", id.c_str());
}

const tagd::id_vec& tagdb::context() const { return _context; }

// though returning a tagd code is irrelevent here, it is useful
// for derived classes to return a code
tagd::code tagdb::pop_context() { _context.pop_back(); return tagd::TAGD_OK; }
tagd::code tagdb::clear_context() { _context.clear(); return tagd::TAGD_OK; }

std::string util::user_db() {
	struct passwd *pw = getpwuid(getuid());
	std::string str(pw->pw_dir);  // home dir
	if (str.empty()) return str;

	str.append("/.tagdb.db");
	return str;
}

} // namespace tagdb
