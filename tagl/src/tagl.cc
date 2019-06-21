#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

// c functions stat, open, close
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "tagl.h"
#include "tagdb.h"

#include <event2/buffer.h>

#include <stdio.h>

/*
 * TODO when available in lemon, replace %extra_argument with %extra_context
 * add extra argument to ParseAlloc() and remove from Parse()
 */
void* ParseAlloc(void* (*allocProc)(size_t));
void* Parse(void*, int, std::string *, TAGL::driver*);
void* ParseFree(void*, void(*freeProc)(void*));

// debug
void* ParseTrace(FILE *stream, char *zPrefix);

namespace TAGL {

const char* token_str(int tok) {
	switch (tok) {
#include "tokens.inc"
		default: return "UNKNOWN_TOKEN";
	}
}

bool driver::_trace_on = false;

driver::driver(tagdb::tagdb *tdb) :
		_scanner{new scanner(this)}, _tdb{tdb}
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s) :
		_own_scanner{false}, _scanner{s}, _tdb{tdb}
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s, callback *cb) :
		_own_scanner{false}, _scanner{s}, _tdb{tdb}, _callback{cb}
{
	// TODO this can produce nasty side effects.  There must be a better way...
	cb->_driver = this;
	this->init();
}

driver::driver(tagdb::tagdb *tdb, callback *cb) :
		_scanner{new scanner(this)}, _tdb{tdb}, _callback{cb}
{
	// TODO this can produce nasty side effects.  There must be a better way...
	cb->_driver = this;
	this->init();
}

driver::~driver() {
	this->free_parser();
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
	// set _code for new parse, _errors will still contain prev errors
	if (_code != tagd::TAGD_OK)
		_code = tagd::TAGD_OK;

	if (_parser != nullptr)
		return;

    // set up parser
    _parser = ParseAlloc(malloc);
}

void driver::free_parser() {
	if (_parser != nullptr) {
		if (_token != TOK_TERMINATOR && !this->has_errors())
			Parse(_parser, TOK_TERMINATOR, NULL, this);
		if (_token > 0)
			Parse(_parser, 0, NULL, this);
		ParseFree(_parser, free);
		_parser = nullptr;
	}
}

void driver::finish() {
	this->free_parser();
	_scanner->reset();
	_token = -1;
	_path.clear();

	if (_callback != nullptr)
		_callback->finish();
}

void driver::trace_on(const char* prefix) {
	_trace_on = true;
	ParseTrace(stderr, (char *)prefix);
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
		case tagd::POS_INCLUDE:
			token = TOK_INCLUDE;
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
	if (statement.empty())
		return _code;

	this->init();

	_scanner->scan(statement.c_str());
	this->finish();
	return _code;
}

tagd::code driver::execute(struct evbuffer *input) {
	this->init();

	size_t read_sz = BUF_SZ - 1;
	size_t sz = evbuffer_remove(input, _scanner->_buf, read_sz); 
	if (sz > 0) {
		if (_trace_on)
			std::cout << "scanning: " << std::string(_scanner->_buf, sz) << std::endl;
		if (sz < BUF_SZ)
			_scanner->_buf[sz] = '\0';
		_scanner->evbuf(input);
		_scanner->scan(_scanner->_buf, sz);
	}

	this->finish();
	return this->code();
}


class tagdio : public tagd::errorable {
    public:
        static tagdio& runtime();

	/*** singleton can't instatiate, can't copy **/
    private:
        tagdio() {}
    public:
        tagdio(tagdio const&) = delete;
        void operator=(tagdio const&) = delete;

		bool is_dir(const std::string&);
};

tagdio& tagdio::runtime() {
	static tagdio S1;
	return S1;
}

bool tagdio::is_dir(const std::string& path) {
	struct stat st;

	if(stat(path.c_str(), &st) != 0) {
		this->ferror(tagd::TAGL_ERR, "cannot stat: %s", path.c_str());
		return false;
	}

	return (st.st_mode & S_IFDIR);
}
/*
 * open path relative to this->_path, return file descriptor
 * if path is absolute, rel_path is ignore
 * path reference passed in test to path opened
 */
int driver::open_rel(const std::string& path, int flags) {
	auto set_open_path_f = [&](const std::string& s) -> int {
		int fp = open(s.c_str(), flags);
		if (fp < 0)
			this->ferror(tagd::TAGL_ERR, "failed to open: %s", s.c_str());
		else
			_path = s;
		return fp;
	};

	// no existing relative path
	if (_path.empty())
		return set_open_path_f(path);

	// dir of current opened file _path
	char *dirc = strdup(_path.c_str());
	std::string dir{dirname(dirc)};
	free(dirc);
	if (dir.empty())
		return set_open_path_f(path);
	dir.push_back('/');

	// new file name to open
	char *basec = strdup(path.c_str());
	std::string base{basename(basec)};
	free(basec);

	int rel_fd = open(dir.c_str(), O_PATH);

	if (rel_fd < 0) {
		this->ferror(tagd::TAGL_ERR, "failed to stat directory: %s", dir.c_str());
		return rel_fd;
	}

	int fd = openat(rel_fd, path.c_str(), flags);
	_path = dir.append(base);
	close(rel_fd);

	return fd;
}

tagd::code driver::include_file(const std::string& path) {
	int fd = this->open_rel(path, O_RDONLY);

	if (fd < 0)
		return this->code();

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return this->ferror(tagd::TAGL_ERR, "stat failed: %s", path.c_str());
	}

	struct evbuffer *input = evbuffer_new();

	if (st.st_size == 0) {
		// TODO create flag FAIL_OPEN_ZERO_LENGTH_FILE
		//std::cerr << "ignoring zero length file: "  << path << std::endl;
		if (fcntl(fd, F_GETFD) != -1)
			close(fd);
		return tagd::TAGD_OK;
	}

	tagd::code rc;
	// evbuffer_add_file closes fd for us
	if (evbuffer_add_file(input, fd, 0, st.st_size) == 0) {
		// create object of this type of driver to execute include file
		// TODO figure out how to make this work create new polymorphic object
		// auto *inc_driver = new decltype(this)(_tdb, _callback);
		driver inc_driver(_tdb, _callback);  // WTF - sets _callback->_driver = this (&inc_driver)
		inc_driver.path(path);
		rc = inc_driver.execute(input);
		if (inc_driver.has_errors())
			this->copy_errors(inc_driver);
	} else {
		rc = this->ferror(tagd::TAGL_ERR, "evbuffer_add_file failed: %s", path.c_str());
	}

	this->callback_ptr(_callback);  // WTF - reset _callback->_driver = this

	evbuffer_free(input);
	return rc;

}

} // namespace TAGL
