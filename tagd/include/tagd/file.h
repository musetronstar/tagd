#pragma once

#include "tagd.h"

namespace tagd {

//class file : public abstract_tag {
class file {
	// _id is a canonical file uri
	// file:///path/to/file.ext
	public:
		//file(const std::string& path) :
		//	abstract_tag(path, HARD_TAG_IS_A, HARD_TAG_FILE, POS_FILE) {}

		// file exension position in path
		static size_t ext_pos(const std::string& path);

		/* starting position in path with leading dir shifted off (to the left)
		 * /path/to/file
		 *       ^ pos
		 */
		static size_t dir_shift_pos(const std::string& path);

		/*\
		|*| IANA media type as listed here
		|*| https://www.iana.org/assignments/media-types/media-types.xhtml
		\*/

		// returns media type given file extension
		static const char * ext_media_type(std::string);
};

}
