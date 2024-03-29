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

namespace TAGL {

const char* token_str(int tok) {
	switch (tok) {
#include "tokens.inc"
		default: return "UNKNOWN_TOKEN";
	}
}

driver::driver(tagdb::tagdb *tdb, tagdb::session *ssn) :
		_own_scanner{true}, _scanner{new scanner(this)},
		_tdb{tdb}, _session{ssn}
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s, tagdb::session *ssn) :
		_scanner{s}, _tdb{tdb}, _session{ssn}
{
	this->init();
}

driver::driver(tagdb::tagdb *tdb, scanner *s, callback *cb, tagdb::session *ssn) :
		_scanner{s}, _tdb{tdb}, _session{ssn}, _callback{cb}
{
	// TODO this can produce nasty side effects.  There must be a better way...
	cb->_driver = this;
	this->init();
}

driver::driver(tagdb::tagdb *tdb, callback *cb, tagdb::session *ssn) :
		_own_scanner{true}, _scanner{new scanner(this)},
		_tdb{tdb}, _session{ssn}, _callback{cb}
{
	// TODO this can produce nasty side effects.  There must be a better way...
	cb->_driver = this;
	this->init();
}

driver::~driver() {
	this->free_parser();
	if (_own_scanner && _scanner != nullptr)
		delete _scanner;
	this->clear_context_levels();
	if (_own_session && _session != nullptr)
		delete _session;
	if (_tag != nullptr)
		delete _tag;
}

void driver::own_session(tagdb::session *ssn) {
	if (_session != nullptr && _session != ssn)
		delete _session;
	_session = ssn;
	_own_session = true;
}

tagd::code driver::push_context(const tagd::id_type& id) {
	if (_session == nullptr)
		own_session(_tdb->new_session());
	auto tc = _session->push_context(id);
	if (tc == tagd::TAGD_OK)
		_context_level++;
	return tc;
}

void driver::clear_context_levels() {
	// only pop the number of contexts pushed on the stack by this instance
	if (_session != nullptr) {
		for (size_t i=0; i<_context_level; ++i)
			_session->pop_context();
	}
	_context_level = 0;
}

void driver::finish() {
	this->free_parser();
	_scanner->reset();
	_token = -1;
	_path.clear();

	if (_callback != nullptr)
		_callback->finish();
}

// looks up a pos type for a tag and returns
// its equivalent token
int driver::lookup_pos(const std::string& s) {
	if (_token == TOK_EQ) // '=' separates object and modifier
		return TOK_MODIFIER;

	int token;
	if (TAGL_TRACE_ON && _session != nullptr) {
		std::cerr << "context: ";
		_session->print_context();
	}
	tagd::part_of_speech pos = _tdb->pos(s, _session);
	TAGL_LOG_TRACE( "pos(" << s << "): " << pos_str(pos) << std::endl )

	// TODO term_pos lookups
	//tagd::part_of_speech term_pos = _tdb->term_pos(s);
	//TAGL_LOG_TRACE( "term_pos(" << s << "): " << pos_list_str(term_pos) << std::endl )

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

tagd::code driver::execute(const std::string& statement) {
	if (statement.empty())
		return _code;

	this->init();

	_scanner->scan(statement);

	this->finish();
	this->clear_context_levels();
	return _code;
}

tagd::code driver::execute(struct evbuffer *input) {
	this->init();

	size_t read_sz = BUF_SZ - 1;
	size_t sz = evbuffer_remove(input, _scanner->_buf, read_sz); 
	if (sz > 0) {
		if (sz < BUF_SZ)
			_scanner->_buf[sz] = '\0';
		_scanner->evbuf(input);
		_scanner->scan(_scanner->_buf, sz);
	}

	this->finish();
	this->clear_context_levels();
	return this->code();
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
			TAGL_LOG_TRACE( "callback::cmd_get: " << *_tag << std::endl )
			_callback->cmd_get(*_tag);
			break;
		case TOK_CMD_PUT:
			TAGL_LOG_TRACE( "callback::cmd_put: " << *_tag << std::endl )
			_callback->cmd_put(*_tag);
			break;
		case TOK_CMD_DEL:
			TAGL_LOG_TRACE( "callback::cmd_del: " << *_tag << std::endl )
			_callback->cmd_del(*_tag);
			break;
		case TOK_CMD_QUERY:
			TAGL_LOG_TRACE( "callback::cmd_query: " << *_tag << std::endl )
			_callback->cmd_query((tagd::interrogator&) *_tag);
			break;
		default:
			this->ferror(tagd::TAGL_ERR, "unknown command: %d", _cmd);
			_callback->cmd_error();
			assert(false);
	}
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
	auto open_path_f = [&](const std::string& s) -> int {
		int fp = open(s.c_str(), flags);
		if (fp < 0)
			this->ferror(tagd::TAGL_ERR, "failed to open: %s", s.c_str());
		return fp;
	};

	// no existing relative path
	if (_path.empty())
		return open_path_f(path);

	// dir of current opened file _path
	char *dirc = strdup(_path.c_str());
	std::string dir{dirname(dirc)};
	free(dirc);
	if (dir.empty())
		return open_path_f(path);
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
		//D_CERR << "ignoring zero length file: "  << path << std::endl;
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

tagd::code driver::new_url(const std::string& url) {
	this->delete_tag();

	auto *u = new tagd::url(url);
	if (!u->ok()) {
		delete u;
		return this->ferror(u->code(), "parse URL failed: %s", url.c_str());
	}
	
	if (!constrain_tag_id.empty()) {
		// the url constrained url can be either in URI or HDURI form
		tagd::url uc(constrain_tag_id);
		if (!uc.ok()) {
			delete u;
			return this->ferror(uc.code(), "parse URL failed for constrained tag id: %s", constrain_tag_id.c_str());
		}

		if (u->id() != uc.id()) {
			delete u;
			return this->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", constrain_tag_id.c_str());
		}
	}

	_tag = u;
	return _tag->code();
}

} // namespace TAGL
