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
	std::cerr << std::endl;
	for(size_t i=0; i<buf_sz; ++i) {
		std::cerr << (i % 10);
	}
	std::cerr << std::endl;
	for(size_t i=0; i<buf_sz; ++i) {
		std::cerr << _buf[i];
	}
	std::cerr << std::endl;
	std::cerr << "_beg(" << (_beg-_buf) << "): " << ((int)*_beg) << ", " << *_beg << std::endl;
	std::cerr << "_cur(" << (_cur-_buf) << "): " << ((int)*_cur) << ", " << *_cur << std::endl;
	std::cerr << "_lim(" << (_lim-_buf) << "): " << ((int)*_lim) << ", " << *_lim << std::endl;
	if (_eof == nullptr)
		std::cerr << "_eof: NULL" << std::endl;
	else
		std::cerr << "_eof(" << (_eof-_buf) << "): " << ((int)*_eof) << ", " << *_eof << std::endl;
	std::cerr << "_val(" << _val.size() << "): `" << _val << "'" << std::endl;
}

const char* scanner::fill() {
	if (driver::_trace_on)
		std::cerr << "ln: " << _line_number << " ,fill() _cur: `" << *_cur << "'" << std::endl;

	if (driver::_trace_on) print_buf();

// YYFILL(n)  should adjust YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed.
	if (_cur == '\0') {
		_eof = _cur;
		if (driver::_trace_on)
			std::cerr << "fill eof. " << std::endl;
	}

	size_t sz, offset;
	assert(_lim >= _beg);
	sz = _lim - _beg;

	if (sz >= buf_sz) {
		_val.append(_buf, buf_sz);
		_beg = _cur = &_buf[0];
		sz = offset = 0;
	} else {
		strncpy(&_buf[0], _beg, sz);
		_cur = &_buf[_cur-_beg];
		_beg = &_buf[0];
		offset = sz;
		_buf[sz] = '\0';
	}
	_mark = _cur;

	if (driver::_trace_on) {
		if (!_val.empty())
			std::cerr << "val: `" << _val << "'" << std::endl;
		std::cerr << "buf(" << sz << "): `" << std::string(_buf, sz) << "'" << std::endl;
	}

	size_t read_sz = buf_sz - offset;
	if (driver::_trace_on) {
		std::cerr << "sz: " << sz << std::endl;
		std::cerr << "offset: " << offset << std::endl;
		std::cerr << "read_sz: " << read_sz << std::endl;
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

#define YYCURSOR _cur
#define YYLIMIT	_lim
#define YYGETSTATE()    _state
#define YYSETSTATE(x)   { _state = (x);  }
#define	YYFILL(n)	{ if(_do_fill && _evbuf && !_eof){ this->fill(); if(_driver->has_error()) return; } }
#define YYMARKER        _mark
#define YYDEBUG(s,c) { if(driver::_trace_on) std::cerr << "scanner debug: s = " << s << ", c = " << c << ", _cur = " << *_cur <<", _beg = " << *_beg << std::endl; }

next:

	if (_driver->has_error()) return;

/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	NL			= "\r"? "\n" ;
	ANY			= [^] ;

	URL_SCHEME = [a-zA-Z]+[a-zA-Z.+-]* "://" ;
	SCHEME_SPEC_DATA  = [^\000 \t\r\n'"]+ ;
	SCHEME_SPEC_LCHAR = [^\000 \t\r\n'",]{1} ;
	URL = URL_SCHEME SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ; 

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
		PARSE(TERMINATOR);
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
		
	"%%"                 { PARSE(CMD_SET); }
	"<<"                 { PARSE(CMD_GET); }
	">>"                 { PARSE(CMD_PUT); }
	"!!"                 { PARSE(CMD_DEL); }
	"??"                 { PARSE(CMD_QUERY); }

	"*"                  { PARSE(WILDCARD); }
	"\"\""               { PARSE(EMPTY_STR); }
	","                  { PARSE(COMMA); }
	"="                  { PARSE(EQ); }
	">"                  { PARSE(GT); }
	">="                 { PARSE(GT_EQ); }
	"<"                  { PARSE(LT); }
	"<="                 { PARSE(LT_EQ); }
	";"                  { PARSE(TERMINATOR); }

	"-"? [0-9]+ ("." [0-9]+)?
					     { 
	                        PARSE_VALUE(QUANTIFIER);
	                     }

	URL                  {  PARSE_VALUE(URL); }

	[^\000 \t\r\n;,=><'"-]+  { goto lookup_parse; }

	[\000]               { return; }

	[^]                  { // ANY
							_driver->error(tagd::TAGL_ERR, tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, std::string(_beg,(_cur-_beg))));
							if (!_driver->filename().empty()) {
								_driver->last_error_relation(
									tagd::make_predicate(HARD_TAG_CAUSED_BY, "_file", _driver->filename()) );
							}
							if (_line_number) {
								_driver->last_error_relation(
									tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(_line_number)) );
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
	_driver->parse_tok(
		_tok,
		( _val.empty()
		 ? new std::string(_beg, (_cur-_beg))
		 : new std::string(_val.append(_beg, (_cur-_beg)))
		)  // parser deletes
	);
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
	_driver->parse_tok(
		( _driver->_token == CMD_QUERY
			? QUOTED_STR	// indicates search
			: _driver->lookup_pos(_val.substr(1, sz-2))
		),
		new std::string(_val.substr(1, sz-2))  // parser deletes
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

} // namespace TAGL


