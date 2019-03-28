#include<iostream>
#include<string>

#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <cassert>

#include "tagl.h"
#include <event2/buffer.h>

namespace TAGL {

void scanner::print_buf() {
	std::cerr << "print_buf:" << std::endl;
	for(size_t i=0; i<BUF_SZ; ++i) {
		std::cerr << (i % 10);
	}
	std::cerr << std::endl;
	for(size_t i=0; i<BUF_SZ; ++i) {
		std::cerr << _buf[i];
	}
	std::cerr << std::endl;
	std::cerr << "print_buf _beg(" << (_beg-_buf) << "): " << ((int)*_beg) << ", " << *_beg << std::endl;
	std::cerr << "print_buf _cur(" << (_cur-_buf) << "): " << ((int)*_cur) << ", " << *_cur << std::endl;
	std::cerr << "print_buf _lim(" << (_lim-_buf) << "): " << ((int)*_lim) << ", " << *_lim << std::endl;
	if (_eof == nullptr)
		std::cerr << "print_buf _eof: NULL" << std::endl;
	else
		std::cerr << "print_buf _eof(" << (_eof-_buf) << "): " << ((int)*_eof) << ", " << *_eof << std::endl;
	std::cerr << "print_buf _val(" << _val.size() << "): `" << _val << "'" << std::endl;
}

const char* scanner::fill() {
	if (driver::_trace_on) {
		std::cerr << "fill ln: " << _line_number << std::endl;
		std::cerr << "fill _cur(" << (_cur-_buf) << "): " << ((int)*_cur) << ", " << *_cur << std::endl;
		print_buf();
	}

// YYFILL(n)  should adjust YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed.
	if (_cur == nullptr) {
		_eof = _cur;
		if (driver::_trace_on)
			std::cerr << "fill eof. " << std::endl;
	}

	size_t sz, offset;
	assert(_lim >= _beg);
	sz = _lim - _beg;
	assert(sz <= BUF_SZ);

	if (sz >= BUF_SZ) {
		// buffer is full, so overflow into a std::string _val (TODO perhaps an evbuffer)
		// _cur is at the end of the buffer, so append everything up to _cur into _val
		// and copy _cur to the beginning of the buffer for scanning
		auto apnd_sz = BUF_SZ - 1;
		_val.append(_buf, apnd_sz);
		if (driver::_trace_on) {
			std::cerr << "fill _val.append(" << apnd_sz << "): `" << std::string(_buf, apnd_sz) << "'" << std::endl;
			std::cerr << "fill    new _val(" << _val.size() << "): `" << _val << "'" << std::endl;
		}
		_buf[0] = *_cur;
		_beg = _cur = &_buf[0];
		sz = offset = 1;
	} else {
		strncpy(&_buf[0], _beg, sz);
		_cur = &_buf[_cur-_beg];
		_beg = &_buf[0];
		offset = sz;
	}
	_buf[sz] = '\0';
	_mark = _cur;

	if (driver::_trace_on) {
		if (!_val.empty())
			std::cerr << "fill val: `" << _val << "'" << std::endl;
		std::cerr << "fill buf(" << sz << "): `" << std::string(_buf, sz) << "'" << std::endl;
	}

	size_t read_sz = BUF_SZ - offset;
	if (driver::_trace_on) {
		std::cerr << "fill sz: " << sz << std::endl;
		std::cerr << "fill offset: " << offset << std::endl;
		std::cerr << "fill read_sz: " << read_sz << std::endl;
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
			std::cerr << "filled(" << sz << "): `" << std::string(_beg, sz) << "'" << std::endl;
	} else {
		_eof = _lim = &_buf[offset];
	}

	if (driver::_trace_on) print_buf();

	return _cur;
}

void scanner::scan(const char *cur, size_t sz) {

	_beg = _mark = _cur = cur;
	//if (!_do_fill)
	//	_eof = &_cur[sz];

	_lim = &_cur[sz];

// parse token, value not needed
#define PARSE(t) _tok = t; goto parse

// parse token and value
#define PARSE_VALUE(t) _tok = t; goto parse_value

// new string to parse (parser deletes)
#define NEW_VALUE() ( _val.empty() \
		 ? new std::string(_beg, (_cur-_beg)) \
		 : new std::string(_val.append(_beg, (_cur-_beg))) )

#define YYCURSOR _cur
#define YYLIMIT	_lim
#define YYGETSTATE()    _state
#define YYSETSTATE(x)   { _state = (x);  }
#define	YYFILL(n)	{ if(_do_fill && _evbuf && !_eof){ this->fill(); if(_driver->has_errors()) return; } }
#define YYMARKER        _mark
#define YYDEBUG(s,c) { if(driver::_trace_on) std::cerr << "scanner debug: s = " << s << ", c = " << c << ", _cur = " << *_cur <<", _beg = " << *_beg << std::endl; }

next:

	if (_driver->has_errors()) return;

/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	NL			= "\r"? "\n" ;
	ANY			= [^] ;

	URI_SCHEME = [a-zA-Z]+[a-zA-Z0-9.+-]* ":" ;
	SCHEME_SPEC_DATA  = [^\000 \t\r\n'"]+ ;
	SCHEME_SPEC_LCHAR = [^\000 \t\r\n'",;]{1} ;
	URI = URI_SCHEME SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	URL = URI_SCHEME "//" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;

	TAGL_FILE  = [^\000 \t\r\n'"]* ".tagl";

	"--"
	{
		goto comment;
	}

	"-*"
	{
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
		PARSE(TOK_TERMINATOR);
	}

	[ \t]
	{  // whitespace
		_beg = _cur;
		goto next;
	}

	NL
	{	// newline
		_line_number++;
		_beg = _cur;
		goto next;
	}

	"%%"                 { PARSE(TOK_CMD_SET); }
	"<<"                 { PARSE(TOK_CMD_GET); }
	">>"                 { PARSE(TOK_CMD_PUT); }
	"!!"                 { PARSE(TOK_CMD_DEL); }
	"??"                 { PARSE(TOK_CMD_QUERY); }

	"*"                  { PARSE(TOK_WILDCARD); }
	"\"\""               { PARSE(TOK_EMPTY_STR); }
	","                  { PARSE(TOK_COMMA); }
	"="                  { PARSE(TOK_EQ); }
	">"                  { PARSE(TOK_GT); }
	">="                 { PARSE(TOK_GT_EQ); }
	"<"                  { PARSE(TOK_LT); }
	"<="                 { PARSE(TOK_LT_EQ); }
	";"                  { PARSE(TOK_TERMINATOR); }

	"-"? [0-9]+ ("." [0-9]+)?
					     {
	                        PARSE_VALUE(TOK_QUANTIFIER);
	                     }

	URL                  {  PARSE_VALUE(TOK_URL); }

	URI                  {  goto lookup_parse_uri; }

	TAGL_FILE            {  goto lookup_tagl_file; }

	[^\000 \t\r\n;,=><'"-]+  { goto lookup_parse; }

	[\000]               { return; }

	[^]                  { // ANY
							_driver->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, std::string(_beg,(_cur-_beg))));
							if (!_driver->path().empty()) {
								_driver->last_error_relation(
									tagd::predicate(HARD_TAG_CAUSED_BY, "_file", _driver->path()) );
							}
							if (_line_number) {
								_driver->last_error_relation(
									tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(_line_number)) );
							}
	                        return;
	                     }

*/ // end re2c

	return;

comment:
/*!re2c
	NL			{
					_line_number++;
					_beg = _cur;
					goto next;
				}
	[\000]      { return; }
	ANY			{	_beg = _cur; goto comment; }
*/

block_comment:
/*!re2c
	"*-"		{
					_beg = _cur;
					goto next;
				}
	NL			{ 	_beg = _cur; _line_number++; goto block_comment; }
	[\000]      { _driver->error(tagd::TAGL_ERR, "unclosed block comment"); return; }
	ANY			{ 	_beg = _cur; goto block_comment; }
*/

parse:
	_driver->parse_tok(_tok, NULL);
	_beg = _cur;
	if(!_val.empty()) _val.clear();
	goto next;

parse_value:
	_driver->parse_tok(_tok, NEW_VALUE());
	if(!_val.empty()) _val.clear();
	_beg = _cur;
	goto next;

quoted_str:
/*!re2c
	"\\\""		{ goto quoted_str; /* escaped */ }
	"\""		{ goto parse_quoted_str; }
	NL			{ _line_number++; goto quoted_str; }
	ANY			{ goto quoted_str; }
*/

parse_quoted_str:
	// _quoted_str uses by parser instead of new string pointer - parser clears
	sz = (_cur-_beg);
	_val.append(_beg, sz);
	// parse the value in quotes, not the quotes themselves
	int tok;
	switch(_driver->_token) {
		case TOK_CMD_QUERY:
		case TOK_INCLUDE:
			tok = TOK_QUOTED_STR;  // don't lookup, just pass through
			break;
		default:
			tok = _driver->lookup_pos(_val.substr(1, sz-2));
	}
	_driver->parse_tok(
		tok, new std::string(_val.substr(1, sz-2))  // parser deletes
	);
	_val.clear();
	_beg = _cur;
	goto next;

lookup_parse:
	{
		std::string *val = NEW_VALUE();
		_driver->parse_tok(_driver->lookup_pos(*val), val);
	}
	_beg = _cur;
	goto next;

lookup_parse_uri:
	{
		std::string *val = NEW_VALUE();
		auto pos = _driver->lookup_pos(*val);
		/* an HDURI is a special URI that represents a URL
		   treat any other URI just like other tokens */
		_driver->parse_tok((pos == TOK_URL ? TOK_HDURI : pos), val);
	}
	_beg = _cur;
	goto next;

lookup_tagl_file:
	{
		std::string *val = NEW_VALUE();
		_driver->parse_tok(TOK_TAGL_FILE, val);
	}
	_beg = _cur;
	goto next;

} // end scan

} // namespace TAGL


