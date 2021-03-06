#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "tagl.h"
#include "tagdb.h"

#include <event2/buffer.h>

#include <stdio.h>
void* ParseAlloc(void* (*allocProc)(size_t));
void* Parse(void*, int, std::string *, TAGL::driver*);
void* ParseFree(void*, void(*freeProc)(void*));

// debug
void* ParseTrace(FILE *stream, char *zPrefix);

const char* token_str(int tok) {
	switch (tok) {
#include "tokens.inc"
		default: return "UNKNOWN_TOKEN";
	}
}

namespace TAGL {

bool driver::_trace_on = false;

driver::driver(tagdb::tagdb *tdb) :
		tagd::errorable(tagd::TAGL_INIT), _own_scanner{true}, _scanner{new scanner(this)}, _parser(nullptr),
		_token(-1), _flags(0), _tdb(tdb), _callback(nullptr),
		_cmd(), _tag(nullptr), _relator()
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s) :
		tagd::errorable(tagd::TAGL_INIT),
		_own_scanner{false}, _scanner{s}, _parser(nullptr),
		_token(-1), _flags(0), _tdb(tdb), _callback(nullptr),
		_cmd(), _tag(nullptr), _relator()
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s, callback *cb) :
		tagd::errorable(tagd::TAGL_INIT),
		_own_scanner{false}, _scanner{s}, _parser(nullptr),
		_token(-1), _flags(0), _tdb(tdb), _callback(cb),
		_cmd(), _tag(nullptr), _relator()
{
	cb->_driver = this;
	this->init();
}

driver::driver(tagdb::tagdb *tdb, callback *cb) :
		tagd::errorable(tagd::TAGL_INIT),
		_own_scanner{true}, _scanner{new scanner(this)}, _parser(nullptr),
		_token(-1), _flags(0), _tdb(tdb), _callback(cb),
		_cmd(), _tag(nullptr), _relator()
{
	cb->_driver = this;
	this->init();
}

driver::~driver() {
	if (_own_scanner)
		delete _scanner;
	if (_tag != nullptr)
		delete _tag;
}

void driver::do_callback() {
	if (_callback == nullptr)  return;

	if (this->has_errors()) {
		_callback->cmd_error();
		return;
	}

	assert(_tag != nullptr);
	if (_tag == nullptr) {
		this->error(tagd::TAGL_ERR, "callback on NULL tag");
		return;
	}

	switch (_cmd) {
		case TOK_CMD_GET:
			if (_trace_on)
				std::cerr << "callback::cmd_get: " << *_tag << std::endl;
			_callback->cmd_get(*_tag);
			break;
		case TOK_CMD_PUT:
			if (_trace_on)
				std::cerr << "callback::cmd_put: " << *_tag << std::endl;
			_callback->cmd_put(*_tag);
			break;
		case TOK_CMD_DEL:
			if (_trace_on)
				std::cerr << "callback::cmd_del: " << *_tag << std::endl;
			_callback->cmd_del(*_tag);
			break;
		case TOK_CMD_QUERY:
			if (_trace_on)
				std::cerr << "callback::cmd_query: " << *_tag << std::endl;
			_callback->cmd_query((tagd::interrogator&) *_tag);
			break;
		default:
			this->ferror(tagd::TAGL_ERR, "unknown command: %d", _cmd);
			_callback->cmd_error();
			assert(false);
	}
}

// sets up scanner and parser, wont init if already setup
void driver::init() {
	if (_parser != nullptr)
		return;

    // set up parser
    _parser = ParseAlloc(malloc);

	_code = tagd::TAGL_INIT;
	// TODO error clear
	// _msg.clear(); 
}

void driver::finish() {
	if (_parser != nullptr) {
		if (_token != TOK_TERMINATOR && !this->has_errors())
			Parse(_parser, TOK_TERMINATOR, NULL, this);
		if (_token != -1 || _token != 0)
			Parse(_parser, 0, NULL, this);
		ParseFree(_parser, free);
		_parser = nullptr;
	}

	_scanner->reset();
	_token = -1;
	_filename.clear();

	if (_callback != nullptr)
		_callback->finish();
}

void driver::trace_on(char* prefix) {
	_trace_on = true;
	ParseTrace(stderr, prefix);
}

void driver::trace_off() {
	_trace_on = false;
	ParseTrace(NULL, NULL);
}

// looks up a pos type for a tag and returns
// its equivalent token
int driver::lookup_pos(const std::string& s) const {
	if ( _token == TOK_EQ ) // '=' separates object and modifier
		return TOK_MODIFIER;

	int token;
	tagd::part_of_speech pos = _tdb->pos(s);

	if (_trace_on) {
		// TODO term_pos lookups
		//tagd::part_of_speech term_pos = _tdb->term_pos(s);
		//std::cerr << "term_pos(" << s << "): " << pos_list_str(term_pos) << std::endl;
		std::cerr << "pos(" << s << "): " << pos_str(pos) << std::endl;
	}

	switch(pos) {
		case tagd::POS_TAG:
			token = TOK_TAG;
			break;
		case tagd::POS_SUB_RELATOR:
			token = TOK_SUB_RELATOR;
			break;
		case tagd::POS_RELATOR:
			token = TOK_RELATOR;
			break;
		case tagd::POS_INTERROGATOR:
			token = TOK_INTERROGATOR;
			break;
		case tagd::POS_REFERENT:
			token = TOK_REFERENT;
			break;
		case tagd::POS_REFERS:
			token = TOK_REFERS;
			break;
		case tagd::POS_REFERS_TO:
			token = TOK_REFERS_TO;
			break;
		case tagd::POS_CONTEXT:
			token = TOK_CONTEXT;
			break;
		case tagd::POS_URL:
			token = TOK_URL;
			break;
		case tagd::POS_FLAG:
			token = TOK_FLAG;
			break;
		default:  // POS_UNKNOWN
			token = TOK_UNKNOWN;
	}

	return token;
}

void driver::parse_tok(int tok, std::string *s) {
		_token = tok;
		if (_trace_on)
			std::cerr << "line " << _scanner->_line_number << ", token " << token_str(_token) << ": " << (s == nullptr ? "NULL" : *s) << std::endl;
		Parse(_parser, _token, s, this);
}

/* parses an entire string, replace end of input with a newline
 * init() should be called before calls to parseln and
 * finish() should be called afterwards
 * empty line will result in passing a TOK_TERMINATOR token to the parser
 */
tagd::code driver::parseln(const std::string& line) {
	this->init();

	// end of input
	if (line.empty()) {
		Parse(_parser, TOK_TERMINATOR, NULL, this);
		_token = 0;
		Parse(_parser, _token, NULL, this);
		return this->code();
	}

	_scanner->scan(line.c_str());

	return this->code();
}

tagd::code driver::execute(const std::string& statement) {
	this->init();

	_scanner->scan(statement.c_str());
	this->finish();
	return this->code();
}

tagd::code driver::execute(struct evbuffer *input) {
	this->init();

	size_t sz, read_sz;

	read_sz = buf_sz - 1;
	if ((sz = evbuffer_remove(input, _scanner->_buf, read_sz)) > 0) {
		if (_trace_on)
			std::cout << "scanning: " << std::string(_scanner->_buf, sz) << std::endl;
		if (sz < buf_sz && _scanner->_buf[sz] != '\0')
			_scanner->_buf[sz] = '\0';
		_scanner->evbuf(input);
		_scanner->scan(_scanner->_buf, sz);
	}

	this->finish();
	return this->code();
}

} // namespace TAGL
