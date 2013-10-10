#pragma once

#include <iostream>
#include <cstring>  // tolower
#include "tagd.h"

namespace tagd {

typedef enum {
    AUTHORITY_UNKNOWN,
    AUTHORITY_HOST,
    AUTHORITY_IPv4,
    AUTHORITY_IPv6
} authority_code;

const char HDURI_DELIM = ':';
// HDURI	
//   0     1         2    3    4      5       6    7    8   (9)
// rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
const size_t HDURI_RPUB = 0;
const size_t HDURI_PRIV = 1;
const size_t HDURI_RSUB = 2;
const size_t HDURI_PATH = 3;
const size_t HDURI_QUERY = 4;
const size_t HDURI_FRAGMENT = 5;
const size_t HDURI_PORT = 6;
const size_t HDURI_USER = 7;
const size_t HDURI_PASS = 8;
// const size_t HDURI_SCHEME = 9; // not needed yet

const size_t HDURI_NUM_ELEMS = 9;

typedef unsigned short url_size_t;

class url : public abstract_tag {
    private:
        /* 
            IMPORTANT, a non-zero value for *_len is the sole determiner of whether
            a url component exists.  *_offset may be set during parsing, but will only
            be considered if the *_len is set as well.
        */
        // no need for _scheme_offset (always 0)
        url_size_t _scheme_len;
        url_size_t _user_offset;
        url_size_t _user_len;
        url_size_t _pass_offset;
        url_size_t _pass_len;
        url_size_t _host_offset;
        url_size_t _host_len;
        url_size_t _port_offset;
        url_size_t _port_len;
        url_size_t _path_offset;
        url_size_t _path_len;
        url_size_t _query_offset;
        url_size_t _query_len;
        url_size_t _fragment_offset;
        url_size_t _fragment_len;

		// disable setting _id, becuase it would
		// bypass url initialization.  require init()
        void id(const id_type& A);

    public:
        url() :
        abstract_tag(id_type(), HARD_TAG_URL, POS_URL),
        _scheme_len(0),
        _user_offset(0),
        _user_len(0),
        _pass_offset(0),
        _pass_len(0),
        _host_offset(0),
        _host_len(0),
        _port_offset(0),
        _port_len(0),
        _path_offset(0),
        _path_len(0),
        _query_offset(0),
        _query_len(0),
        _fragment_offset(0),
        _fragment_len(0) {
			_code = URL_EMPTY;
		}

        url(const std::string& u) :
        abstract_tag(id_type(), HARD_TAG_URL, POS_URL),
        _scheme_len(0),
        _user_offset(0),
        _user_len(0),
        _pass_offset(0),
        _pass_len(0),
        _host_offset(0),
        _host_len(0),
        _port_offset(0),
        _port_len(0),
        _path_offset(0),
        _path_len(0),
        _query_offset(0),
        _query_len(0),
        _fragment_offset(0),
		_fragment_len(0) {
			_code = URL_EMPTY;
			this->init(u);
		}
/*
 * TODO
 * before we can allow identity relations other than _url,
 * we must enforce that the super itself is_a web resource
 * For example,
 * http://example.com/wiki _is_a wiki  # ok because wiki is a web resource
 * http://example.com/wiki _is_a dog   # not ok becuase dog is not a web resource

        url(const std::string& u, const id_type& is_a) :
        abstract_tag(id_type(), is_a, POS_URL),
        _scheme_len(0),
        _user_offset(0),
        _user_len(0),
        _pass_offset(0),
        _pass_len(0),
        _host_offset(0),
        _host_len(0),
        _port_offset(0),
        _port_len(0),
        _path_offset(0),
        _path_len(0),
        _query_offset(0),
        _query_len(0),
        _fragment_offset(0),
        _fragment_len(0),
        _code(URL_EMPTY)
        { this->init(u); }
*/
        
		const id_type& id() const { return _id; }
		// id() setter is private

        tagd_code init(const std::string &url);
        tagd_code init_hduri(const std::string &hduri);

        std::string scheme() const { return url_substr(0, _scheme_len); }
        std::string user() const { return url_substr(_user_offset, _user_len); }
        std::string pass() const { return url_substr(_pass_offset, _pass_len); }
        std::string host() const { return url_substr(_host_offset, _host_len); }
        std::string port() const { return url_substr(_port_offset, _port_len); }
        std::string path() const { return url_substr(_path_offset, _path_len); }
        std::string query() const { return url_substr(_query_offset, _query_len); }
        std::string fragment() const { return url_substr(_fragment_offset, _fragment_len); }

		void debug() {
			std::cout << '\t' << "_scheme_len: " << _scheme_len << std::endl;
        	std::cout << '\t' << "_user_offset: " << _user_offset << std::endl;
        	std::cout << '\t' << "_user_len: " << _user_len << std::endl;
        	std::cout << '\t' << "_pass_offset: " << _pass_offset << std::endl;
        	std::cout << '\t' << "_pass_len: " << _pass_len << std::endl;
        	std::cout << '\t' << "_host_offset: " << _host_offset << std::endl;
        	std::cout << '\t' << "_host_len: " << _host_len << std::endl;
        	std::cout << '\t' << "_port_offset: " << _port_offset << std::endl;
        	std::cout << '\t' << "_port_len: " << _port_len << std::endl;
        	std::cout << '\t' << "_path_offset: " << _path_offset << std::endl;
        	std::cout << '\t' << "_path_len: " << _path_len << std::endl;
        	std::cout << '\t' << "_query_offset: " << _query_offset << std::endl;
        	std::cout << '\t' << "_query_len: " << _query_len << std::endl;
        	std::cout << '\t' << "_fragment_offset: " << _fragment_offset << std::endl;
        	std::cout << '\t' << "_fragment_len: " << _fragment_len << std::endl;
		}

        // TODO this will need to be determined during parsing
        inline authority_code authority_type() const { return AUTHORITY_HOST; }

        std::string hduri() const;

        inline void clear() { _id.clear(); _code = URL_EMPTY; }
        inline bool empty() const { return _id.empty(); }

		// insert url part relation into predicate_set
		static void insert_url_part_relations(tagd::predicate_set&, const url&);

    private:
        inline std::string url_substr(const url_size_t offset, const url_size_t len) const {
            if (len == 0 || this->empty()) return std::string();
            return _id.substr(offset, len);
        }

        void host_lower() {
            for (url_size_t i = _host_offset; i < (_host_offset + _host_len); i++)
                _id[i] = tolower(_id[i]);
        }

		// we have to distinguish between "no password" and "blank password"
		// empty pass will have non-zero offset and zero length
		inline bool pass_empty() {
			return (_pass_offset == 0 && _pass_len == 0);
		}
};

} // namespace tagd

