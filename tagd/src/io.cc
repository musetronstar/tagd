#include <string>
#include <sys/stat.h>
#include "tagd/io.h"

namespace tagd {

// concatenates apnd to dir, adding a '/' separator if needed
std::string io::concat_dir(const std::string& dir, const std::string& apnd) {
	if (dir.empty()) return apnd;
	if (apnd.empty()) return dir;

	if (dir[dir.size()-1] == '/') {
		// don't append double '//'
		if (apnd[0] == '/') {
			if (apnd.size() == 1)
				return dir;
			else
				return std::string(dir).append(apnd.substr(1));
		} else {
			return std::string(dir).append(apnd);
		}
	} else {  // no dir trailing '/'
		if (apnd[0] == '/') 
			return std::string(dir).append(apnd);
		else
			return std::string(dir).append("/").append(apnd);
	}
}

bool io::file_exists(const std::string& path) {
  struct stat s_buf;
  return (stat(path.c_str(), &s_buf) == 0);
}

bool io::dir_exists(const std::string& path) {
  struct stat s_buf;
  return (stat(path.c_str(), &s_buf) == 0 && S_ISDIR(s_buf.st_mode));
}

}
