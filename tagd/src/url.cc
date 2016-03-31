#include <cassert>
#include <iostream>
#include <sstream>
#include <string>


#include "tagd/url.h"

namespace tagd {

tagd_code url::init(const std::string &url) {
    if (!this->empty()) this->clear();

    size_t sz = url.size();
    if (sz == 0) return _code;  // URL_EMPTY

    if (sz > URL_MAX_LEN) return code(URL_MAX_LEN);

    _id = url;
    url_size_t i = 0;

// scheme
    for (; i < sz; i++) {
        _id[i] = tolower(_id[i]);
        if (_id[i] == ':') {
            if (i == 0) return code(URL_ERR_SCHEME);

            if (_id.substr(i, 3) == "://") {
				/*
				 * hduri://com:example:http
				 * becomes:
				 * http://example.com
				 */
				if (_id.substr(0, i) == "hduri")
					return this->init_hduri(_id.substr(i+3));
                _scheme_len = i;
                i += 3;
                goto authority;
            } else if (_id.substr(0, i) == "mailto") {
                // techically a uri, but lets allow it
                _scheme_len = i;
                i++;  // pass over ':'
                goto authority;
            } else {
                return code(URL_ERR_SCHEME);
            }
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
        switch (_id[i]) {
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
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
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
        Empty passwords are ok (i.e. http://joe:example.com)
    */

    _pass_offset = i;
    for (; i < sz; i++) {
        switch (_id[i]) {
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
        switch (_id[i]) {
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
        switch (_id[i]) {
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
        switch (_id[i]) {
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
        if (_id[i] == '#') {
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
	this->host_lower();
    return this->code(TAGD_OK);
}

tagd_code url::init_hduri(const std::string &hduri) {
    if (!this->empty()) this->clear();

    size_t sz = hduri.size();
    if (sz == 0) return _code;  // URL_EMPTY

    if (sz > URL_MAX_LEN) return code(URL_MAX_LEN);

	// find scheme (at the end)
    size_t i = sz;
	while (--i > 0) {
		if (hduri[i] == HDURI_DELIM)
			break;
	}

	// uri has no scheme, it should be treated as a tag
	if (i == 0) return code(URL_ERR_SCHEME);

	// scheme is after the last delim
    std::stringstream ss_url;
	ss_url << hduri.substr(i+1);

	_scheme_len = sz - (i+1);
	sz = _scheme_len;  // reuse sz for url now

	// *** don't leak by forgetting to delete any new strings before returning
	std::string* elems[HDURI_NUM_ELEMS] = { NULL };

	//std::cout << "ss_url(" << __LINE__ << "): " << ss_url.str() << std::endl;
	//std::cout << "sz(" << __LINE__ << "): " << sz << std::endl;
	//std::cout << "_scheme_len(" << __LINE__ << "): " << _scheme_len << std::endl;

	{
		// we will split on the hduri before :scheme
		std::string s(hduri.substr(0, i));

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
		ss_url << *(elems[HDURI_USER]);
		_user_offset = sz;
		_user_len = elems[HDURI_USER]->size();
		sz += _user_len;

		if (elems[HDURI_PASS]) {
			ss_url << ':' << *(elems[HDURI_PASS]);
			_pass_offset = sz + 1;
			_pass_len = elems[HDURI_PASS]->size();
			sz += _pass_len + 1;
		}

		ss_url << '@';
		sz++;
	}

	if (elems[HDURI_RSUB]) {
		ss_url << reverse_labels(*(elems[HDURI_RSUB]));
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

		ss_url << *(elems[HDURI_PRIV]);
		_host_len += elems[HDURI_PRIV]->size();
		sz += elems[HDURI_PRIV]->size();
	} 

	if (elems[HDURI_RPUB]) {
		if (_host_offset == 0) _host_offset = sz;
		if (_host_len > 0) {
			ss_url << '.';
			_host_len++;  sz++;
		}

		ss_url << reverse_labels(*(elems[HDURI_RPUB]));
		_host_len += elems[HDURI_RPUB]->size();
		sz += elems[HDURI_RPUB]->size();
	}

	if (elems[HDURI_PORT]) {
		ss_url << ':' << *(elems[HDURI_PORT]);
		_port_offset = sz + 1;
        _port_len = elems[HDURI_PORT]->size();
		sz += elems[HDURI_PORT]->size() + 1;
	}

	if (elems[HDURI_PATH]) {
		ss_url << *(elems[HDURI_PATH]);
		_path_offset = sz;
        _path_len = elems[HDURI_PATH]->size();
		sz += elems[HDURI_PATH]->size();
	}

	if (elems[HDURI_QUERY]) {
		ss_url << *(elems[HDURI_QUERY]);
		_query_offset = sz;
        _query_len = elems[HDURI_QUERY]->size();
		sz += elems[HDURI_QUERY]->size();
	}

	if (elems[HDURI_FRAGMENT]) {
		ss_url << '#' << *(elems[HDURI_FRAGMENT]);
		_fragment_offset = sz + 1;
        _fragment_len = elems[HDURI_FRAGMENT]->size();
		sz += elems[HDURI_FRAGMENT]->size() + 1;
	}

	for( i = 0; i < HDURI_NUM_ELEMS; i++ )
		if (elems[i] != NULL) delete elems[i];

	_id = ss_url.str();
    return code(TAGD_OK);
}

void print_hduri_elem(std::ostream& os, const std::string& elem, int *dropped, bool rev_labels=false) {
	if (!elem.empty()) {
		if (*dropped) {
			os << std::string((*dropped), HDURI_DELIM);
			*dropped = 0;
		}

		os << HDURI_DELIM << (rev_labels ? reverse_labels(elem) : elem); 
	} else
		(*dropped)++;
}

// A Hierarchical URI Decomposition (HDURI) is composed of:
//
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
//
//  ':' characters in any url parts must be encoded (%3a)
//
//  http://sub2.sub1.example.co.uk/a/b/c?id=foo&x=123#summary
//                     ||  ||
//                     \/  \/
//  uk.co:example:sub1.sub2:/a/b/c:?id=foo&x=123:summary:http
//    |    |        |          |         |        |     ^  |
//  rpub   |       rsub       path     query    anchor  ^  |
//       private                                        ^ scheme
//                                                      ^
//	port, user, pass are optional (can be dropped) in this case,
//	because they are followed by non-empty elements
//
//  empty elements between non-empty ones, must be delimmed,
//  for example:
//  
//  http://example.com?hi=hey
//  			|
//              v
//  com:example:::?hi=hey:http
//
//  rsub and path are blank, but must be represented
//  fragment, port, user and pass can be dropped because
//  a trailing scheme is required
//
//  TODO, you probably want to use '~' (ascii 127) as your delimiter
//  because it will collate after all other printable chars
std::string url::hduri() const {
	assert(!this->scheme().empty());

    std::stringstream ss;
	tagd::domain d(this->host());
	int dropped = 0;  // the number of delims dropped

	if (!d.pub().empty())
		ss << reverse_labels(d.pub());
	else
		dropped++;

	print_hduri_elem(ss, d.priv_label(), &dropped);
	print_hduri_elem(ss, d.sub(), &dropped, true);
	print_hduri_elem(ss, this->path(), &dropped);
	print_hduri_elem(ss, this->query(), &dropped);
	print_hduri_elem(ss, this->fragment(), &dropped);
	print_hduri_elem(ss, this->port(), &dropped);
	print_hduri_elem(ss, this->user(), &dropped);

	// we have to distinguish between "no password"
	// and "blank password"
	if (_pass_offset != 0 && _pass_len == 0) {
		if (dropped) {
			ss << std::string(dropped, HDURI_DELIM);
			dropped = 0;
		}
		ss << HDURI_DELIM;  // :<blank pass>
	} else
		print_hduri_elem(ss, this->pass(), &dropped);

	ss << HDURI_DELIM << this->scheme();

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

	//std::cerr << "parse_query: " << s << std::endl;
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
					//std::cerr << "k: " << k << std::endl;
					if (s[j] == '&') {
						v.clear();
						//std::cerr << "v: " << v << std::endl;
						goto key_val;
					}
					if (s[j] == '=') {
						if (++j<s.size()) {
							size_t k = j;
							for(; (k<s.size() && s[k] != '&'); ++k );
							v = s.substr(j, (k-j));
							//std::cerr << "v: " << v << std::endl;
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
			//std::cerr << "M[" << k << "] = " << v << std::endl;
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
bool url::looks_like_hduri(const std::string& str) {
	size_t i = str.rfind(':');
	if (i == std::string::npos) return false;

	int sz = str.size() - i;
	switch (sz) {
		case 4:  // ":ftp"
			if (str.substr((i+1), sz-1) == "ftp")
				return true;
			break;
		case 5:
			// ":http"
			if (str.substr((i+1), sz-1) == "http")
				return true;
			// ":file"
			if (str.substr((i+1), sz-1) == "file")
				return true;
			break;
		case 6:  // ":https"
			if (str.substr((i+1), sz-1) == "https")
				return true;
			break;
		default:
			return false;
	}

	return false;
}



// Uri encode and decode.
// RFC1630, RFC1738, RFC2396
// see: http://www.codeguru.com/cpp/cpp/algorithms/strings/article.php/c12759/URI-Encoding-and-Decoding.htm
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
