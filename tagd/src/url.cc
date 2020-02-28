#include <cassert>
#include <iostream>
#include <sstream>
#include <string>


#include "tagd/url.h"

namespace tagd {

tagd::code url::init(const std::string &u) {
    if (u.size() == 0) return _code;  // URL_EMPTY
    if (u.size() > URL_MAX_LEN) return code(URL_MAX_LEN);

	auto url_str = u;  // copy becuase we lower scheme
	auto sz = url_str.size();
    url_size_t i = 0;

	auto f_scheme = [this, &url_str] {
		return url_str.substr(0, this->_scheme_len);
	};

// scheme
    for (; i < sz; i++) {
        url_str[i] = tolower(url_str[i]);
        if (url_str[i] == ':') {
            if (i == 0)
				return code(URL_ERR_SCHEME);

			_scheme_len = i;
            if (url_str.substr(i, 3) == "://") {
                i += 3;
                goto authority;
			} 
			
			if (url_str.substr(0, HDURI_SCHEME.size()) == HDURI_SCHEME)
				return this->init_hduri(url_str);

			if (url_str.substr(0, i) == "mailto") {
                // techically a uri, but lets allow it
                i++;  // pass over ':'
                goto authority;
            }

			return code(URL_ERR_SCHEME);
        }
    }

    // missing ':'
    return code(URL_ERR_SCHEME);

authority:
    /*
        Parsing user:pass for our purposes is the same as parsing host:port,
        the way to differentiate is if a '@' is first encountered, we know
        the user:pass parsed is actually a user:pass and we can procede to
        parsing host:port.  But if a {'/', '?', '#'} is first encountered,
        we know that there is no actual user:pass and that what was thought
        to be user:pass should be assigned to user:port
    */

    _user_offset = i;
    for (; i < sz; i++) {
        switch (url_str[i]) {
            case '@':
                if (i == _user_offset) return code(URL_ERR_USER);
                _user_len = i - _user_offset;
                i++; // advance past '@'
                goto host;
            case ':':
                if (i == _user_offset) return code(URL_ERR_USER);
                _user_len = i - _user_offset;
                i++; // advance past ':'
                goto pass;

            /* {'/', '?', '#'} indicate no user, so it must be host */
            case '/':
                // empty user
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                _host_len = i - _host_offset;
                if (_host_len == 0 && f_scheme() != "file")
					return code(URL_ERR_HOST);
                goto path;
            case '?':
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
                //i++; keep the '?'
                goto query;
            case '#':
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				// TODO we probably want to include the # so that we can be consistent with query strings
				// and urls with empty framents can be represented (i.e. http://hypermega.com#)
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // empty user and host
    if (i == _user_offset) return code(URL_ERR_HOST);

    // user is really host, but no port, path, query, fragment
    _host_offset = _user_offset;
    _host_len = i - _host_offset;
	// _user_offset = 0;  TODO uncomment

    goto url_ok;

pass:
    /*
        If we made it here, the parsed out user may in fact be a host.
        If no '@' is encountered, we can assume the user is really a host.
        Empty passwords are allowed (i.e. http://joe:example.com)
    */

    _pass_offset = i;
    for (; i < sz; i++) {
        switch (url_str[i]) {
            case '@':
                _pass_len = i - _pass_offset;
                i++; // advance past '@'
                goto host;

            /* {'/', '?', '#'} indicate no user:pass, so it must be host:port */
            case '/':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
                goto path;
            case '?':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
                //i++; // advance past '?'
                goto query;
            case '#':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // user:pass is really host:port, path, query, fragment
    _host_offset = _user_offset;
    _host_len = _user_len;
    _port_offset = _pass_offset;
    _port_len = i - _port_offset;
    _user_offset = _user_len = _pass_offset = _pass_len = 0;  // no user:pass

    goto url_ok;

host:
    _host_offset = i;
    for (; i < sz; i++) {

        // I wish their were a more elegent way than repeating
        // the same two lines of code for each case
        // We could use gcc's "computed labels" but that's not portable
        switch (url_str[i]) {
            case '/':
                // empty host
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
                goto path;
            case ':':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
                i++; // advance past ':'
                goto port;
            case '?':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // empty host
   if (i == _host_offset) return code(URL_ERR_HOST);

    // no port, path, query, fragment
    _host_len = i - _host_offset;
    goto url_ok;

port:
    _port_offset = i;
    for (; i < sz; i++) {
        switch (url_str[i]) {
            case '/':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
                goto path;
            case '?':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // no path, query, fragment
    _port_len = i - _port_offset;
    goto url_ok;

path:
    _path_offset = i;
    for (; i < sz; i++) {
        switch (url_str[i]) {
            case '?':
                if (i == _path_offset) return code(URL_ERR_PATH);
                _path_len = i - _path_offset;
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _path_offset) return code(URL_ERR_PATH);
                _path_len = i - _path_offset;
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // no query
    _path_len = i - _path_offset;
    goto fragment;

query:
    _query_offset = i;
    for (; i < sz; i++) {
        if (url_str[i] == '#') {
            _query_len = i - _query_offset;
            i++;  // advance past '#'
            goto fragment;
        }
    }

    // no fragment
    _query_len = i - _query_offset;
    goto url_ok;

fragment:
    _fragment_offset = i;
    _fragment_len = sz - _fragment_offset;
	if (_fragment_len > 0)

url_ok:
	// lower host
	for (i = _host_offset; i < (_host_offset + _host_len); i++)
		url_str[i] = tolower(url_str[i]);
	_id = url_str;
    return this->code(TAGD_OK);
}

tagd::code url::init_hduri(const std::string &uri) {
	if (uri.substr(0, HDURI_SCHEME.size()) != HDURI_SCHEME)
		return code(URI_ERR_SCHEME);

	// payload after uri scheme
	const std::string& hduri = uri.substr(HDURI_SCHEME.size());
    auto sz = hduri.size();
    if (sz == 0) return _code;  // URL_EMPTY

    if (sz > URL_MAX_LEN) return code(URL_MAX_LEN);

	// find scheme (at the end)
    auto i = sz;
	while (--i > 0) {
		if (hduri[i] == HDURI_DELIM)
			break;
	}

	// uri has no scheme, it should be treated as a tag
	if (i == 0) return code(URL_ERR_SCHEME);

	// scheme is after the last delim
    std::stringstream ss_url;
	ss_url << decode_hduri_delim(hduri.substr(i+1));

	_scheme_len = sz - (i+1);
	sz = _scheme_len;  // reuse sz for url now

	std::string* elems[HDURI_NUM_ELEMS] = { nullptr };

	{
		// we will split on the hduri before !scheme
		const std::string& s = hduri.substr(0, i);

		size_t idx, j;
		idx = j = i = 0;
		for ( ; i < s.size() && j < s.size() && idx < HDURI_NUM_ELEMS; j++) {
			if (s[j] == HDURI_DELIM) {
				if ((j-i) > 0) {
					elems[idx] = new std::string(s.substr(i, (j-i)));
				} // else remain NULL
				idx++;
				i = j + 1;
			}
		}

		// if last char is delim, we have a trailing blank string indicating a blank password (not NULL)
		if (s[s.size()-1] == HDURI_DELIM) {
			elems[idx] = new std::string();
		} else {
			if (i <= s.size()) {
				elems[idx] = new std::string(s.substr(i));
			}
		}
	}

	ss_url << "://";
	sz += 3;

	if (elems[HDURI_USER]) {
		ss_url << decode_hduri_delim(*(elems[HDURI_USER]));
		_user_offset = sz;
		_user_len = elems[HDURI_USER]->size();
		sz += _user_len;

		if (elems[HDURI_PASS]) {
			// an encoded NULL (%00) denotes an empty password
			//   http://user:@example.com
			// versus a non-existant password
			//   http://user@example.com
			if (*(elems[HDURI_PASS]) == "%00") { // literal empty password
				ss_url << ':';
				_pass_offset = sz + 1;
				_pass_len = 0;  // literal empty passwd gets offset but zero len
				sz += 1;
			} else {
				_pass_len = elems[HDURI_PASS]->size();
				if (_pass_len > 0) {
					ss_url << ':' << decode_hduri_delim(*(elems[HDURI_PASS]));
					_pass_offset = sz + 1;
					sz += _pass_len + 1;
				}
			}
		}

		ss_url << '@';
		sz++;
	}

	if (elems[HDURI_RSUB]) {
		ss_url << reverse_labels(decode_hduri_delim(*(elems[HDURI_RSUB])));
		_host_offset = sz;
		_host_len = elems[HDURI_RSUB]->size();
		sz += _host_len;
	}

	if (elems[HDURI_PRIV]) {
		if (_host_offset == 0) _host_offset = sz;
		if (_host_len > 0) {
			ss_url << '.';
			_host_len++;  sz++;
		}

		ss_url << decode_hduri_delim(*(elems[HDURI_PRIV]));
		_host_len += elems[HDURI_PRIV]->size();
		sz += elems[HDURI_PRIV]->size();
	}

	if (elems[HDURI_RPUB]) {
		if (_host_offset == 0) _host_offset = sz;
		if (_host_len > 0) {
			ss_url << '.';
			_host_len++;  sz++;
		}

		ss_url << reverse_labels(decode_hduri_delim(*(elems[HDURI_RPUB])));
		_host_len += elems[HDURI_RPUB]->size();
		sz += elems[HDURI_RPUB]->size();
	}

	if (elems[HDURI_PORT]) {
		ss_url << ':' << decode_hduri_delim(*(elems[HDURI_PORT]));
		_port_offset = sz + 1;
        _port_len = elems[HDURI_PORT]->size();
		sz += elems[HDURI_PORT]->size() + 1;
	}

	if (elems[HDURI_PATH]) {
		ss_url << decode_hduri_delim(*(elems[HDURI_PATH]));
		_path_offset = sz;
        _path_len = elems[HDURI_PATH]->size();
		sz += elems[HDURI_PATH]->size();
	}

	if (elems[HDURI_QUERY]) {
		ss_url << decode_hduri_delim(*(elems[HDURI_QUERY]));
		_query_offset = sz;
        _query_len = elems[HDURI_QUERY]->size();
		sz += elems[HDURI_QUERY]->size();
	}

	if (elems[HDURI_FRAGMENT]) {
		ss_url << '#' << decode_hduri_delim(*(elems[HDURI_FRAGMENT]));
		_fragment_offset = sz + 1;
        _fragment_len = elems[HDURI_FRAGMENT]->size();
		sz += elems[HDURI_FRAGMENT]->size() + 1;
	}

	for( i = 0; i < HDURI_NUM_ELEMS; i++ )
		if (elems[i] != nullptr) delete elems[i];

	_id = ss_url.str();
    return code(TAGD_OK);
}

std::string encode_hduri_delim(std::string elem) {
	size_t pos = 0;
	while ((pos = elem.find(HDURI_DELIM, pos)) != std::string::npos) {
		elem.replace(pos, 1, HDURI_DELIM_ENCODED);
		pos += HDURI_DELIM_ENCODED.length();
	}
	return elem;
}

std::string decode_hduri_delim(std::string elem) {
	size_t pos = 0;
	while ((pos = elem.find(HDURI_DELIM_ENCODED, pos)) != std::string::npos) {
		elem.replace(pos, HDURI_DELIM_ENCODED.length(), 1, HDURI_DELIM);  // replace encoded str with 1 char delim
		pos++;  // length of char
	}
	return elem;
}

// A Hierarchical URI Decomposition (HDURI) is composed of:
//
//  rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
//
//  '!' characters in any url parts must be encoded (%21)
//
//  http://sub2.sub1.example.co.uk/a/b/c?id=foo&x=123#summary
//                     ||  ||
//                     \/  \/
//  uk.co!example!sub1.sub2!/a/b/c!?id=foo&x=123!summary!!!!http
//    |    |        |          |         |        |     ^    |
//  rpub   |       rsub       path     query   fragment ^    |
//       private                                        ^   scheme
//                                                      ^
//	though port, user, pass are empty, they must have placeholders
//
//  empty URL parts between non-empty ones still must have placeholders
//  for example:
//
//  http://example.com?hi=hey
//  			|
//              v
//  com!example!!!?hi=hey!!!!!http
std::string url::hduri() const {
	assert(!this->scheme().empty());

    std::stringstream ss;
	ss << HDURI_SCHEME;

	tagd::domain d(this->host());

	auto hduri_elem_f = [&ss](const std::string &elem, bool rev_labels=false) {
		ss << HDURI_DELIM;
		if (!elem.empty())
			ss << encode_hduri_delim(rev_labels ? reverse_labels(elem) : elem);
	};

	if (!d.pub().empty())
		ss << encode_hduri_delim(reverse_labels(d.pub()));

	hduri_elem_f(d.priv_label());
	hduri_elem_f(d.sub(), true);
	hduri_elem_f(this->path());
	hduri_elem_f(this->query());
	hduri_elem_f(this->fragment());
	hduri_elem_f(this->port());
	hduri_elem_f(this->user());

	// we have to distinguish between "no password" and "blank password"
	if (_pass_offset != 0 && _pass_len == 0)
		   ss << HDURI_DELIM << "%00"; // :<blank pass>
	else
		   hduri_elem_f(this->pass());

	hduri_elem_f(this->scheme());

    return ss.str();
}

// inserts host part relations, returns tld_code from domain::init
void insert_host_parts(tagd::predicate_set& P, const url& u) {
	tagd::domain d(u.host());
	if (d.code() == TLD_ERR) return;

	std::string s = d.pub();
	if (!s.empty())
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_PUBLIC, d.pub());

	s = d.priv_label();
	if (!s.empty())
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, d.priv_label());

	s = d.sub();
	if (!s.empty())
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, d.sub());

	return;
}

size_t url::parse_query(url_query_map_t& M, const std::string& s) {
	size_t n = M.size();

	for(size_t i=0; i<s.size(); ++i) {
		if ((s[i] == '?' || i==0 || s[i] == '&') && ((i+1)<s.size())) {
			std::string k, v;
			if (!(i == 0 && s[i] != '?')) {  // handle leading '?' or not
				if (++i == s.size()-1) {
					k = s.substr(i);
					goto key_val;
				}
			}
			for(size_t j=i; j<s.size(); ++j) {
				if (s[j] == '=' || s[j] == '&' || (j == (s.size()-1))) {
					k = s.substr(i, (j-i));
					if (s[j] == '&') {
						v.clear();
						goto key_val;
					}
					if (s[j] == '=') {
						if (++j<s.size()) {
							size_t k = j;
							for(; (k<s.size() && s[k] != '&'); ++k );
							v = s.substr(j, (k-j));
							goto key_val;
						}
					}
				}
			}
key_val:
			v = uri_decode(v);
			for (size_t i=0; i<v.size(); i++) {
				if (v[i] == '+')
					v[i] = ' ';
			}
			M[k] = v;
		}
	}

	return (M.size() - n);
}

bool url::query_find(const url_query_map_t& m, std::string& val, const std::string& key) {
	auto it = m.find(key);
	if (it != m.end()) {
		val = it->second;
		return true;
	} else {
		return false;
	}
}

bool url::looks_like_url(const std::string& s) {
	return (s.find("://") != std::string::npos);
}

// if looks like hduri
bool url::looks_like_hduri(const std::string& uri) {
	if (uri.substr(0, HDURI_SCHEME.size()) != HDURI_SCHEME)
		return false;

	// payload after uri scheme
	const std::string& str = uri.substr(HDURI_SCHEME.size());

	auto i = str.rfind(HDURI_DELIM);
	if (i == std::string::npos) return false;

	auto sz = str.size() - i;
	switch (sz) {
		case 4:  // "!ftp"
			if (str.substr((i+1), sz-1) == "ftp")
				return true;
			break;
		case 5:
			// "!http"
			if (str.substr((i+1), sz-1) == "http")
				return true;
			// "!file"
			if (str.substr((i+1), sz-1) == "file")
				return true;
			break;
		case 6:  // "!https"
			if (str.substr((i+1), sz-1) == "https")
				return true;
			break;
		default:
			return false;
	}

	return false;
}

/*\
|*| Uri encode and decode.
|*| RFC1630, RFC1738, RFC2396
|*| Thank you Jin Qing
|*| see: http://www.codeguru.com/cpp/cpp/algorithms/strings/article.php/c12759/URI-Encoding-and-Decoding.htm
\*/
const char HEX2DEC[256] =
{
    /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

    /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

    /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

    /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

std::string uri_decode(const std::string & sSrc)
{
    // Note from RFC1630:  "Sequences which start with a percent sign
    // but are not followed by two hexadecimal characters (0-9, A-F) are reserved
    // for future extension"

    const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
	const int SRC_LEN = sSrc.length();
    const unsigned char * const SRC_END = pSrc + SRC_LEN;
    const unsigned char * const SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

    char * const pStart = new char[SRC_LEN];
    char * pEnd = pStart;

    while (pSrc < SRC_LAST_DEC)
	{
		if (*pSrc == '%')
        {
            char dec1, dec2;
            if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)])
                && -1 != (dec2 = HEX2DEC[*(pSrc + 2)]))
            {
                *pEnd++ = (dec1 << 4) + dec2;
                pSrc += 3;
                continue;
            }
        }

        *pEnd++ = *pSrc++;
	}

    // the last 2- chars
    while (pSrc < SRC_END)
        *pEnd++ = *pSrc++;

    std::string sResult(pStart, pEnd);
    delete [] pStart;
	return sResult;
}

// safe chars: [0-9a-zA-Z_]
const char SAFE[256] =
{
    /*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
    /* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

    /* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    /* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,1,
    /* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    /* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,

    /* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

    /* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

std::string uri_encode(const std::string & sSrc)
{
    const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
    const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
    const int SRC_LEN = sSrc.length();
    unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
    unsigned char * pEnd = pStart;
    const unsigned char * const SRC_END = pSrc + SRC_LEN;

    for (; pSrc < SRC_END; ++pSrc)
	{
		if (SAFE[*pSrc])
            *pEnd++ = *pSrc;
        else
        {
            // escape this char
            *pEnd++ = '%';
            *pEnd++ = DEC2HEX[*pSrc >> 4];
            *pEnd++ = DEC2HEX[*pSrc & 0x0F];
        }
	}

    std::string sResult((char *)pStart, (char *)pEnd);
    delete [] pStart;
    return sResult;
}

void url::insert_url_part_relations(tagd::predicate_set& P, const url& u) {
	if (u._scheme_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_SCHEME, u.scheme());

	if (u._user_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_USER, u.user());

	if (u._pass_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_PASS, u.pass());

	if (u._host_len > 0) {
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_HOST, u.host());
		insert_host_parts(P, u);
	}

	if (u._port_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_PORT, u.port());

	if (u._path_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_PATH, u.path());

	if (u._query_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_QUERY, u.query());

	if (u._fragment_len > 0)
		tagd::insert_predicate(P, HARD_TAG_HAS, HARD_TAG_FRAGMENT, u.fragment());
}

} // namespace tagd
