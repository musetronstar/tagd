#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "tagl.h"  // includes taglparser.h
#include "tagspace.h"

#include <event2/buffer.h>

#include <stdio.h>
void* ParseAlloc(void* (*allocProc)(size_t));
void* Parse(void*, int, std::string *, TAGL::driver*);
void* ParseFree(void*, void(*freeProc)(void*));

// debug
void* ParseTrace(FILE *stream, char *zPrefix);

namespace TAGL {

bool driver::_trace_on = false;

driver::driver(tagspace::tagspace *ts) :
		tagd::errorable(tagd::TAGL_INIT), _scanner(this), _parser(NULL),
		_token(-1), _TS(NULL), _callback(NULL),
		_cmd(), _tag(NULL), _relator()
{
	_TS = ts;
	this->init();
}

driver::driver(tagspace::tagspace *ts, callback *cb) :
		tagd::errorable(tagd::TAGL_INIT), _scanner(this), _parser(NULL),
		_token(-1), _TS(NULL), _callback(NULL),
		_cmd(), _tag(NULL), _relator()
{
	_TS = ts;
	_callback = cb;
	this->init();
}

driver::~driver() {
	if (_tag != NULL)
		delete _tag;

	this->finish();
}

void driver::do_callback() {
	if (_callback == NULL)  return;

	if (_code != tagd::TAGD_OK) {
		_callback->error(*this);
		return;
	}

	assert(_tag != NULL);
	if (_tag == NULL) {
		this->error(tagd::TAGL_ERR, "callback on NULL tag");
		return;
	}

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
			this->error(tagd::TAGL_ERR, "unknown command: %d", _cmd);
			_callback->error(*this);
			assert(false);
	}
}

// sets up scanner and parser, wont init if already setup
void driver::init() {
	if (this->is_setup())
		return;

    // set up parser
    _parser = ParseAlloc(malloc);

	_code = tagd::TAGL_INIT;
	// TODO error clear
	// _msg.clear(); 
}

inline bool driver::is_setup() {
	return ( _parser != NULL );
}

void driver::finish() {
	if (_parser != NULL) {
		if (_token != TERMINATOR && _code != tagd::TAGL_ERR)
			Parse(_parser, TERMINATOR, NULL, this);
		if (_token != -1 || _token != 0)
			Parse(_parser, 0, NULL, this);
		ParseFree(_parser, free);
		_parser = NULL;
	}

	_token = -1;
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
	if ( _token == EQUALS ) // '=' separates object and modifier
		return MODIFIER;

	int token;
	switch(_TS->pos(s)) {
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
		case tagd::POS_REFERENT:
			token = REFERENT;
			break;
		case tagd::POS_REFERS:
			token = REFERS;
			break;
		case tagd::POS_REFERS_TO:
			token = REFERS_TO;
			break;
		case tagd::POS_CONTEXT:
			token = CONTEXT;
			break;
		default:  // POS_UNKNOWN
			token = UNKNOWN;
	}

	return token;
}

void driver::parse_tok(int tok, std::string *s) {
		_token = tok;
		if (_trace_on)
			std::cerr << _token << ": " << (s == NULL ? "NULL" : *s) << std::endl;
		Parse(_parser, _token, s, this);
}

/* parses an entire string, replace end of input with a newline
 * init() should be called before calls to parseln and
 * finish() should be called afterwards
 * empty line will result in passing a TERMINATOR token to the parser
 */
tagd_code driver::parseln(const std::string& line) {
	this->init();

	// end of input
	if (line.empty()) {
		Parse(_parser, TERMINATOR, NULL, this);
		_token = 0;
		Parse(_parser, _token, NULL, this);
		return this->code();
	}

	scanner scnr(this);
	scnr.scan(line.c_str());

	return this->code();
}


tagd_code driver::execute(const std::string& statement) {
	this->init();

	_scanner.scan(statement.c_str());
	this->finish();
	return this->code();
}

tagd_code driver::tagdurl_get(const std::string& tagdurl) {
	this->init();

	_scanner.scan_tagdurl_path(CMD_GET, tagdurl);

	return this->code();
}

tagd_code driver::tagdurl_put(const std::string& tagdurl) {
	this->init();

	_scanner.scan_tagdurl_path(CMD_PUT, tagdurl);

	return this->code();
}

tagd_code driver::evbuffer_execute(struct evbuffer *input) {
	this->init();

	size_t sz = evbuffer_get_length(input);

	const size_t buf_sz = 1024 * 4; // 4k
	size_t read_sz;
    char buf[buf_sz];
	size_t offset = 0;
	std::string leftover;
	while (1) {
		if (!leftover.empty()) {
			strncpy(&buf[0], leftover.c_str(), leftover.size());
			offset = leftover.size();
			buf[offset] = '\0';	
			// std::cout << "buf leftover: '" << buf << "'" << std::endl;
			// std::cout << "offset: " << offset << std::endl;
			leftover.clear();
		} else {
			offset = 0;
		}

		read_sz = buf_sz - offset - 1;
        if ((sz = evbuffer_remove(input, &buf[offset], read_sz)) == 0)
			break;

		sz += offset;
		buf[sz] = '\0';
		// std::cout << "buf: " <<  buf << std::endl;

		// overflow, find partial last token
		if (evbuffer_get_length(input) > 0) {
			for (size_t z=sz; z>0; --z) {
				if(isspace(buf[z])) {
					leftover = &buf[z+1];
					buf[z+1] = '\0';
					// std::cout << "leftover: '" << leftover << "'" << std::endl;
					break;
				}
			}
		}

		// std::cout << "scanning: " << buf << std::endl;
		_scanner.scan(buf);
	}

	this->finish();
	return this->code();
}

} // namespace TAGL
