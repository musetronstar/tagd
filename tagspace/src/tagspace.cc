#include <cassert>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "tagspace.h"

namespace tagspace {

std::string util::user_db() {
	struct passwd *pw = getpwuid(getuid());
	std::string str(pw->pw_dir);
	if (str.empty()) return str;

	str.append("/tagspace.db");
	return str;
}

} // namespace tagspace
