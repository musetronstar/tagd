#pragma once

#include <string>
#include "tagd.h"
#include "tagspace.h"
#include "parser.h"

// forward declare
struct evbuffer;

// forward declare (in parser)
struct yyParser;
static void yy_reduce(yyParser *, int);

namespace TAGL {

class driver; 

class callback {
	friend class TAGL::driver;

	protected:
		driver *_driver;

    public:
        callback() {}
        virtual ~callback() {}

        // pure virtuals - consumers must override
        virtual void cmd_get(const tagd::abstract_tag&) = 0;
        virtual void cmd_put(const tagd::abstract_tag&) = 0;
        virtual void cmd_del(const tagd::abstract_tag&) = 0;
        virtual void cmd_query(const tagd::interrogator&) = 0;
        virtual void cmd_error() = 0;
        virtual void finish() {} // optionally, can be overridden
};

const size_t buf_sz = 256;

class scanner {
		friend void ::yy_reduce(yyParser *, int);
		friend class TAGL::driver;
	
		driver *_driver;
		size_t _line_number;

		const char *_beg, *_cur, *_mark, *_lim, *_eof;	
		char _buf[buf_sz];
		int _tok;
		int32_t _state;
		std::string _val;
		bool _do_fill;
		evbuffer *_evbuf;


	public:
		scanner(driver *d) :
			_driver(d), _line_number(1),
			_beg{nullptr}, _cur{nullptr}, _mark{nullptr}, _lim{nullptr}, _eof{nullptr},
			_tok{-1}, _state{-1}, _do_fill{false}, _evbuf{nullptr} {}
		~scanner() {}

		const char* fill();
		void scan(const std::string& s) { this->scan(s.c_str(), s.size()); }
		void scan(const char*, size_t);
		void evbuf(evbuffer *ev) { _evbuf = ev; _do_fill = true; }
		void print_buf();

		void scan_tagdurl_path(int cmd, const std::string&);

		void reset() {
			_line_number = 1;

			_beg = _mark = _cur = _lim = _eof = nullptr;
			_val.clear();
			_evbuf = nullptr;
			_state = -1;
			_tok = -1;
		}
};

class driver : public tagd::errorable {
		friend void ::yy_reduce(yyParser *, int);
		friend class TAGL::scanner;

		scanner _scanner;
		void* _parser;	// lemon parser context
		int _token;		// last token scanned
		std::string _filename;
		tagspace::flags_t _flags;

		static bool _trace_on;

		// sets up scanner and parser for a fresh start
		void init();
		int parse_tokens();

		tagspace::tagspace *_TS;
		callback *_callback;
		int _cmd;
		tagd::abstract_tag *_tag;  // tag of the current statement
		tagd::id_type _relator;    // current relator

		typedef scanner scanner_t;	// for scanner() below

	public:
		driver(tagspace::tagspace *ts);
		driver(tagspace::tagspace *ts, callback *);
		~driver();

		void callback_ptr(callback * c) { _callback = c; c->_driver = this; }
		callback *callback_ptr() { return _callback; }
		tagd_code parseln(const std::string& = std::string());
		tagd_code execute(const std::string&);
		tagd_code tagdurl_get(const std::string&);
		tagd_code tagdurl_put(const std::string&);
		tagd_code tagdurl_del(const std::string&);
		tagd_code evbuffer_execute(struct evbuffer*);
		int token() const { return _token; }
		bool is_trace_on() const { return _trace_on; }
		tagspace::flags_t flags() const { return _flags; }
		int lookup_pos(const std::string&) const;
		void parse_tok(int, std::string*);

		int cmd() const { return _cmd; }

		size_t line_number() const {
			return _scanner._line_number;
		}

		void filename(const std::string& f) { _filename = f; }
		const std::string& filename() const { return _filename; }

		const tagd::abstract_tag& tag() const {
			static const tagd::abstract_tag empty_tag;
			return (_tag == NULL ? empty_tag : *_tag);
		}

		bool is_setup(); 
		void do_callback();

		// adds end of input to parser,
		// frees scanner and parser
		void finish();

		static void trace_on(char * = NULL);
		static void trace_off();
};

} // namespace TAGL
