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
    public:
        callback() {}
        virtual ~callback() {}

        // pure virtuals - consumers must override
        virtual void cmd_get(const tagd::abstract_tag&) = 0;
        virtual void cmd_put(const tagd::abstract_tag&) = 0;
        virtual void cmd_del(const tagd::abstract_tag&) = 0;
        virtual void cmd_query(const tagd::interrogator&) = 0;
        virtual void cmd_error(const TAGL::driver&) = 0;

		// helper function
		// static bool test_tag_ok(tagspace::tagspace&, const tagd::abstract_tag&);
};

class scanner {
		driver *_driver;
		size_t process_block_comment(const char *);
	public:
		scanner(driver *d) : _driver(d) {}
		~scanner() {}
		void scan(const char*);
		void scan_tagdurl_path(int cmd, const std::string&, const url_query_map_t* qm = nullptr);
};

class driver : public tagd::errorable {
		friend void ::yy_reduce(yyParser *, int);
		friend class TAGL::scanner;

		scanner _scanner;
		void* _parser;	// lemon parser context
		int _token;		// last token scanned
		bool _block_comment_open;

		static bool _trace_on;

		// sets up scanner and parser for a fresh start
		void init();
		int parse_tokens();

		tagspace::tagspace *_TS;
		callback *_callback;
		int _cmd;
		tagd::abstract_tag *_tag;  // tag of the current statement
		tagd::id_type _relator;    // current relator

	public:
		driver(tagspace::tagspace *ts);
		driver(tagspace::tagspace *ts, callback *);
		~driver();

		void callback_ptr(callback * c) { _callback = c; }
		callback *callback_ptr() { return _callback; }
		tagd_code parseln(const std::string& = std::string());
		tagd_code execute(const std::string&);
		tagd_code tagdurl_get(const std::string&, const url_query_map_t* qm = nullptr);
		tagd_code tagdurl_put(const std::string&, const url_query_map_t* qm = nullptr);
		tagd_code tagdurl_del(const std::string&, const url_query_map_t* qm = nullptr);
		tagd_code evbuffer_execute(struct evbuffer*);
		int token() const { return _token; }
		int lookup_pos(const std::string&) const;
		void parse_tok(int, std::string*);

		int cmd() const { return _cmd; }

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
