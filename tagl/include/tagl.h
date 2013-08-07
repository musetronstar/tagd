#pragma once

#include <string>
#include "tagd.h"
#include "tagspace.h"
#include "taglparser.h"

typedef enum {
    TAGL_INIT,
    TAGL_OK,
    TAGL_ERR
} tagl_code;

// wrap in struct so we can define here
struct tagl_util {
    static std::string tagl_code_str(tagl_code c) {
        switch (c) {
            case TAGL_INIT: return "TAGL_INIT";
            case TAGL_OK:   return "TAGL_OK";
            case TAGL_ERR:  return "TAGL_ERR";
            default:        return "STR_EMPTY";
        }
    }
};

#define tagl_code_str(c) tagl_util::tagl_code_str(c)

// forward declarations for parser friendship
namespace TAGL { class driver; }
// in parser
struct yyParser;
static void yy_reduce(yyParser *, int);
// in scanner
int scanner_pos(TAGL::driver*, const char*);

namespace TAGL {

class callback {
    public:
        callback() {}
        virtual ~callback() {}

        // pure virtuals - consumers must override
        virtual void cmd_get(const tagd::abstract_tag&) = 0;
        virtual void cmd_put(const tagd::abstract_tag&) = 0;
        virtual void cmd_query(const tagd::interrogator&) = 0;
        // virtual void cmd_test(const tagd::abstract_tag&) = 0;
        virtual void error(const TAGL::driver&) = 0;

		// helper function
		// static bool test_tag_ok(tagspace::tagspace&, const tagd::abstract_tag&);
};


class driver {
	private:
		// flex scanner context
		// yyscan_t not defined yet (but its void*)
		void* _scanner;

		// lemon parser context
		void* _parser;

		int _token;  // last token scanned
		tagl_code _code;
		std::string _msg;

		static bool _trace_on;

		// sets up scanner and parser for a fresh start
		void init();
		int parse_tokens();

		tagspace::tagspace *_TS;
		callback *_callback;

		friend void ::yy_reduce(yyParser *, int);
		friend int ::scanner_pos(TAGL::driver*, const char*);

		// members accessed by yy_reduce
		int _default_cmd;
		int _cmd;
		tagd::abstract_tag *_tag;  // tag of the current statement
		tagd::id_type _relator;    // current relator
		void statement_end();

	public:
		driver(tagspace::tagspace *ts);
		driver(tagspace::tagspace *ts, callback *);
		~driver();

		tagl_code parseln(const std::string& = std::string());
		tagl_code parse(const std::string&);
		// tagl_code parsefp(FILE *fp, int sz = -1);  // neg size, use YY_BUF_SIZE

		void code(const tagl_code& c) { _code = c; }
		tagl_code code() const { return _code; }

		void msg(const std::string&, std::string * = NULL);
		std::string msg() const { return _msg; }
		int cmd() const { return _cmd; }
		const tagd::abstract_tag * tag() const { return _tag; }
		void error(const std::string&, std::string * = NULL);

		bool is_setup(); 

		// adds end of input to parser,
		// frees scanner and parser
		void finish();

		static void trace_on(char * = NULL);
		static void trace_off();
};

} // namespace TAGL
