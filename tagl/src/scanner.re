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

	if (_driver->code() == TAGL_ERR) return;

/*!re2c
        re2c:define:YYCTYPE  = "unsigned char";
        re2c:define:YYCURSOR = p;
        re2c:yyfill:enable   = 0;
        re2c:yych:conversion = 1;
        re2c:indent:top      = 1;

		";"                  { PARSE(TERMINATOR); }
		([ \t\r]*[\n]){2,}   { PARSE(TERMINATOR); }

		[ \t\n\r]		     {
								// std::cout << "SPACE" << std::endl;
								beg = p;
								goto next;
							 }

		","                  { PARSE(SEPARATOR); }
		
		"GET"                { PARSE(CMD_GET); }
		"PUT"                { PARSE(CMD_PUT); }
		"QUERY"              { PARSE(CMD_QUERY); }

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

*/

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

} // namespace TAGL


