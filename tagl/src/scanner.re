#include<iostream>
#include<string>

#include <cstring>
#include <unistd.h>
#include <stdio.h>

#include "tagl.h"
#include <event2/buffer.h>

namespace TAGL {

const char* scanner::fill() {
// YYFILL(n)  should adjust YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed.
	if (_cur == '\0') {
		_eof = _cur;
		if (driver::_trace_on)
			std::cout << "fill eof. " << std::endl;
	}

	if (_eof) return _cur;

	size_t sz, offset;
	sz = _lim - _beg;
	if (sz > (buf_sz/2)) {
		std::cerr << "append(" << sz << ")" << std::endl;
		if (!_ignore)
			_val.append(_beg, sz);
		_beg = _cur = &_buf[0];
		offset = 0;
	} else {
		strncpy(&_buf[0], _beg, sz);
		_cur = &_buf[_cur-_beg];
		_beg = &_buf[0];
		offset = sz;
	}
	_buf[sz] = '\0';

	if (driver::_trace_on) {
		if (!_val.empty())
			std::cout << "val: `" << _val << "'" << std::endl;
		std::cout << "buf(" << sz << "): `" << std::string(_buf, sz) << "'" << std::endl;
	}

	size_t read_sz = buf_sz - offset;
	if (driver::_trace_on) {
		std::cout << "sz: " << sz << std::endl;
		std::cout << "offset: " << offset << std::endl;
		std::cout << "read_sz: " << read_sz << std::endl;
	}

	if ((sz = evbuffer_remove(_evbuf, &_buf[offset], read_sz)) != 0) {
		if (sz < read_sz) {
			sz += offset;
			if (_buf[sz] != '\0')
				_buf[sz] = '\0';
			_eof = &_buf[sz];
		} else {
			sz += offset;
		}
		_lim = &_buf[sz];
		if (driver::_trace_on)
			std::cout << "filled(" << sz << "): `" << std::string(_beg, sz) << "'" << std::endl;
	} else {
		_eof = _lim = &_buf[offset];
	}

	return _cur;
}

void scanner::scan(const char *cur, size_t sz) {
	
	const char *mark; 
	_beg = _cur = cur;
	if (!_do_fill)
		_eof = &_cur[sz];

	_lim = &_cur[sz];

// parse token, value not needed
#define PARSE(t) _tok = t; goto parse

// parse token and value
#define PARSE_VALUE(t) _tok = t; goto parse_value

#define YYCURSOR _cur
#define YYLIMIT	_lim
#define YYGETSTATE()    _state
#define YYSETSTATE(x)   { _state = (x);  }
#define	YYFILL(n)	{ if(_do_fill && _evbuf && !_eof){this->fill();} }
#define YYMARKER        mark

next:

	if (_driver->has_error()) return;

/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	NL			= "\r"? "\n" ;
	ANY			= [^] ;

	"--"
	{
		_ignore = true;
		goto comment;
	}

	"-*"
	{
		_ignore = true;
		goto block_comment;
	}

	"\\\""
	{
		_driver->error(tagd::TAGL_ERR, "escaped double quote");
		return;
	}

	"\""
	{
		goto quoted_str;
	}
	
	([ \t\r]*[\n]){2,}
	{   // TERMINATOR
		// increment line_number
		for (const char *p = _beg; p <= _cur; ++p) {
			if (*p == '\n')
				_line_number++;
		}
		PARSE(TERMINATOR);
	}

	[ \t\r]
	{  // whitespace
		_beg = _cur;
		goto next;
	}

	"\n"
	{	// newline
		_line_number++;
		_beg = _cur;
		goto next;
	}

	[a-zA-Z]+[a-zA-Z.+-]* "://" [^\000 \t\r\n'"]+
	                         {  PARSE_VALUE(URL); }
		
	[Ss][Ee][Tt]             { PARSE(CMD_SET); }
	[Gg][Ee][Tt]             { PARSE(CMD_GET); }
	[Pp][Uu][Tt]             { PARSE(CMD_PUT); }
	[Dd][Ee][Ll][Ee][Tt][Ee] { PARSE(CMD_DEL); }
	[Dd][Ee][Ll]             { PARSE(CMD_DEL); }
	[Qq][Uu][Ee][Rr][Yy]     { PARSE(CMD_QUERY); }

	[0-9]+               { // TODO NUM;
	                        PARSE_VALUE(QUANTIFIER);
	                     }

	"*"                  { PARSE(WILDCARD); }
	"\"\""               { PARSE(EMPTY_STR); }
	","                  { PARSE(COMMA); }
	"="                  { PARSE(EQ); }
	">"                  { PARSE(GT); }
	">="                 { PARSE(GT_EQ); }
	"<"                  { PARSE(LT); }
	"<="                 { PARSE(LT_EQ); }
	";"                  { PARSE(TERMINATOR); }

	[^\000 \t\r\n;,=><'"-]+  { goto lookup_parse; }

	[\000]               { return; }

	[^]                  { // ANY
	                        return;
	                     }

*/ // end re2c

	return;

comment:
/*!re2c
	NL			{
					_ignore = false;
					/*if (driver::_trace_on) {
						tok = "COMMENT";
						goto parse;
					} else {*/
						_beg = _cur;
						goto next;
					//}
				}
	ANY			{ goto comment; }
*/

block_comment:
/*!re2c
	"*-"		{
					_ignore = false;
					/*if (driver::_trace_on) {
						tok = "BLOCK_COMMENT";
						goto parse;
					} */
					_beg = _cur;
					goto next;
				}
	[\000]      { _driver->error(tagd::TAGL_ERR, "unclosed block comment"); return; }
	ANY			{ goto block_comment; }
*/

parse:
	_driver->parse_tok(_tok, NULL);
	_beg = _cur;
	goto next;

parse_value:
	_driver->parse_tok(
		_tok,
		( _val.empty()
		 ? new std::string(_beg, (_cur-_beg))
		 : new std::string(_val.append(_beg, (_cur-_beg)))
		)  // parser deletes
	);
	_val.clear();
	_beg = _cur;
	goto next;

quoted_str:
/*!re2c
	"\\\""		{ goto quoted_str; /* escaped */ }
	"\""		{ goto parse_quoted_str; }
	ANY			{ goto quoted_str; }
*/

parse_quoted_str:
	// _quoted_str uses by parser instead of new string pointer - parser clears
	_beg++;
	sz = (_cur-_beg)-1;
	_val.append(_beg, sz);
	_driver->parse_tok(
		_driver->lookup_pos(_val),
		new std::string(_val)  // parser deletes
	);
	_val.clear();
	_beg = _cur;
	goto next;

lookup_parse:
	{
		// parser deletes value
		std::string *value = ( _val.empty()
		 ? new std::string(_beg, (_cur-_beg))
		 : new std::string(_val.append(_beg, (_cur-_beg)))
		);  // parser deletes
		_driver->parse_tok(_driver->lookup_pos(*value), value);
	}
	_beg = _cur;
	goto next;
}


void scanner::scan_tagdurl_path(int cmd, const std::string& path, const url_query_map_t* qm) {
	// path separator defs
	const size_t max_seps = 2;
	size_t sep_i = 0;
	size_t seps[max_seps] = {0, 0};  // offsets '/' chars in path

	if (path[0] != '/') {
		_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
		return;
	}

	for(size_t i = 0; i < path.size(); i++) {
		if (path[i] == '/') {
			if (sep_i == max_seps) {
				// TODO use error tag
				_driver->error(tagd::TAGL_ERR, "max_seps exceeded");
				return;
			}
			seps[sep_i++] = i;
		}
	}

	size_t num_seps = sep_i;

	if (cmd == CMD_PUT && num_seps > 1) {
		_driver->error(tagd::TAGL_ERR, "malformed path: trailing '/'");
		return;
	}
	
	// path segment
	std::string segment;

	// first segment
	sep_i = 0;
	if (seps[sep_i+1]) {
		size_t sz = seps[sep_i+1] - seps[sep_i] - 1;
		segment = path.substr((seps[sep_i]+1), sz);
	}
	else {
		// get
		segment = path.substr(seps[sep_i]+1);
	}

	if (cmd == CMD_GET && num_seps > 1) {
		cmd = CMD_QUERY;
		_driver->parse_tok(cmd, NULL);
		_driver->parse_tok(INTERROGATOR, (new std::string(HARD_TAG_INTERROGATOR)));  // parser deletes

		// first segment of "*" is a placeholder for super relation, so ignore it
		if (segment != "*") {
			_driver->parse_tok(SUPER_RELATOR, (new std::string(HARD_TAG_SUPER)));
			this->scan(tagd::uri_decode(segment).c_str());
		}
	} else {
		_driver->parse_tok(cmd, NULL);
		this->scan(tagd::uri_decode(segment).c_str());
	}

	if (++sep_i >= num_seps)
		return;	

	// second segment
	segment = path.substr(seps[sep_i]+1);

	if (!segment.empty()) {
		_driver->parse_tok(WILDCARD, NULL);
		this->scan(tagd::uri_decode(segment).c_str());
	}
}

} // namespace TAGL


