#pragma once

#include <iostream>
#include <cstring>  // tolower
#include <map>
#include "tagd.h"

typedef std::map<std::string, std::string> url_query_map_t;

namespace tagd {

typedef enum {
	AUTHORITY_UNKNOWN,
	AUTHORITY_HOST,
	AUTHORITY_IPv4,
	AUTHORITY_IPv6
} authority_code;

/*\
|*| Heirarchical Decomposition of a URL (HDURI)
|*|
|*|   0     1         2    3    4      5       6    7    8   (9)
|*| rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
\*/
const size_t HDURI_RPUB = 0;
const size_t HDURI_PRIV = 1;
const size_t HDURI_RSUB = 2;
const size_t HDURI_PATH = 3;
const size_t HDURI_QUERY = 4;
const size_t HDURI_FRAGMENT = 5;
const size_t HDURI_PORT = 6;
const size_t HDURI_USER = 7;
const size_t HDURI_PASS = 8;
// const size_t HDURI_SCHEME = 9; // not needed

inline constexpr std::string_view TAGD_SCHEME{"tagd:"};
const std::string HDURI_SCHEME{"hd:"};
const char HDURI_DELIM = '!';
const std::string HDURI_DELIM_ENCODED{"%21"};
const std::string HDURI_PERCENT_ENCODED{"%25"};
const std::string HDURI_EMPTY_PASSWORD{"%00"};
//   hd:rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
const size_t HDURI_DELIM_COUNT = 9;

// Each URL byte can expand to three bytes; separators and the empty-password marker are extra.
const size_t HDURI_MAX_LEN = HDURI_DELIM_ENCODED.size() * URL_MAX_LEN + HDURI_SCHEME.size() +
	HDURI_DELIM_COUNT + HDURI_EMPTY_PASSWORD.size();

std::string encode_hduri_delim(id_view);
// Decode one HDURI layer only; failure leaves the destination unchanged.
[[nodiscard]] bool decode_hduri_delim(std::string&, id_view);

const size_t HDURI_NUM_ELEMS = 9;

typedef unsigned short url_size_t;

class url : public abstract_tag {
	private:
		/*
			Component lengths select bytes in the canonical URL, not its HDURI encoding.
			A nonzero password offset with zero length preserves an explicit empty
			password; other offsets with zero length do not establish presence.
		*/
		// no need for _scheme_offset (always 0)
		url_size_t _scheme_len = 0;
		url_size_t _user_offset = 0;
		url_size_t _user_len = 0;
		url_size_t _pass_offset = 0;
		url_size_t _pass_len = 0;
		url_size_t _host_offset = 0;
		url_size_t _host_len = 0;
		url_size_t _port_offset = 0;
		url_size_t _port_len = 0;
		url_size_t _path_offset = 0;
		url_size_t _path_len = 0;
		url_size_t _query_offset = 0;
		url_size_t _query_len = 0;
		url_size_t _fragment_offset = 0;
		url_size_t _fragment_len = 0;

	protected:
		tagd::code init(id_view url);
		tagd::code init_hduri(id_view hduri);

	public:
		url() :
			abstract_tag("", HARD_TAG_SUB, HARD_TAG_URL, POS_URL)
		{
			_code = URL_EMPTY;
		}

		url(id_view u) :
			abstract_tag("", HARD_TAG_SUB, HARD_TAG_URL, POS_URL)
		{
			_code = URL_EMPTY;
			this->init(u);
		}

		// Explicit stored-file classification must survive parsing and serialization.
		url(id_view u, id_view classification) : url(u) {
			if (ok() && scheme() == "file" && classification == HARD_TAG_FILE_STORED) {
				_super_object = classification;
			} else {
				_code = TS_ERR;
			}
		}

		const id_string& id() const { return _id; }
		std::string scheme() const { return url_substr(0, _scheme_len); }
		std::string user() const { return url_substr(_user_offset, _user_len); }
		std::string pass() const { return url_substr(_pass_offset, _pass_len); }
		std::string host() const { return url_substr(_host_offset, _host_len); }
		std::string port() const { return url_substr(_port_offset, _port_len); }
		std::string path() const { return url_substr(_path_offset, _path_len); }
		std::string query() const { return url_substr(_query_offset, _query_len); }
		std::string fragment() const { return url_substr(_fragment_offset, _fragment_len); }

		/*
		void debug() {
			TAGD_CERR << '\t' << "_scheme_len: " << _scheme_len << std::endl;
			TAGD_CERR << '\t' << "_user_offset: " << _user_offset << std::endl;
			TAGD_CERR << '\t' << "_user_len: " << _user_len << std::endl;
			TAGD_CERR << '\t' << "_pass_offset: " << _pass_offset << std::endl;
			TAGD_CERR << '\t' << "_pass_len: " << _pass_len << std::endl;
			TAGD_CERR << '\t' << "_host_offset: " << _host_offset << std::endl;
			TAGD_CERR << '\t' << "_host_len: " << _host_len << std::endl;
			TAGD_CERR << '\t' << "_port_offset: " << _port_offset << std::endl;
			TAGD_CERR << '\t' << "_port_len: " << _port_len << std::endl;
			TAGD_CERR << '\t' << "_path_offset: " << _path_offset << std::endl;
			TAGD_CERR << '\t' << "_path_len: " << _path_len << std::endl;
			TAGD_CERR << '\t' << "_query_offset: " << _query_offset << std::endl;
			TAGD_CERR << '\t' << "_query_len: " << _query_len << std::endl;
			TAGD_CERR << '\t' << "_fragment_offset: " << _fragment_offset << std::endl;
			TAGD_CERR << '\t' << "_fragment_len: " << _fragment_len << std::endl;
		}
		*/

		// TODO this will need to be determined during parsing
		authority_code authority_type() const { return AUTHORITY_HOST; }

		std::string hduri() const;
		bool empty() const { return _id.empty(); }

		// we have to distinguish between "no password" and "blank password"
		// empty pass will have non-zero offset and zero length
		bool pass_empty() const {
			return (_pass_offset == 0 && _pass_len == 0);
		}

		// parse keys/vals from query string and populate map - returns num k/v pairs parsed
		static size_t parse_query(url_query_map_t&, const std::string&);
		static bool query_find(const url_query_map_t&, std::string& val, const std::string& key);

		// insert url part relation into predicate_set
		static void insert_url_part_relations(tagd::predicate_set&, const url&);

		static bool looks_like_url(const std::string&);
		static bool looks_like_hduri(const std::string&);

	private:
		std::string url_substr(const url_size_t offset, const url_size_t len) const {
			if (len == 0 || this->empty()) return std::string();
			return _id.substr(offset, len);
		}
};

// initializes a url with an hduri string
// upper case class name because it conflicts with url::hduri()
class HDURI : public url {
	public:
		HDURI(id_view h) : url() {
			this->init_hduri(h);
		}
};


// Checked decoding rejects malformed escapes and NUL before callers use path bytes.
bool uri_decode_checked(std::string&, const std::string&);
std::string uri_decode(const std::string&);
std::string uri_encode(const std::string&);

} // namespace tagd
