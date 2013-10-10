#include <cassert>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "tagspace.h"

namespace tagspace {

tagd_code tagspace::push_context(const tagd::id_type& id) {
	tagd::abstract_tag t;
	if (this->exists(id) == tagd::TAGD_OK) {
		_context.push_back(id);
		return _code;
	}

	return this->error(tagd::TS_INTERNAL_ERR, "push_context failed: %s");
}

const tagd::id_vec& tagspace::context() const { return _context; }

// though returning a tagd code is irrelevent here, it is useful
// for derived classes to return a code
tagd_code tagspace::pop_context() { _context.pop_back(); return tagd::TAGD_OK; }
tagd_code tagspace::clear_context() { _context.clear(); return tagd::TAGD_OK; }


std::string util::user_db() {
	struct passwd *pw = getpwuid(getuid());
	std::string str(pw->pw_dir);  // home dir
	if (str.empty()) return str;

	str.append("/.tagspace.db");
	return str;
}

} // namespace tagspace
