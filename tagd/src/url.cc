#include <cassert>
#include <iostream>
#include <sstream>

#include "tagd/url.h"

namespace tagd {

url_code url::init(const std::string &url) {
    if (!this->empty()) this->clear();

    size_t sz = url.size();
    if (sz == 0) return _url_code;  // URL_EMPTY

    if (sz > URL_MAX_LEN) return code(URL_MAX_LEN);

    _uri = url;
    url_size_t i = 0;

// scheme
    for (; i < sz; i++) {
        _uri[i] = tolower(_uri[i]);
        if (_uri[i] == ':') {
            if (i == 0) return code(URL_ERR_SCHEME);

            if (_uri.substr(i, 3) == "://") {
				/*
				 * hduri://com:example:http
				 * becomes:
				 * http://example.com
				 */
				if (_uri.substr(0, i) == "hduri")
					return this->init_hduri(_uri.substr(i+3));
                _scheme_len = i;
				this->relation(HARD_TAG_HAS, HARD_TAG_SCHEME, _uri.substr(0, _scheme_len));
                i += 3;
                goto authority;
            } else if (_uri.substr(0, i) == "mailto") {
                // techically a uri, but lets allow it
                _scheme_len = i;
				this->relation(HARD_TAG_HAS, HARD_TAG_SCHEME, _uri.substr(0, _scheme_len));
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
        switch (_uri[i]) {
            case '@':
                if (i == _user_offset) return code(URL_ERR_USER);
                _user_len = i - _user_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_USER, _uri.substr(_user_offset, _user_len));
                i++; // advance past '@'
                goto host;
            case ':':
                if (i == _user_offset) return code(URL_ERR_USER);
                _user_len = i - _user_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_USER, _uri.substr(_user_offset, _user_len));
                i++; // advance past ':'
                goto pass;

            /* {'/', '?', '#'} indicate no user, so it must be host */
            case '/':
                // empty user
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
                goto path;
            case '?':
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
                //i++; keep the '?'
                goto query;
            case '#':
                _host_offset = _user_offset;
				_user_offset = _user_len = 0;
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
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
	this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));

    goto url_ok;

pass:
    /*
        If we made it here, the parsed out user may in fact be a host.
        If no '@' is encountered, we can assume the user is really a host.
        Empty passwords are ok (i.e. http://joe:example.com)
    */

    _pass_offset = i;
    for (; i < sz; i++) {
        switch (_uri[i]) {
            case '@':
                _pass_len = i - _pass_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PASS, _uri.substr(_pass_offset, _pass_len));
                i++; // advance past '@'
                goto host;

            /* {'/', '?', '#'} indicate no user:pass, so it must be host:port */
            case '/':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_PASS);
                goto path;
            case '?':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_PASS);
                //i++; // advance past '?'
                goto query;
            case '#':
                _host_offset = _user_offset;
                _host_len = _user_len;
                _user_offset = _user_len = 0;
                _port_offset = _pass_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _pass_offset;
                _pass_offset = _pass_len = 0;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
				this->not_relation(HARD_TAG_HAS, HARD_TAG_PASS);
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

	this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
	this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
	this->not_relation(HARD_TAG_HAS, HARD_TAG_USER);
	this->not_relation(HARD_TAG_HAS, HARD_TAG_PASS);

    goto url_ok;

host:
    _host_offset = i;
    for (; i < sz; i++) {
        _uri[i] = tolower(_uri[i]);

        // I wish their were a more elegent way than repeating
        // the same two lines of code for each case
        // We could use gcc's "computed labels" but that's not portable
        switch (_uri[i]) {
            case '/':
                // empty host
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
                goto path;
            case ':':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
                i++; // advance past ':'
                goto port;
            case '?':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _host_offset) return code(URL_ERR_HOST);
                _host_len = i - _host_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_HOST, _uri.substr(_host_offset, _host_len));
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
        switch (_uri[i]) {
            case '/':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
                goto path;
            case '?':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _port_offset) return code(URL_ERR_PORT);
                _port_len = i - _port_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // no path, query, fragment
    _port_len = i - _port_offset;
	this->relation(HARD_TAG_HAS, HARD_TAG_PORT, _uri.substr(_port_offset, _port_len));
    goto url_ok;

path:
    _path_offset = i;
    for (; i < sz; i++) {
        switch (_uri[i]) {
            case '?':
                if (i == _path_offset) return code(URL_ERR_PATH);
                _path_len = i - _path_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PATH, _uri.substr(_path_offset, _path_len));
                //i++; // advance past '?'
                goto query;
            case '#':
                if (i == _path_offset) return code(URL_ERR_PATH);
                _path_len = i - _path_offset;
				this->relation(HARD_TAG_HAS, HARD_TAG_PATH, _uri.substr(_path_offset, _path_len));
                i++; // advance past '#'
                goto fragment;
            //default: NOP
        }
    }

    // no query
    _path_len = i - _path_offset;
	this->relation(HARD_TAG_HAS, HARD_TAG_PATH, _uri.substr(_path_offset, _path_len));
    goto fragment;

query:
    _query_offset = i;
    for (; i < sz; i++) {
        if (_uri[i] == '#') {
            _query_len = i - _query_offset;
			this->relation(HARD_TAG_HAS, HARD_TAG_QUERY, _uri.substr(_query_offset, _query_len));
            i++;  // advance past '#'
            goto fragment;
        }
    }

    // no fragment
    _query_len = i - _query_offset;
	this->relation(HARD_TAG_HAS, HARD_TAG_QUERY, _uri.substr(_query_offset, _query_len));
    goto url_ok; 

fragment:
    _fragment_offset = i;
    _fragment_len = sz - _fragment_offset;
	if (_fragment_len > 0)
		this->relation(HARD_TAG_HAS, HARD_TAG_FRAGMENT, _uri.substr(_fragment_offset, _fragment_len));

url_ok:
	this->host_lower();
	if (_host_len > 0)
		this->add_host_parts();
	_id = this->hduri();
    return code(URL_OK);
}

// adds host part relations, returns tld_code from domain::init
tld_code url::add_host_parts() {
	assert(_host_len > 0);

	tagd::domain d(this->host());
	if (d.code() == TLD_ERR) return TLD_ERR;

	std::string s = d.pub();
	if (!s.empty())
		this->relation(HARD_TAG_HAS, HARD_TAG_PUB, d.pub());

	s = d.priv_label();
	if (!s.empty())
		this->relation(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, d.priv_label());

	s = d.sub();
	if (!s.empty())
		this->relation(HARD_TAG_HAS, HARD_TAG_SUB, d.sub());

	return d.code();
}

url_code url::init_hduri(const std::string &hduri) {
    if (!this->empty()) this->clear();

    size_t sz = hduri.size();
    if (sz == 0) return _url_code;  // URL_EMPTY

    if (sz > URL_MAX_LEN) return code(URL_MAX_LEN);

    _id = hduri;
    size_t i = sz;

	while (--i > 0) {
		if (_id[i] == HDURI_DELIM)
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

	_uri = ss_url.str();
    return code(URL_OK);
}

inline void print_hduri_elem(std::ostream& os, const std::string& elem, int *dropped, bool rev_labels=false) {
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


} // namespace tagd
