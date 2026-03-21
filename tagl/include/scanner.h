#ifndef TAGL_SCANNER_H
#define TAGL_SCANNER_H

#include <cstdint>
#include <string>

struct evbuffer;

namespace TAGL {

class driver;

class scanner {
	friend class TAGL::driver;

	protected:
		driver *_driver;

		const char
			*_beg  = nullptr ,
			*_cur  = nullptr ,
			*_mark = nullptr ,
			*_lim  = nullptr ,
			*_eof  = nullptr ;

		size_t _line_number = 0; // current line
		int _tok = -1;
		int32_t _state = -1;
		char *_buf = nullptr;
		std::string _val;  // holds _buf overflow
		evbuffer *_evbuf = nullptr;
		bool _do_fill = false;

		void begin_scan(const char*, size_t);
		void clear_value();
		void advance_begin();
		std::string* new_value();
		void next_line(size_t=1);

		// emit parser token with unknown semantic value.
		void emit(int tok, std::string *val=nullptr);

		// emit scanner error for unrecognized tokens, or other errors
		void emit_error();

		// emit a tagd POS looked-up token
		void emit_tagd_pos_lookup();

		// emit a looked-up URI token, remapping URL lookups to `TOK_HDURI`.
		void emit_lookup_uri_token();

		// Emit a `TOK_TAGL_FILE` token for include-like file references.
		void emit_tagl_file_token();

		// Emit the contents of a quoted string without the quotes.
		void emit_quoted_string_token();

	public:
		class tagdurl;

		scanner(driver *d);
		virtual ~scanner();

		const char* fill();
		void scan(const std::string& s) { this->scan(s.c_str(), s.size()); }
		// Scan input and emit parser tokens through the driver contract.
		virtual void scan(const char*, size_t);
		void scan(const char*) = delete; // don't allow implicit conversion to std::string
		void evbuf(evbuffer *ev) { _evbuf = ev; _do_fill = true; }
		void print_buf();

		size_t line_number();

		virtual void reset();
};

class scanner::tagdurl : public scanner {
	public:
		tagdurl(driver *d) : scanner(d) {}
		void scan(const char*, size_t) override;
};

} // namespace TAGL

#endif  // TAGL_SCANNER_H

/*!rules:re2c:tagl_defs
	NL			= "\r"? "\n" ;
	ANY			= [^] ;

	URI_SCHEME = [a-zA-Z]+[a-zA-Z0-9.+-]* ":" ;
	SCHEME_SPEC_DATA  = [^\000 \t\r\n'"]+ ;
	SCHEME_SPEC_LCHAR = [^\000 \t\r\n'",;]{1} ;
	URI = URI_SCHEME SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	URL = URI_SCHEME "//" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	HDURI = "hd:" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;

	TAGL_FILE  = [^\000 \t\r\n'"]* ".tagl";
*/

/*!rules:re2c:tagl_root
	!use:tagl_defs;

	"--"                 { goto comment; }
	"-*"                 { goto block_comment; }

	"\\\""               { _driver->error(tagd::TAGL_ERR, "escaped double quote"); return; }
	"\""                 { goto quoted_str; }

	([ \t\r]*[\n]){2,}   { next_line(2); emit(TOK_TERMINATOR); goto next; }

	[ \t]                { advance_begin(); goto next; }
	NL                   { next_line(); advance_begin(); goto next; }

	"%%"                 { emit(TOK_CMD_SET); goto next; }
	"<<"                 { emit(TOK_CMD_GET); goto next; }
	">>"                 { emit(TOK_CMD_PUT); goto next; }
	"!!"                 { emit(TOK_CMD_DEL); goto next; }
	"??"                 { emit(TOK_CMD_QUERY); goto next; }

	"*"                  { emit(TOK_WILDCARD); goto next; }
	"\"\""               { emit(TOK_EMPTY_STR); goto next; }
	","                  { emit(TOK_COMMA); goto next; }
	"="                  { emit(TOK_EQ); goto next; }
	">"                  { emit(TOK_GT); goto next; }
	">="                 { emit(TOK_GT_EQ); goto next; }
	"<"                  { emit(TOK_LT); goto next; }
	"<="                 { emit(TOK_LT_EQ); goto next; }
	";"                  { emit(TOK_TERMINATOR); goto next; }

	"-"? [0-9]+ ("." [0-9]+)?
	                     { emit(TOK_QUANTIFIER, new_value()); goto next; }

	URL                  { emit(TOK_URL, new_value()); goto next; }
	HDURI                { emit(TOK_HDURI, new_value()); goto next; }
	URI                  { emit_lookup_uri_token(); goto next; }
	TAGL_FILE            { emit_tagl_file_token(); goto next; }
	[^\000 \t\r\n;,=><'"-]+
	                     { emit_tagd_pos_lookup(); goto next; }

	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/

/*!rules:re2c:tagl_comment
	!use:tagl_defs;

	NL                   { next_line(); advance_begin(); goto next; }
	[\000]               { return; }
	ANY                  { advance_begin(); goto comment; }
*/

/*!rules:re2c:tagl_block_comment
	!use:tagl_defs;

	"*-"                 { advance_begin(); goto next; }
	NL                   { next_line(); advance_begin(); goto block_comment; }
	[\000]               { _driver->error(tagd::TAGL_ERR, "unclosed block comment"); return; }
	ANY                  { advance_begin(); goto block_comment; }
*/

/*!rules:re2c:tagl_quoted
	!use:tagl_defs;

	"\\\""               { goto quoted_str; }
	"\""                 { emit_quoted_string_token(); goto next; }
	NL                   { next_line(); goto quoted_str; }
	ANY                  { goto quoted_str; }
*/
