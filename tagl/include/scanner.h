#ifndef TAGL_SCANNER_H
#define TAGL_SCANNER_H

#include <cstdint>
#include <deque>
#include <string>

struct evbuffer;

namespace TAGL {

const std::string QUERY_OPT_SEARCH{"q"};    // full text search
const std::string QUERY_OPT_VIEW{"v"};      // view name
const std::string QUERY_OPT_CONTEXT{"c"};   // tagspace context

class driver;

// Parse-lifetime backing for token text that must outlive scanner buffer refill.
class token_store {
	protected:
		// deque keeps string object addresses stable so emitted text_token views stay valid.
		std::deque<std::string> _tokens;

	public:
		const std::string& store(const char *z, size_t n) {
			_tokens.emplace_back(z, n);
			return _tokens.back();
		}

		const std::string& store(const std::string& s) {
			_tokens.push_back(s);
			return _tokens.back();
		}

		text_token store_text(const char *z, size_t n) {
			const std::string& s = store(z, n);
			return text_token{s.c_str(), (int)s.size()};
		}

		text_token store_text(const std::string& s) {
			const std::string& stored = store(s);
			return text_token{stored.c_str(), (int)stored.size()};
		}

		void clear() { _tokens.clear(); }
		bool empty() const { return _tokens.empty(); }
		size_t size() const { return _tokens.size(); }
	};

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
		// The scanner owns this refill buffer because re2c works on mutable contiguous memory.
		char *_buf = nullptr;
		std::string _val;  // holds _buf overflow
		token_store _token_store;  // parse-lifetime backing for emitted text_token slices
		evbuffer *_evbuf = nullptr;
		bool _do_fill = false;

		void begin_scan(const char*, size_t);
		void clear_value();
		void advance_begin();
		text_token store_token_text();
		const std::string& store_value();
		text_token new_value();
		void next_line(size_t=1);
		void log_begin(size_t);
		void log_token(int, text_token);
		void log_refill(size_t, size_t, size_t, bool);
		void log_error(const std::string&);

		// emit parser token with unknown semantic value.
		void emit(int tok, text_token val=EMPTY_VALUE);

		// emit parser token with given value (not _val)
		void emit_literal_value(int tok, const char *);

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
		scanner(const scanner&) = delete;
		scanner& operator=(const scanner&) = delete;
		scanner(scanner&&) = delete;
		scanner& operator=(scanner&&) = delete;

		const char* fill();
		void scan(const std::string& s) { this->scan(s.c_str(), s.size()); }
		// Scan input and emit parser tokens through the driver contract.
		virtual void scan(const char*, size_t);
		void scan(const char*) = delete; // don't allow implicit conversion to std::string
		void evbuf(evbuffer *ev) { _evbuf = ev; _do_fill = true; }

		size_t line_number();

		virtual void reset();
};

class scanner::tagdurl : public scanner {
	public:
		tagdurl(driver *d) : scanner(d) {}
		void scan(const char*, size_t) override;
		void scan(int cmd, const std::string& path);
		void scan(tagd::http_method, const std::string& path);

		// accapt HTTP method, returns TOK_CMD_*
		static int method_command(tagd::http_method);
};

} // namespace TAGL

#endif  // TAGL_SCANNER_H

/*!rules:re2c:tagl_defs
	NL			= "\r"? "\n" ;
	ANY			= [^] ;

	URI_SCHEME = [a-zA-Z]+[a-zA-Z0-9.+-]* ":" ;
	SCHEME_SPEC_DATA  = [^\000 \t\r\n'"]+ ;
	SCHEME_SPEC_LCHAR = [^\000 \t\r\n'",;]{1} ;

	TAGDURL = "/" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR; 

	URI = URI_SCHEME SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	URL = URI_SCHEME "//" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	HDURI = "hd:" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	EVURI = "ev:" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;
	ERRURI = "err:" SCHEME_SPEC_DATA SCHEME_SPEC_LCHAR ;

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

	"<:"                 { emit_literal_value(TOK_SUB_RELATOR_SYMBOL, HARD_TAG_SUB.data()); goto next; }
	"->"                 { emit_literal_value(TOK_RELATOR_SYMBOL, HARD_TAG_RELATOR.data()); goto next; }

	"-"? [0-9]+ "." [0-9]+
	                     { emit(TOK_FLOAT, new_value()); goto next; }
	"-"? [0-9]+          { emit(TOK_INTEGER, new_value()); goto next; }

	TAGDURL              { emit(TOK_TAGDURL, new_value()); goto next; }
	URL                  { emit(TOK_URL, new_value()); goto next; }
	HDURI                { emit(TOK_HDURI, new_value()); goto next; }
	EVURI                { emit(TOK_EVURI, new_value()); goto next; }
	ERRURI               { emit(TOK_ERRURI, new_value()); goto next; }
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
