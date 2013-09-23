#include<iostream>
#include<string>

#include <cstring>
#include <unistd.h>
#include <stdio.h>

#include "tagl.h"
#include "taglparser.h"

namespace TAGL {

void scanner::scan(const char *p) {

	const char *beg = p;
	const char *mark;
	std::string *value;
	int tok;

// parse token, value not needed
#define PARSE(t) tok = t; goto parse

// parse token and value
#define PARSE_VALUE(t) tok = t; goto parse_value

#define YYMARKER        mark

next:

	if (_driver->code() == tagd::TAGL_ERR) return;

/*!re2c
        re2c:define:YYCTYPE  = "unsigned char";
        re2c:define:YYCURSOR = p;
        re2c:yyfill:enable   = 0;
        re2c:yych:conversion = 1;
        re2c:indent:top      = 1;

		";"                  { PARSE(TERMINATOR); }
		([ \t\r]*[\n]){2,}   { PARSE(TERMINATOR); }

		[ \t\n\r]		     {  // SPACE
								beg = p;
								goto next;
							 }

		","                  { PARSE(SEPARATOR); }
		
		[Ss][Ee][Tt]         { PARSE(CMD_SET); }
		[Gg][Ee][Tt]         { PARSE(CMD_GET); }
		[Pp][Uu][Tt]         { PARSE(CMD_PUT); }
		[Qq][Uu][Ee][Rr][Yy] { PARSE(CMD_QUERY); }

		"*"                  { PARSE(WILDCARD); }
		"\"\""               { PARSE(EMPTY_STR); }

        [0-9]+               {
							   // TODO NUM;
							   PARSE_VALUE(QUANTIFIER);
						     }

		[a-zA-Z]+[a-zA-Z.+-]* "://" [^ \t\r\n;,'"]+
							 {  PARSE_VALUE(URL); }

		[^\000 \t\r\n;,'"]+  { goto lookup_parse; }


        [^]                  {
							   // std::cout << "ANY" << std::endl;
							   return;
						     }

*/ // end re2c

	return;

parse:
	_driver->parse_tok(tok, NULL);
	beg = p;
	goto next;

parse_value:
	value = new std::string(beg, (p-beg));  // parser deletes
	_driver->parse_tok(tok, value);
	beg = p;
	goto next;

lookup_parse:
	value = new std::string(beg, (p-beg));  // parser deletes
	_driver->parse_tok(_driver->lookup_pos(*value), value);
	beg = p;
	goto next;

}


void scanner::scan_tagurl_path(int cmd, const std::string& path) {
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
		_driver->parse_tok(INTERROGATOR, (new std::string(HARD_TAG_INTERROGATOR)));
		_driver->parse_tok(SUPER, (new std::string(HARD_TAG_SUPER)));
	} else {
		_driver->parse_tok(cmd, NULL);
	}

	this->scan(segment.c_str());

	if (++sep_i >= num_seps)
		return;	

	// second segment
	segment = path.substr(seps[sep_i]+1);

	if (!segment.empty()) {
		_driver->parse_tok(WILDCARD, NULL);
		this->scan(segment.c_str());
	}
}

} // namespace TAGL


