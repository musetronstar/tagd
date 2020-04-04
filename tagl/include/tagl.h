#pragma once

#include <string>
#include "tagd.h"
#include "tagdb.h"
#include "parser.h"

// forward declare
struct evbuffer;

// forward declare types used by lemon parser
struct yyParser;
static void yy_reduce(yyParser*, unsigned int, int, std::string*);

namespace TAGL {

const char* token_str(int);

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
        virtual void finish() {} // optional, can be overridden
};

const size_t BUF_SZ = 16384;

class scanner {
	friend void ::yy_reduce(yyParser*, unsigned int, int, std::string*);
	friend class TAGL::driver;

	protected:
		driver *_driver;

		const char
			*_beg  = nullptr ,
			*_cur  = nullptr ,
			*_mark = nullptr ,
			*_lim  = nullptr ,
			*_eof  = nullptr ;

		size_t _line_number = 1;
		int _tok = -1;
		int32_t _state = -1;
		char _buf[BUF_SZ];
		std::string _val;  // holds _buf overflow
		evbuffer *_evbuf = nullptr;
		bool _do_fill = false;


	public:
		scanner(driver *d) : _driver(d) { _buf[0] = '\0'; }

		virtual ~scanner() {}

		const char* fill();
		void scan(const std::string& s) { this->scan(s.c_str(), s.size()); }
		void scan(const char*, size_t);
		void scan(const char*) = delete; // don't allow implicit conversion to std::string
		void evbuf(evbuffer *ev) { _evbuf = ev; _do_fill = true; }
		void print_buf();

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
	friend void ::yy_reduce(yyParser*, unsigned int, int, std::string*);
	friend class TAGL::scanner;

	protected:
		bool _own_scanner = false;
		bool _own_session = false;

		// number of items pushed on the stack by this parser
		size_t _context_level = 0;
		// pop off only what this instance pushed on (leaving previous items untouched)
		void clear_context_levels();

		scanner *_scanner = nullptr;
		void *_parser = nullptr;	// lemon parser context
		int _token = -1;		// last token scanned: 0 = <End of Input>, -1 = unitialized (ready for new parse tree)
		int _cmd = -1;		// _token value representing a TAGL command
		tagdb::flags_t _flags = 0;

		tagdb::tagdb *_tdb = nullptr;
		tagdb::session *_session = nullptr;
		callback *_callback = nullptr;
		tagd::abstract_tag *_tag = nullptr;  // tag of the current statement
		tagd::id_type _relator;    // current relator
		tagd::id_type _constrain_tag_id; // if set, the assigned _tag.id() must be equal to this
		std::string _path;

		static bool _trace_on;

		// sets up scanner and parser for a fresh start
		void init();
		void free_parser();
		int parse_tokens();

	public:
		driver(tagdb::tagdb*, tagdb::session* = nullptr);
		driver(tagdb::tagdb*, scanner*, tagdb::session* = nullptr);
		driver(tagdb::tagdb*, scanner*, callback*, tagdb::session* = nullptr);
		driver(tagdb::tagdb*, callback*, tagdb::session* = nullptr);
		virtual ~driver();

		void session_ptr(tagdb::session *ssn) { _session = ssn; }
		tagdb::session* session_ptr() { return _session; }
		// sets _session and _own_session, delete old session (if set and not the same)
		void own_session(tagdb::session *);

		void callback_ptr(callback *c) {
			_callback = c;
			// TODO this can produce nasty side effects.  There must be a better way...
			c->_driver = this;
		}
		callback *callback_ptr() { return _callback; }

		tagd::code parseln(const std::string& = std::string());
		tagd::code execute(const std::string&);
		tagd::code execute(evbuffer*);
		int token() const { return _token; }
		bool is_trace_on() const { return _trace_on; }
		tagdb::flags_t flags() const { return _flags; }
		int lookup_pos(const std::string&);
		void parse_tok(int, std::string*);
		tagd::code include_file(const std::string&);
		int open_rel(const std::string& path, int flags);

		int cmd() const { return _cmd; }

		size_t line_number() const {
			return _scanner->_line_number;
		}

		void path(const std::string& f) { _path = f; }
		const std::string& path() const { return _path; }

		const tagd::abstract_tag& tag() const {
			static const tagd::abstract_tag empty_tag;
			return (_tag == nullptr ? empty_tag : *_tag);
		}

		void constrain_tag_id(const tagd::id_type &id) { _constrain_tag_id = id; }

		bool is_setup();
		void do_callback();

		// adds end of input to parser and frees scanner and parser
		void finish();

		static void trace_on(const char * = nullptr);
		static void trace_off();

		// checks that _tag is set to nullptr, otherwise deletes _tag and sets to nullptr
		void delete_tag() {
			if (_tag == nullptr) return;

			delete _tag;
			_tag = nullptr;
		}

		// creates a new tagd::url and sets the _tag ptr
		tagd::code new_url(const std::string&);
};

} // namespace TAGL
