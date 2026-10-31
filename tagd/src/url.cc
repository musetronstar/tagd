#include <cassert>
#include <algorithm>
#include <memory>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <strings.h>  // strcasecmp
#include <arpa/inet.h>  // inet_pton


#include "tagd/url.h"
#include "tagd/utf8.h"

namespace tagd {

id_string canonical_url_id(id_view input) {
	const std::string id(input);
	if (url::looks_like_url(id) || url::looks_like_hduri(id)) {
		const url value(id);
		if (value.ok())
			return value.hduri();
	}
	return id;
}

namespace {

// URL case rules are ASCII rules; the C library's tolower would follow the process locale.
constexpr char ascii_lower(char c) {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool hex_digit(unsigned char c) {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// RFC 3986 unreserved and sub-delims: the ASCII a host holds without percent-encoding.
constexpr bool host_byte(unsigned char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
		|| std::string_view{"-._~!$&'()*+,;="}.find(static_cast<char>(c)) != std::string_view::npos;
}

constexpr bool userinfo_byte(unsigned char c) {
	return host_byte(c) || c == ':';
}

// Path, query, and fragment keep producer syntax, excluding ASCII space and controls.
constexpr bool text_byte(unsigned char c) {
	return c > 0x20 && c < 0x7F;
}

enum class url_escapes {
	unchecked,  // '%' is an ordinary byte
	checked     // '%' begins a %XX escape
};

/*
 * Raw text uses the component's ASCII rules and well-formed UTF-8 without C1
 * controls. Checked escapes validate spelling only, preserving userinfo octets.
 */
bool url_component_ok(std::string_view s, bool (*ascii)(unsigned char), url_escapes escapes) {
	for (size_t i = 0; i < s.size(); ) {
		const auto c = static_cast<unsigned char>(s[i]);
		if (c == '%' && escapes != url_escapes::unchecked) {
			if (s.size() - i < 3 || !hex_digit(s[i + 1]) || !hex_digit(s[i + 2]))
				return false;

			i += 3;
			continue;
		}

		if (c < 0x80) {
			if (!ascii(c))
				return false;

			i++;
			continue;
		}

		// C1 controls U+0080 to U+009F are the sequences 0xC2 0x80 to 0xC2 0x9F.
		const auto len = utf8_sequence_len(s, i);
		if (len == 0 || (c == 0xC2 && static_cast<unsigned char>(s[i + 1]) < 0xA0))
			return false;

		i += len;
	}

	return true;
}

// RFC 3986 IP-literal: "[" ( IPv6address / IPvFuture ) "]"; RFC 6874 zone IDs are not supported.
bool ip_literal_ok(std::string_view host) {
	if (host.size() < 3 || host.back() != ']')
		return false;

	const auto address = host.substr(1, host.size() - 2);
	if (address.front() == 'v' || address.front() == 'V') {
		// IPvFuture: "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
		const auto dot = address.find('.');
		if (dot == std::string_view::npos || dot == 1 || dot + 1 == address.size())
			return false;

		return std::all_of(address.begin() + 1, address.begin() + dot,
				[](unsigned char c) { return hex_digit(c); })
			&& std::all_of(address.begin() + dot + 1, address.end(),
				[](unsigned char c) { return userinfo_byte(c); });
	}

	// Check every byte before inet_pton, whose C-string input would stop at an embedded NUL.
	if (!std::all_of(address.begin(), address.end(),
			[](unsigned char c) { return hex_digit(c) || c == ':' || c == '.'; }))
		return false;

	in6_addr parsed;
	return inet_pton(AF_INET6, std::string(address).c_str(), &parsed) == 1;
}

bool host_ok(std::string_view host) {
	if (!host.empty() && host.front() == '[')
		return ip_literal_ok(host);

	// HDURI and HTTP transport escapes are already removed; actual hosts must be literal text.
	return url_component_ok(host, host_byte, url_escapes::unchecked);
}

bool port_ok(std::string_view port) {
	return std::all_of(port.begin(), port.end(),
		[](unsigned char c) { return c >= '0' && c <= '9'; });
}

/*
 * RFC 3986 sections 3.2.2 and 6.2.2.1: ASCII host letters compare without case,
 * so URL identity keeps them lowercase. Requires a host that passed host_ok.
 *
 * TODO: Unicode hosts stay Unicode in tag IDs, but are not case-mapped yet.
 * Map them with IDNA (UTS #46) and keep the Unicode result; convert to
 * punycode only where a transport requires ASCII, such as an HTTP request.
 */
void normalize_host(std::string& url, size_t offset, size_t len) {
	for (size_t i = offset; i < offset + len; i++) {
		url[i] = ascii_lower(url[i]);
	}
}

} // namespace

tagd::code url::init(id_view u) {
	if (u.size() == 0) return _code;  // URL_EMPTY

	// HDURI escaping can exceed the URL byte limit even when the decoded URL fits.
	if (u.size() >= HDURI_SCHEME.size() && (u[0] == 'h' || u[0] == 'H') &&
		(u[1] == 'd' || u[1] == 'D') && u[2] == ':') {
		if (u.size() > HDURI_MAX_LEN)
			return code(URL_MAX_LEN);

		if (u.starts_with(HDURI_SCHEME))
			return init_hduri(u);

		return init_hduri(HDURI_SCHEME + std::string(u.substr(HDURI_SCHEME.size())));
	}
	if (u.size() > URL_MAX_LEN) return code(URL_MAX_LEN);

	auto url_str = std::string(u);  // copy becuase we lower scheme
	auto sz = url_str.size();
	url_size_t i = 0;

	auto f_scheme = [this, &url_str] {
		return url_str.substr(0, this->_scheme_len);
	};

// scheme
	for (; i < sz; i++) {
		url_str[i] = ascii_lower(url_str[i]);
		if (url_str[i] == ':') {
			if (i == 0)
				return code(URL_ERR_SCHEME);

			_scheme_len = i;
			if (url_str.substr(i, 3) == "://") {
				// Only the nonhierarchical mailto form has a reversible component layout.
				if (f_scheme() == "mailto")
					return code(URL_ERR_SCHEME);
				i += 3;
				goto authority;
			}

			if (url_str.substr(0, i) == "mailto") {
				// techically a uri, but lets allow it
				i++;  // pass over ':'
				goto authority;
			}

			return code(URL_ERR_SCHEME);
		}

		// Every URL input path, including HDURI reconstruction, shares this scheme grammar.
		if (!(i == 0 ? url_scheme_initial(url_str[i]) : url_scheme_byte(url_str[i])))
			return code(URL_ERR_SCHEME);
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
			case '[':
				// Userinfo cannot hold '[', so an authority starting with one is an IP literal host.
				if (i != _user_offset)
					break;
				_user_offset = 0;
				goto host;

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
		Empty passwords are allowed (i.e. http://joe:@example.com)
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
				goto fragment;
			//default: NOP
		}
	}

	// user:pass is really host:port, path, query, fragment
	_host_offset = _user_offset;
	_host_len = _user_len;
	_port_offset = _pass_offset;
	_port_len = i - _port_offset;
	// The layout has no empty-port sentinel, so a trailing ':' must not lose identity.
	if (_port_len == 0)
		return code(URL_ERR_PORT);
	_user_offset = _user_len = _pass_offset = _pass_len = 0;  // no user:pass

	goto url_ok;

host:
	_host_offset = i;
	// Colons inside an IP literal belong to the address, not a port delimiter.
	if (i < sz && url_str[i] == '[') {
		const auto close = url_str.find(']', i);
		if (close == std::string::npos)
			return code(URL_ERR_HOST);
		i = close + 1;
	}
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
				goto fragment;
			//default: NOP
		}
	}

	// no path, query, fragment
	_port_len = i - _port_offset;
	if (_port_len == 0)
		return code(URL_ERR_PORT);
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
			goto fragment;
		}
	}

	// no fragment
	_query_len = i - _query_offset;
	goto url_ok;

fragment:
	_fragment_offset = i;
	_fragment_len = sz - _fragment_offset;

url_ok:
	{
		const std::string_view parsed{url_str};
		auto part = [&parsed](url_size_t offset, url_size_t len) {
			return parsed.substr(offset, len);
		};

		// All URL input forms share these authority and raw-text checks before identity forms.
		if (!url_component_ok(part(_user_offset, _user_len), userinfo_byte, url_escapes::checked)
			|| !url_component_ok(part(_pass_offset, _pass_len), userinfo_byte, url_escapes::checked))
			return code(URL_ERR_USER);

		if (!host_ok(part(_host_offset, _host_len)))
			return code(URL_ERR_HOST);

		if (!port_ok(part(_port_offset, _port_len)))
			return code(URL_ERR_PORT);

		// tagd has no query or fragment codes, so their errors report as path errors.
		if (!url_component_ok(part(_path_offset, _path_len), text_byte, url_escapes::unchecked)
			|| !url_component_ok(part(_query_offset, _query_len), text_byte, url_escapes::unchecked)
			|| !url_component_ok(part(_fragment_offset, _fragment_len), text_byte,
				url_escapes::unchecked))
			return code(URL_ERR_PATH);
	}

	/*
	 * Construction is where URL identity forms. HDURI decoding re-parses here
	 * too, so an HDURI spelling a host any other way fails its canonical check.
	 */
	normalize_host(url_str, _host_offset, _host_len);
	_id = url_str;
	// a file: scheme url identifies materialized content;
	// the url object provides the subordinate relation, never the user
	if (this->scheme() == "file") {
		_sub_relator = HARD_TAG_SUB;
		_super_object = HARD_TAG_FILE;
	}
	return this->code(TAGD_OK);
}

namespace {
std::string hduri_host(const std::string& rpub, const std::string& priv,
	const std::string& rsub) {
	std::string host = reverse_labels(rsub);
	for (const auto& part : {priv, reverse_labels(rpub)}) {
		if (part.empty()) {
			continue;
		}
		if (!host.empty()) {
			host += '.';
		}
		host += part;
	}

	return host;
}
} // namespace

tagd::code url::init_hduri(id_view uri) {
	if (uri.substr(0, HDURI_SCHEME.size()) != HDURI_SCHEME)
		return code(URI_ERR_SCHEME);

	// payload after uri scheme
	const auto hduri = uri.substr(HDURI_SCHEME.size());
	auto sz = hduri.size();
	if (sz == 0) return _code;  // URL_EMPTY

	if (uri.size() > HDURI_MAX_LEN) return code(URL_MAX_LEN);

	// find scheme (at the end)
	auto i = sz;
	while (--i > 0) {
		if (hduri[i] == HDURI_DELIM)
			break;
	}

	// uri has no scheme, it should be treated as a tag
	if (i == 0) return code(URL_ERR_SCHEME);

	// scheme is after the last delim
	std::string scheme;
	if (!decode_hduri_delim(scheme, hduri.substr(i + 1)) || scheme.empty())
		return code(HDURI_ERR);

	// A reconstructed URL must not dispatch back into HDURI parsing.
	if (strcasecmp(scheme.c_str(), "hd") == 0 || scheme.find(':') != std::string::npos)
		return code(HDURI_ERR);

	std::stringstream ss_url;
	ss_url << scheme;

	_scheme_len = scheme.size();
	sz = _scheme_len;  // reuse sz for url now

	// Fields remain owned when validation returns early.
	std::unique_ptr<std::string> elems[HDURI_NUM_ELEMS];
	bool empty_password = false;

	{
		// we will split on the hduri before !scheme
		const auto s = hduri.substr(0, i);
		if (std::count(s.begin(), s.end(), HDURI_DELIM) != HDURI_NUM_ELEMS - 1)
			return code(HDURI_ERR);

		size_t idx, j;
		idx = j = i = 0;
		for ( ; i < s.size() && j < s.size() && idx < HDURI_NUM_ELEMS; j++) {
			if (s[j] == HDURI_DELIM) {
				if ((j-i) > 0) {
					elems[idx] = std::make_unique<std::string>(s.substr(i, (j-i)));
				} // else remain NULL
				idx++;
				i = j + 1;
			}
		}

		// if last char is delim, we have a trailing blank string indicating a blank password (not NULL)
		if (s[s.size()-1] == HDURI_DELIM) {
			elems[idx] = std::make_unique<std::string>();
		} else {
			if (i <= s.size()) {
				elems[idx] = std::make_unique<std::string>(s.substr(i));
			}
		}

		// an encoded NULL (%00) denotes an empty password; literal %00 is %2500
		empty_password = elems[HDURI_PASS] && *(elems[HDURI_PASS]) == HDURI_EMPTY_PASSWORD;

		// decode once, in place, so the offset arithmetic below counts URL bytes
		for (idx = 0; idx < HDURI_NUM_ELEMS; idx++) {
			if (!elems[idx] || (idx == HDURI_PASS && empty_password))
				continue;
			if (!decode_hduri_delim(*(elems[idx]), *(elems[idx])))
				return code(HDURI_ERR);
		}
	}

	const bool mailto = (scheme == "mailto");
	ss_url << (mailto ? ":" : "://");
	sz += (mailto ? 1 : 3);

	if (elems[HDURI_USER]) {
		ss_url << *(elems[HDURI_USER]);
		_user_offset = sz;
		_user_len = elems[HDURI_USER]->size();
		sz += _user_len;

		if (elems[HDURI_PASS]) {
			// an encoded NULL (%00) denotes an empty password
			//   http://user:@example.com
			// versus a non-existant password
			//   http://user@example.com
			if (empty_password) { // literal empty password
				ss_url << ':';
				_pass_offset = sz + 1;
				_pass_len = 0;  // literal empty passwd gets offset but zero len
				sz += 1;
			} else {
				_pass_len = elems[HDURI_PASS]->size();
				if (_pass_len > 0) {
					ss_url << ':' << *(elems[HDURI_PASS]);
					_pass_offset = sz + 1;
					sz += _pass_len + 1;
				}
			}
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
		ss_url << *(elems[HDURI_FRAGMENT]);
		_fragment_offset = sz;
		_fragment_len = elems[HDURI_FRAGMENT]->size();
		sz += elems[HDURI_FRAGMENT]->size() + 1;
	}

	/*
	 * Re-encoding must reproduce the canonical fields. Otherwise punctuation in
	 * one field could change another field's meaning during URL reconstruction.
	 */
	const auto id = ss_url.str();
	url check(id);
	if (!check.ok())
		return code(check.code());
	if (check.hduri() != uri)
		return code(HDURI_ERR);

	_id = id;

	// a file: scheme url identifies materialized content;
	// the url object provides the subordinate relation, never the user
	if (this->scheme() == "file") {
		_sub_relator = HARD_TAG_SUB;
		_super_object = HARD_TAG_FILE;
	}
	return code(TAGD_OK);
}

std::string encode_hduri_delim(id_view elem) {
	return percent_encode(elem, [](unsigned char c) {
		return c != '%' && c != HDURI_DELIM;
	});
}

std::string file_url(std::string_view path) {
	// Collection sources and --store paths must produce the same URL identity.
	return "file://" + percent_encode(path, [](unsigned char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'
			|| c == '~' || c == '/';
	});
}

bool decode_hduri_delim(std::string& result, id_view elem) {
	std::string decoded;
	for (size_t i = 0; i < elem.size(); ++i) {
		if (elem[i] == HDURI_DELIM)
			return false;
		if (elem[i] != '%') {
			decoded += elem[i];
			continue;
		}

		const auto escape = elem.substr(i, HDURI_DELIM_ENCODED.size());
		if (escape == HDURI_DELIM_ENCODED) {
			decoded += HDURI_DELIM;
		} else if (escape == HDURI_PERCENT_ENCODED) {
			decoded += '%';
		} else {
			return false;
		}
		// Advancing over source bytes prevents recursively decoding literal percent sequences.
		i += escape.size() - 1;
	}

	result = std::move(decoded);
	return true;
}

// A Hierarchical URI Decomposition (HDURI) is composed of:
//
//  rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
//
//  '%' and '!' in URL parts are encoded as %25 and %21; decode exactly one layer.
//
//  http://sub2.sub1.example.co.uk/a/b/c?id=foo&x=123#summary
//                     ||  ||
//                     \/  \/
//  uk.co!example!sub1.sub2!/a/b/c!?id=foo&x=123!#summary!!!!http
//    |    |        |          |         |         |     ^    |
//  rpub   |       rsub       path     query    fragment ^    |
//       private                                         ^  scheme
//                                                       ^
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

	const auto host = this->host();
	tagd::domain d(host);
	std::string pub = d.pub();
	std::string priv = d.priv_label();
	std::string sub = d.sub();

	/*
	 * Domain classification can trim dots or reject long hosts; URL identity must retain
	 * those bytes. An IP literal is an address, so its dots never separate domain labels.
	 */
	if (host.starts_with('[')
		|| hduri_host(reverse_labels(pub), priv, reverse_labels(sub)) != host) {
		pub.clear();
		priv = host;
		sub.clear();
	}

	auto hduri_elem_f = [&ss](const std::string &elem, bool rev_labels=false) {
		ss << HDURI_DELIM;
		if (!elem.empty())
			ss << encode_hduri_delim(rev_labels ? reverse_labels(elem) : elem);
	};

	if (!pub.empty())
		ss << encode_hduri_delim(reverse_labels(pub));

	hduri_elem_f(priv);
	hduri_elem_f(sub, true);
	hduri_elem_f(this->path());
	hduri_elem_f(this->query());
	hduri_elem_f(this->fragment());
	hduri_elem_f(this->port());
	hduri_elem_f(this->user());

	// we have to distinguish between "no password" and "blank password"
	if (_pass_offset != 0 && _pass_len == 0)
		   ss << HDURI_DELIM << HDURI_EMPTY_PASSWORD; // :<blank pass>
	else
		   hduri_elem_f(this->pass());

	hduri_elem_f(this->scheme());

	return ss.str();
}

// inserts host part relations, returns tld_code from domain::init
void insert_host_parts(tagd::predicate_set& P, const url& u) {
	// An IP literal is an address, not a registered name with public and private labels.
	if (u.host().starts_with('['))
		return;

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

bool url::looks_like_url(const std::string& s) {
	return (s.find("://") != std::string::npos);
}

bool url::looks_like_mailto(const std::string& s) {
	constexpr std::string_view prefix{"mailto:"};
	return s.size() >= prefix.size() && strncasecmp(s.c_str(), prefix.data(), prefix.size()) == 0;
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

bool uri_decode_checked(std::string& result, const std::string& input) {
	for (size_t i = 0; i < input.size(); ++i) {
		if (input[i] != '%')
			continue;
		if (i + 2 >= input.size() || !std::isxdigit(static_cast<unsigned char>(input[i + 1]))
			|| !std::isxdigit(static_cast<unsigned char>(input[i + 2])))
			return false;
		i += 2;
	}
	result = uri_decode(input);
	return result.find('\0') == std::string::npos;
}

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
