#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "tagl.h"  // includes taglparser.h
#include "taglscanner.yy.h"
#include "tagspace.h"

void* ParseAlloc(void* (*allocProc)(size_t));
void* Parse(void*, int, std::string *, TAGL::driver*);
void* ParseFree(void*, void(*freeProc)(void*));

// debug
void* ParseTrace(FILE *stream, char *zPrefix);

namespace TAGL {

/*
bool callback::test_tag_ok(tagspace::tagspace& TS, const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	ts_res_code ts_rc = TS.get(T, t.id());

	bool test_ok = (ts_rc == TS_OK);

	if (test_ok) {
		if (!t.super().empty() && T.super() != t.super())
			return false;
	}

	if (test_ok && t.relations.size() > 0) {
		for (tagd::predicate_set::iterator it = t.relations.begin(); it != t.relations.end(); ++it) {
			if (!T.related(*it)) {
				return false;
			}
		}
	}

	return test_ok;
}
*/

bool driver::_trace_on = false;

driver::driver(tagspace::tagspace *ts) :
		_scanner(NULL), _parser(NULL), _token(-1),
		_code(TAGL_INIT), _msg(), _TS(NULL), _callback(NULL),
		_default_cmd(CMD_GET), _cmd(_default_cmd), _tag(NULL), _relator()
{
	_TS = ts;
	this->init();
}

driver::driver(tagspace::tagspace *ts, callback *cb) :
		_scanner(NULL), _parser(NULL), _token(-1),
		_code(TAGL_INIT), _msg(), _TS(NULL), _callback(NULL),
		_default_cmd(CMD_GET), _cmd(_default_cmd), _tag(NULL), _relator()
{
	_TS = ts;
	_callback = cb;
	this->init();
}

driver::~driver() {
	this->finish();
}

void driver::statement_end() {
	assert(_tag != NULL);

	if (_callback == NULL)  return;

	switch (_cmd) {
		case CMD_GET:
			_callback->cmd_get(*_tag);
			break;
		case CMD_PUT:
			_callback->cmd_put(*_tag);
			break;
		case CMD_QUERY:
			_callback->cmd_query((tagd::interrogator&) *_tag);
			break;
	// case CMD_TEST:
		//	_callback->cmd_test(*_tag);
		//	break;
		default:
			this->code(TAGL_ERR);
			this->msg("unknown command");
			assert(false);
	}
}

// sets up scanner and parser, wont init if already setup
void driver::init() {
	if (this->is_setup())
		return;

	// set up scanner
	yylex_init(&_scanner);
	yylex_init_extra(this, &_scanner);

    // set up parser
    _parser = ParseAlloc(malloc);

	_code = TAGL_INIT;
	_msg.clear(); 
}

inline bool driver::is_setup() {
	return (
		_scanner != NULL &&
		_parser != NULL
	);
}

void driver::finish() {
	if (_scanner != NULL) {
		yylex_destroy(_scanner);
		_scanner = NULL;
	}

	if (_parser != NULL) {
		if (_token != TERMINATOR && _code != TAGL_ERR)
			Parse(_parser, TERMINATOR, NULL, this);
		if (_token != -1 || _token != 0)
			Parse(_parser, 0, NULL, this);
		ParseFree(_parser, free);
		_parser = NULL;
	}

	_token = -1;
	if (_tag != NULL) {
		delete _tag;
		_tag = NULL;
	}
}

void driver::trace_on(char* prefix) {
	_trace_on = true;
	ParseTrace(stderr, prefix);
}

void driver::trace_off() {
	_trace_on = false;
	ParseTrace(NULL, NULL);
}

void driver::msg(const std::string& s, std::string *token) {
	if (token != NULL) {
		std::stringstream ss;
		ss << s << ": " << *token;
		_msg = ss.str();
	} else {
		_msg = s;
	}
}

void driver::error(const std::string& s, std::string *token) {
	_code = TAGL_ERR;
	this->msg(s, token);
	if (_callback == NULL)  return;
	_callback->error(*this);
}

int driver::parse_tokens() {
	int last = -1;
    while ( (_token = yylex(_scanner)) != 0 ) {  //  0 is end of input
		std::string *s = new std::string(yyget_text(_scanner), yyget_leng(_scanner));
		if (_trace_on)
			std::cout << _token << ": " << *s << std::endl;
		Parse(_parser, _token, s, this);
		if (_code == TAGL_ERR)
			break;
		last = _token;
    }

	return last;
}

/* parses an entire string, replace end of input with a newline
 * init() should be called before calls to parseln and
 * finish() should be called afterwards
 * empty line will result in passing a TERMINIATOR token to the parser
 */
tagl_code driver::parseln(const std::string& line) {
	this->init();

	// end of input
	if (line.empty()) {
		Parse(_parser, TERMINATOR, NULL, this);
		_token = 0;
		Parse(_parser, _token, NULL, this);
		return this->code();
	}

    YY_BUFFER_STATE buff = yy_scan_string(line.c_str(), _scanner);
	int last = this->parse_tokens();
    yy_delete_buffer(buff, _scanner);
 
    if (_token == -1) {
		this->code(TAGL_ERR);
		this->msg("Scanner error");
		this->finish();
    } else if (_token == 0 && last == TERMINATOR) {
		// Parse the EOF token to force a statement reduce action (e.g. parseln ending with ;)
		Parse(_parser, _token, NULL, this);
	}

	return this->code();
}


/* parses an entire string until scanner reaches the end of input */
tagl_code driver::parse(const std::string& statement) {
	this->init();

    YY_BUFFER_STATE buff = yy_scan_string(statement.c_str(), _scanner);
	this->parse_tokens();
    yy_delete_buffer(buff, _scanner);
 
    if (_token == -1) {
		this->code(TAGL_ERR);
		this->msg("Scanner error");
    }

	this->finish();

	return this->code();
}

/* parses a file pointer until scanner reaches the end of input */
/*
tagl_code driver::parsefp(FILE *fp, int sz) {
	this->init();

	if (sz < 0)
		sz = YY_BUF_SIZE;

    YY_BUFFER_STATE buff = yy_create_buffer(fp, sz, _scanner);
	yyset_in(fp, _scanner);
	yyset_in(stdout, _scanner);
	this->parse_tokens();
    //yy_delete_buffer(buff, _scanner);
 
    if (_token == -1) {
		this->code(TAGL_ERR);
		this->msg("Scanner error");
    }

	this->finish();

	return this->code();
}
*/


} // namespace TAGL
