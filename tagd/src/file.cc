#include <algorithm> // for transform
#include <cctype>    // for tolower
#include <string>    // for npos

#include "tagd/file.h"

namespace tagd {

// return position of beginning of extension or std::string::npos for not found
size_t file::ext_pos(const std::string& path) {
	/* minimum lenth to have an extention = size 3
	 *
	 *   path    return
	 *   -----   ------
	 *   ""      npos
	 *   "."     npos
	 *   ".a"    npos    .a is a filename
	 *   "a."    npos     ext empty
	 *   "a.e"   2
	 *   "a.e."  npos     ext empty
	 */
	if (path.size() <= 2)
		return std::string::npos;

	size_t pos = path.find_last_of('.');
	if (pos == std::string::npos)
		return std::string::npos;

	// if pos greater than min length and not trailing '.'
	if (pos >= 2 && pos != path.size()-1) {
		// hidden file not an ext: /path/to/.file
		if (path[pos-1] == '/')
			return std::string::npos;
		else
			return pos + 1;
	}

	return std::string::npos;  // not found
}

// starting position in path with leading dir shifted off (to the left)
size_t file::dir_shift_pos(const std::string& path) {
	/*\
	|*|  examples
	|*|  --------
	|*|
	|*|  /path/to/file
	|*|        ^ pos
	|*|
	|*|  path/to/file
	|*|       ^ pos
	|*|
	|*|  ./path/to/file
	|*|    ^ pos
	|*|
	|*|  ../path/to/file
	|*|     ^ pos
	|*|
	|*|  ./../path/to/file
	|*|    ^ pos
	|*|
	|*|  ../../path/to/file
	|*|     ^ pos
	|*|
	|*|  file
	|*|  ^ pos
	|*|
	|*|  ""
	|*|  npos
	|*|
	|*|  /
	|*|  ^ npos
	|*|
	|*|  //
	|*|  ^ npos
	|*|
	|*|  //file
	|*|    ^ pos
	\*/

	if (path.empty())
		return std::string::npos;

	size_t pos = 0;

	// eat leading slash
	if (path[pos] == '/') {
		pos++;
		// all slashes, no shift
		if (pos >= path.size())
			return std::string::npos;
	}

   	pos = path.find_first_of('/', pos);
	while (pos != std::string::npos) {
		if ((pos+1) >= path.size())  // path all '/'
			return std::string::npos;

		if (path[pos+1] == '/') // next char also '/'
			pos = path.find_first_of('/', pos);
		else
			return pos + 1;
	}

	// no leading '/', pos in beginning of path
	return 0;
}

// return media type given file extension or null pointer if unknown
const char * file::ext_media_type(std::string ext) {
	if (ext.empty())
		return nullptr;

	// lowercase string
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c){ return std::tolower(c); });	

	// file extensions borrowed from "mime-support" package: /etc/mime.types
	size_t i = 0;
	switch(ext[i]) {
		case '7':
			if (ext == "7z")  return "application/x-7z-compressed";
			break;
		case 'a':
			if (ext == "aif")  return "audio/x-aiff";
			if (ext == "asc")  return "text/plain";
			break;
		case 'c':
			if (ext == "css")  return "text/css";
			if (ext == "csv")  return "text/csv";
			break;
		case 'd':
			if (ext == "doc")  return "application/msword";
			break;
		case 'f':
			if (ext == "flac")  return "audio/flac";
			if (ext == "flv")  return "video/x-flv";
			break;
		case 'g':
			if (ext == "gif")  return "image/gif";
			if (ext == "gz")  return "application/gzip";
			break;
		case 'h':
			if (ext == "html")  return "text/html";
			if (ext == "htm")  return  "text/html";
			break;
		case 'i':
			if (ext == "ico")  return "image/vnd.microsoft.icon";
			break;
		case 'j':
			switch(ext[++i]) {
				case 's':
					if (ext == "js")   return "application/javascript";
					if (ext == "json") return "application/json";
					break;
				case 'p':
					if (ext == "jpeg") return "image/jpeg";
					if (ext == "jpg")  return "image/jpeg";
					break;
			}
			break;
		case 'l':
			if (ext == "latex")  return "application/x-latex";
			break;
		case 'm':
			switch(ext[++i]) {
				case '3':
					if (ext == "m3u")  return "audio/mpegurl";
					break;
				case '4':
					if (ext == "m4a")  return "audio/mpeg";
					break;
				case 'd':
					if (ext == "md")  return "text/markdown";
					break;
				case 'i':
					if (ext == "midi") return "audio/midi";
					break;
				case 'k':
					if (ext == "mkv") return "video/x-matroska";
					break;
				case 'p':
					if (ext == "mpeg")  return "video/mpeg";
					if (ext == "mp3")  return  "audio/mpeg";
					if (ext == "mp4")  return  "video/mp4";
					break;
			}
			break;
		case 'o':
			if (ext == "ogg")  return "audio/ogg";
			break;
		case 'p':
			switch(ext[++i]) {
				case 'd':
					if (ext == "pdf")  return "application/pdf";
					break;
				case 'g':
					if (ext == "pgp")  return "application/pgp-encrypted";
					break;
				case 'n':
					if (ext == "png")  return "image/png";
					break;
				case 'p':
					if (ext == "ppt")  return "application/vnd.ms-powerpoint";
					break;
				case 's':
					if (ext == "ps")   return "application/postscript";
					break;
			}
			break;
		case 'r':
			if (ext == "rar")  return "application/rar";
			if (ext == "rpm")  return "application/x-redhat-package-manager";
			if (ext == "rtf")  return "application/rtf";
			break;
		case 's':
			if (ext == "svg")  return "image/svg+xml";
			if (ext == "swf")  return "application/x-shockwave-flash";
			break;
		case 't':
			switch (ext[++i]) {
				case 'a':
					if (ext == "tar")  return "application/x-tar";
					break;
				case 'g':
					if (ext == "tgz")  return "application/x-gtar-compressed";
					break;
				case 'o':
					if (ext == "torrent")  return "application/x-bittorrent";
					break;
				case 'x':
					if (ext == "txt")  return "text/plain";
					break;
			}
			break;
		case 'w':
			switch (ext[++i]) {
				case 'a':
					if (ext == "wav")  return "audio/x-wav";
					break;
				case 'e':
					if (ext == "webm")  return "video/webm";
					break;
				case 'm':
					if (ext == "wma")  return "audio/x-ms-wma";
					if (ext == "wmv")  return "video/x-ms-wmv";
					break;
			}
			break;
		case 'x':
			switch (ext[++i]) {
				case 'h':
					if (ext == "xhtml")  return "application/xhtml+xml";
					break;
				case 'l':
					if (ext == "xls")  return   "application/vnd.ms-excel";
					break;
				case 'm':
					if (ext == "xml")  return   "application/xml";
					break;
			}
			break;
		case 'z':
			if (ext == "zip")  return "application/zip";
			break;

		default:
			return nullptr;
	}

	return nullptr;
}

}
