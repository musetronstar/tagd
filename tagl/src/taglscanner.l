%{
#include "taglparser.h"
#include "tagl.h"

int scanner_pos(TAGL::driver *tagl, const char *s) {
	int token;
	switch(tagl->_TS->pos(s)) {
		case tagd::POS_TAG:
			token = TAG;
			break;
		case tagd::POS_SUPER:
			token = SUPER;
			break;
		case tagd::POS_RELATOR:
			token = RELATOR;
			break;
		case tagd::POS_INTERROGATOR:
			token = INTERROGATOR;
			break;
		default:  // POS_UNKNOWN
			token = UNKNOWN;
	}

	if ( !tagl->_relator.empty()  // in a predicate list
		 && token != RELATOR      // relator starts a new predicate list
	     && tagl->_token == TAG ) // last token a TAG, so this must be a quantifier
	{
		return QUANTIFIER;
	}

	return token;
}
%}
 
%option reentrant
%option extra-type="TAGL::driver *"
%option noyywrap
 
%x SINGLE_QUOTED
%x DOUBLE_QUOTED

%%

[;]  {
	return TERMINATOR;
}

([ \t\r]*[\n]){2,}  {
	// terminated by 2 or more blank lines ignoring whitespace
	return TERMINATOR;
}

[ \t\n\r] { /* throw away whitespace */ }

[,]		  { return SEPARATOR; }
 
[']                     { BEGIN(SINGLE_QUOTED); }
<SINGLE_QUOTED>[^']+    {}
<SINGLE_QUOTED>[']      { BEGIN(INITIAL); return scanner_pos(yyextra, yytext); }
<SINGLE_QUOTED><<EOF>>  { return -1; }
 
["]                     { BEGIN(DOUBLE_QUOTED); }
<DOUBLE_QUOTED>[^"]+    {}
<DOUBLE_QUOTED>["]      { BEGIN(INITIAL); return scanner_pos(yyextra, yytext); }
<DOUBLE_QUOTED><<EOF>>  { return -1; }

GET     { return CMD_GET; }
PUT     { return CMD_PUT; }
QUERY   { return CMD_QUERY; }
 
[a-zA-Z]+[a-zA-Z.+-]*:\/\/[^ \t\r\n;,'"]+  {  return URL; }

[^ \t\r\n;,'"]+    {  return scanner_pos(yyextra, yytext); }

%%
