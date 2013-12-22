#include<iostream>
#include<string>

#include <cstring>
#include <unistd.h>
#include <stdio.h>

#include "tagl.h"

namespace TAGL {

inline size_t eat_until_eol(const char *cur) {
	const char *start = cur;

	while(*cur != '\0' && *cur != '\n')
		++cur;	

	return (cur - start);
}


size_t scanner::process_block_comment(const char *cur) {
	const char *start = cur;
	_block_comment_open = true;

	while(*cur != '\0') {
		if (*cur == '\n')
			_line_number++;
		// close of block comment "*-"
		if (*(cur++) == '*' && *(cur++) == '-') {
			_block_comment_open = false;
			break;
		}
	}

	return (cur - start);
}

size_t scanner::process_double_quotes(const char *cur) {
	const char *start = cur;
	_double_quotes_open = true;

	while(*cur != '\0') {
		if (*cur == '\n')
			_line_number++;
		if (*cur == '\\' && *(cur+1) == '"')  { // escaped double quote
			cur += 2;
			continue;
		}
		if (*(cur++) == '"') {
			_double_quotes_open = false;
			break;
		}
	}

	size_t sz = (cur - start);
	if (sz > 0) {
		if (_double_quotes_open)
			_quoted_str.append(start, sz);
		else	// nip the '"' off the end
			_quoted_str.append(start, (sz-1));
	}

	return sz;
}


void scanner::scan(const char *cur) {

	const char *beg = cur;
	const char *mark;
	int tok;

// parse token, value not needed
#define PARSE(t) tok = t; goto parse

// parse token and value
#define PARSE_VALUE(t) tok = t; goto parse_value

#define YYMARKER        mark

	// this may be a continuation of the last scan,
	// which may have ended in the middle of a block comment
	if (_block_comment_open) {
		cur += this->process_block_comment(cur);
		if (_driver->_trace_on)
			std::cerr << "comment: `" << std::string(beg, (cur-beg)) << "'" << std::endl;
		beg = cur;
	}

	// last scan may have ended in the middle of a double quoted string
	if (_double_quotes_open) {
		cur += this->process_double_quotes(cur);
		if (!_double_quotes_open)
			goto parse_quoted_str;
	}

next:

	if (_driver->has_error()) return;

/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:define:YYCURSOR = cur;
	re2c:yyfill:enable   = 0;
	re2c:yych:conversion = 1;
	re2c:indent:top      = 1;

	"--"
	{	// comment
		cur += eat_until_eol(cur);
		if (_driver->_trace_on) 
			std::cerr << "comment: `" << std::string(beg, (cur-beg)) << "'" << std::endl;
		if (*cur == '\n') {
			cur++;
			_line_number++;
		}
		beg = cur;
		goto next;
	}

	"-*"
	{	// block comment
		cur += this->process_block_comment(cur);
		if (_driver->_trace_on)
			std::cerr << "comment: `" << std::string(beg, (cur-beg)) << "'" << std::endl;
		beg = cur;
		goto next;
	}

	"\""
	{	// double quoted string
		cur += this->process_double_quotes(cur);
		if (!_double_quotes_open)
			goto parse_quoted_str;
	}

	([ \t\r]*[\n]){2,}
	{   // TERMINATOR
		// increment line_number
		for (const char *p = beg; p <= cur; ++p) {
			if (*p == '\n')
				_line_number++;
		}
		PARSE(TERMINATOR);
	}

	[ \t\r]
	{  // whitespace
		beg = cur;
		goto next;
	}

	"\n"
	{	// newline
		_line_number++;
		beg = cur;
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
	"="                  { PARSE(EQUALS); }
	";"                  { PARSE(TERMINATOR); }

	[^\000 \t\r\n;,='"-]+  { goto lookup_parse; }

	[\000]               { return; }

	[^]                  { // ANY
	                        return;
	                     }

*/ // end re2c

	return;

parse:
	_driver->parse_tok(tok, NULL);
	beg = cur;
	goto next;

parse_quoted_str:
	// _quoted_str uses by parser instead of new string pointer - parser clears
	_driver->parse_tok(QUOTED_STR, NULL);
	beg = cur;
	goto next;

parse_value:
	_driver->parse_tok(
		tok,
		new std::string(beg, (cur-beg))  // parser deletes
	);
	beg = cur;
	goto next;

lookup_parse:
	{
		// parser deletes value
		std::string *value = new std::string(beg, (cur-beg));
		_driver->parse_tok(_driver->lookup_pos(*value), value);
	}
	beg = cur;
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


