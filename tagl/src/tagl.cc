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
#include <filesystem>
#include "tagspace.h"

#include <event2/buffer.h>

#include <stdio.h>

void *ParseAlloc(void *(*)(size_t), TAGL::driver *);
void Parse(void *, int, TAGL::text_token);
void ParseFree(void *, void (*)(void *));
void ParseTrace(FILE *, char *);

/*
 * TODO when available in lemon, replace %extra_argument with %extra_context
 * add extra argument to ParseAlloc() and remove from Parse()
 */

bool TAGL_TRACE_ON = false;
static tagd::logger *TAGL_LOGGER = nullptr;

void TAGL_SET_TRACE_ON() {
	TAGL_TRACE_ON = true;

#ifndef NDEBUG
	ParseTrace(stderr, (char *)"tagl_trace: ");
#endif
}

void TAGL_SET_TRACE_OFF() {
	TAGL_TRACE_ON = false;

#ifndef NDEBUG
	ParseTrace(NULL, NULL);
#endif
}

void TAGL_SET_LOGGER(tagd::logger *log) {
	TAGL_LOGGER = log;
}

bool TAGL_LOG_ENABLED(const std::string& role, tagd::log_level lvl) {
	if (TAGL_LOGGER == nullptr)
		return false;

	return static_cast<int>(lvl) <= static_cast<int>(TAGL_LOGGER->level(role));
}

void TAGL_LOG(const std::string& role, tagd::log_level lvl, const std::string& msg) {
	if (TAGL_LOGGER == nullptr)
		return;

	if ((role == std::string(HARD_TAG_ROLE_SCANNER) || role == std::string(HARD_TAG_ROLE_PARSER)
			|| role == std::string(HARD_TAG_ROLE_DRIVER))
			&& lvl == tagd::log_level::DEBUG) {
		std::stringstream ss(msg);
		std::string line;

		while (std::getline(ss, line)) {
			TAGL_LOGGER->log(role, lvl, std::string("-- ").append(line));
		}

		if (!msg.empty() && msg.back() == '\n')
			TAGL_LOGGER->log(role, lvl, "-- ");

		return;
	}

	TAGL_LOGGER->log(role, lvl, msg);
}

namespace TAGL {

const char* token_str(int tok) {
	switch (tok) {
#include "tokens.inc"
		default: return "UNKNOWN_TOKEN";
	}
}

driver::driver(tagd::tagspace *tsp, tagd::tagspace_session *ssn) :
		_own_scanner{true}, _scanner{new scanner(this)},
		_tsp{tsp}, _session{ssn}
{
	this->init();
}

driver::driver(tagd::tagspace *tsp, scanner *s, tagd::tagspace_session *ssn) :
		_scanner{s}, _tsp{tsp}, _session{ssn}
{
	this->init();
}

driver::driver(tagd::tagspace *tsp, scanner *s, callback *cb, tagd::tagspace_session *ssn) :
		_scanner{s}, _tsp{tsp}, _session{ssn}
{
	this->bind_callback(cb);
	this->init();
}

driver::driver(tagd::tagspace *tsp, callback *cb, tagd::tagspace_session *ssn) :
		_own_scanner{true}, _scanner{new scanner(this)},
		_tsp{tsp}, _session{ssn}
{
	this->bind_callback(cb);
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

// sets up scanner and parser, wont init if already setup
void driver::parser_deleter(void* p) {
	ParseFree(p, ::operator delete);
}

void driver::init() {
	// set _code for new parse, _errors will still contain prev errors
	if (_code != tagd::TAGD_OK)
		_code = tagd::TAGD_OK;

	if (_parser)
		return;

	// A new parser begins a new statement; per-statement scanner and error state starts clean.
	_error_callback_delivered = false;
	_set_statement = false;
	_tagspace_arguments = false;

	_parser.reset(ParseAlloc(::operator new, this));
}

void driver::free_parser() {
	if (_parser) {
		if (_token != TOK_TERMINATOR && !this->has_errors())
			Parse(_parser.get(), TOK_TERMINATOR, EMPTY_VALUE);
		if (_token > 0 && !this->has_errors())
			Parse(_parser.get(), 0, EMPTY_VALUE);
		_parser.reset();  // calls parser_deleter → ParseFree(p, ::operator delete)
	}
}

void driver::parse_tok(int tok, text_token s) {
		if (_predicate_fragment) {
			// No statement may execute until the complete predicate fragment has been accepted.
			switch (tok) {
				case TOK_CMD_SET:
				case TOK_CMD_GET:
				case TOK_CMD_PUT:
				case TOK_CMD_STORE:
				case TOK_CMD_DELETE:
				case TOK_CMD_DELETE_STORAGE:
				case TOK_CMD_QUERY:
				case TOK_TERMINATOR:
					error(tagd::TAGL_ERR, "STORE predicates cannot contain commands or terminators");
					return;
			}
			if (_predicate_fragment_empty && (tok == TOK_SUB_RELATOR || tok == TOK_SUB_RELATOR_SYMBOL)) {
				error(tagd::TAGL_ERR, "STORE predicates cannot declare a subordinate relation");
				return;
			}
			_predicate_fragment_empty = false;
		}
		// SET operands enable location tokens only for this statement, until termination or error.
		if (tok == TOK_CMD_SET)
			_set_statement = true;
		else if (tok == TOK_TERMINATOR) {
			_set_statement = false;
			_tagspace_arguments = false;
		}
		if (tok == TOK_SET_USE || tok == TOK_SET_TAGSPACE || tok == TOK_SET_CONFIG_ID)
			_tagspace_arguments = true;

		_token = tok;
		TAGL_LOG_TRACE( "line " << _scanner->_line_number
				<< ", token " << token_str(_token) << ": " << (s.empty() ? "NULL" : s.str())
				<< std::endl )

		Parse(_parser.get(), _token, s);
		if (has_errors()) {
			_set_statement = false;
			_tagspace_arguments = false;
		}
}

void driver::parse_tok(int tok, const std::string& s) {
	this->parse_tok(tok, this->store_token_text(s));
}

void driver::parse_tok(int tok, const char *s) {
	this->parse_tok(tok, this->store_token_text(std::string(s)));
}

void driver::bind_callback(callback *cb) {
	_callback = cb;
	if (cb != nullptr)
		cb->_driver = this;
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
		Parse(_parser.get(), TOK_TERMINATOR, EMPTY_VALUE);
		_token = 0;
		Parse(_parser.get(), _token, EMPTY_VALUE);
		return this->code();
	}

	// Scanner errors never reach a parser action; report them through the callback here.
	_scanner->scan(line);
	if (has_errors())
		do_callback();

	return this->code();
}

void driver::own_session(tagd::tagspace_session *ssn) {
	if (_session != nullptr && _session != ssn)
		delete _session;
	_session = ssn;
	_own_session = true;
	bind_membership();
}

void driver::bind_membership() {
	if (_selection && _session) {
		auto selected = _selection;
		// A copied session retains the selection and its borrow, never a raw driver address.
		_session->membership([selected](tagd::id_view id) {
			return selected->target->exists(id);
		});
	}
}

tagd::code parse_tagdurl_address(const std::string& input, tagdurl_address& address,
	tagd::errorable& errors) {
	address = {};
	auto bad_path = [&]() {
		return errors.error(tagd::TAGL_ERR,
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, input));
	};
	const auto query = input.find('?');
	const auto path = input.substr(0, query);
	if (path.size() < 2 || path.front() != '/' || path.find("//") != std::string::npos)
		return bad_path();

	const auto slash = path.find('/', 1);
	const auto encoded = path.substr(1, slash == std::string::npos ? slash : slash - 1);
	if (!tagd::uri_decode_checked(address.selector, encoded))
		return bad_path();
	if (address.selector != "_" && address.selector != tagd::HARD_TAG_DEFAULT
		&& address.selector != tagd::HARD_TAG_CONFIG
		&& !tagd::valid_tagspace_name(address.selector))
		return bad_path();

	address.description = slash == std::string::npos || slash == path.size() - 1;
	address.descendants = slash != std::string::npos && slash == path.size() - 1;
	address.operation = slash == std::string::npos ? "/" : path.substr(slash);
	if (query != std::string::npos)
		address.query = input.substr(query);
	return tagd::TAGD_OK;
}

void driver::restore_tagdurl() {
	if (_tagdurl_previous) {
		_selection->target = std::move(*_tagdurl_previous);
		_tagdurl_previous.reset();
	}
	if (_tagdurl_context && _session) {
		_session->lookup_contexts(*_tagdurl_context);
		_tagdurl_context.reset();
	}
	_tagdurl_command = -1;
}

tagd::code driver::select_tagdurl(const tagdurl_address& address,
	std::string& operation, bool description_read) {
	if (!_config) {
		if (address.selector != "_" || address.description)
			return error(tagd::TS_MISUSE, "tagspace collection not configured");
		operation = address.operation + address.query;
		return tagd::TAGD_OK;
	}

	/*
	 * Selector aliases use the requested context before the target is known.
	 * This temporary lookup does not validate against config or retain contexts;
	 * the scanner applies them once, with membership in the selected target.
	 */
	struct restore_context {
		tagd::tagspace_session& session;
		tagd::id_vec previous;
		~restore_context() {
			session.lookup_contexts(previous);
		}
	} restore{*_session, _session->context()};
	auto contexts = restore.previous;
	tagd::id_vec requested;
	const auto context_rc = tagdurl_contexts(address.query, requested, *this);
	if (context_rc != tagd::TAGD_OK)
		return context_rc;
	contexts.insert(contexts.end(), requested.begin(), requested.end());
	_session->lookup_contexts(contexts);

	// Description reads are config operations: authorize before resolving or discovering names.
	if (description_read && address.description && _http && !_http_config_allowed) {
		_http_forbidden = true;
		return error(tagd::TS_MISUSE,
			tagd::predicate(HARD_TAG_CAUSED_BY, tagd::HARD_TAG_TAGSPACE, address.selector));
	}

	tagd::id_string id;
	auto rc = tagd::TAGD_OK;
	const auto target = address.selector == "_" ? _config->tagspace_id(tsp()) : address.selector;
	rc = _config->resolve_tagspace(target, id, _session);
	if (rc != tagd::TAGD_OK && rc != tagd::TS_NOT_FOUND) {
		copy_errors(*_config);
		return code(rc);
	}
	if (rc == tagd::TS_NOT_FOUND && (!_config->tagdurl_autoload() || _config->home().empty()))
		return error(tagd::TS_NOT_FOUND,
			tagd::predicate(HARD_TAG_CAUSED_BY, tagd::HARD_TAG_TAGSPACE, target));

	// Description operations execute in config, regardless of the described source.
	const bool other_target = description_read && address.description
		? _http_target && _http_target.get() != _config
		: _http_target && (id.empty() || id != _config->tagspace_id(_http_target.get()));
	if (other_target) {
		_http_forbidden = true;
		return error(tagd::TS_MISUSE,
			tagd::predicate(HARD_TAG_CAUSED_BY, tagd::HARD_TAG_TAGSPACE, target));
	}
	if (_config_loading && !(description_read && address.description) && id != tagd::HARD_TAG_CONFIG)
		return error(tagd::TS_MISUSE, "config loading cannot switch tagspaces");

	if (description_read && address.description) {
		std::shared_ptr<tagd::tagspace> unused;
		bool denied = false;
		const auto access = !_http ? tagd::tagspace_access::local
			: tagd::tagspace_access::named_with_config;
		rc = _config->open_tagspace(target, "", unused, _session, {}, access,
			{}, &denied, tagd::source_action::register_only);
		if (rc == tagd::TAGD_OK)
			rc = _config->resolve_tagspace(target, id, _session);
		if (rc != tagd::TAGD_OK) {
			copy_errors(*_config);
			_http_forbidden = _http && denied;
			return code(rc);
		}
		_selection->target = _config->borrow(_config);
		operation = "/" + tagd::uri_encode(id) + (address.descendants ? "/" : "") + address.query;
		return tagd::TAGD_OK;
	}

	if (id != _config->tagspace_id(tsp())) {
		rc = set_tagspace(true, false, target);
		if (rc != tagd::TAGD_OK)
			return rc;
	}
	operation = address.operation + address.query;
	return tagd::TAGD_OK;
}

void driver::scan_addressed_tagdurl(const std::string& input, int command) {
	tagdurl_address address;
	if (parse_tagdurl_address(input, address, *this) != tagd::TAGD_OK)
		return;

	// TODO: General scopes should share this statement-local selection lifetime.
	if (_selection)
		_tagdurl_previous = _selection->target;
	if (!_session)
		own_session(tsp()->new_session());
	_tagdurl_context = _session->context();
	std::string operation;
	if (select_tagdurl(address, operation, true) != tagd::TAGD_OK)
		return;

	// A separate parser prevents recursive reductions in the outer statement parser.
	driver inner(tsp(), _session);
	inner.scan_tagdurl(command, operation, tagdurl_mode::resolve_subject);
	inner.finish();
	// The outer statement owns restoration after its callback, including lookup contexts.
	inner._context_level = 0;
	if (inner.has_errors()) {
		copy_errors(inner);
		code(inner.code());
	} else if (inner.tag_ptr()) {
		delete_tag();
		tag_ptr(inner.tag_ptr());
		inner.tag_ptr(nullptr);
		_tagdurl_command = inner.cmd();
	}
}

void driver::configure(tagd::config& cfg, bool http, bool config_loading) {
	_config = &cfg;
	_http = http;
	// HTTP may reach config only when the operator made it the server default (the initial target).
	_http_config_allowed = http && tsp() == &cfg;
	_config_loading = config_loading;

	_selection = std::make_shared<selection>(selection{cfg.borrow(tsp())});
	if (!_session)
		own_session(tsp()->new_session());
	bind_membership();
}

// Local writes are unrestricted; HTTP writes need a mutating method and never target config.
bool driver::permit_write() {
	if (!_http || (_http_mutations && tsp() != _config))
		return true;
	_http_forbidden = true;
	error(tagd::TS_MISUSE, "configuration is read-only over HTTP");
	return false;
}

tagd::code driver::set_config_profile(const std::string& id) {
	if (!_config_loading || !_config)
		return error(tagd::TS_MISUSE, "_config_id requires config-file loading");

	// Includes share config ownership; the last SET retains its own context for deferred lookup.
	_config->file_profile(id, _session->context());
	return tagd::TAGD_OK;
}

void driver::unset_context() {
	if (_session)
		_session->clear_context();
	_context_level = 0;
}

tagd::code driver::set_tagspace(bool use, bool unset, const std::string& target,
	const std::string& alias) {
	if (!_config)
		return error(tagd::TS_MISUSE, "tagspace collection not configured");

	if (_http_target && use) {
		tagd::id_string id;
		auto rc = _config->resolve_tagspace(target, id, _session);
		if (rc != tagd::TAGD_OK || id != _config->tagspace_id(_http_target.get())) {
			_http_forbidden = true;
			return error(tagd::TS_MISUSE,
				tagd::predicate(HARD_TAG_CAUSED_BY, tagd::HARD_TAG_TAGSPACE, target));
		}
	}

	// Config files cannot select targets; HTTP cannot close shared instances or remove
	// aliases, and only POST/PUT may create aliases.
	if ((_config_loading && use) || (_http && (unset || (!alias.empty() && !_http_aliases)))) {
		_http_forbidden = _http;
		return error(tagd::TS_MISUSE, "tagspace operation prohibited in this execution");
	}

	/*
	 * TODO: Define scopes and transactions for TAGL file execution, including
	 * selection lifetime and coordinated rollback of data and tagspace lifecycle
	 * changes. Execution is currently sequential like _include: discarding a
	 * failed memory load does not undo earlier effects in other tagspaces or
	 * the application's registration state.
	 */
	auto load = [this](const std::string& file, std::shared_ptr<tagd::tagspace> candidate) {
		auto previous = _selection->target;
		_selection->target = candidate;
		auto rc = include_file(file);
		// Explicit script switches survive; the private candidate never escapes a failed load.
		if (_selection->target == candidate)
			_selection->target = std::move(previous);
		return rc;
	};

	// Relative locations resolve against the executing file's directory.
	const auto base = _path.empty() ? std::string{} : std::filesystem::path(_path).parent_path().string();
	std::shared_ptr<tagd::tagspace> next;
	auto access = !_http ? tagd::tagspace_access::local
		: (_http_config_allowed ? tagd::tagspace_access::registered_with_config
			: tagd::tagspace_access::registered_only);
	if (_http && _config->tagdurl_autoload())
		access = _http_config_allowed ? tagd::tagspace_access::named_with_config
			: tagd::tagspace_access::named_only;
	bool denied = false;
	auto rc = unset ? _config->close_tagspace(target, alias, _session, base)
		: _config->open_tagspace(target, alias, next, _session, base, access, load, &denied);
	if (rc != tagd::TAGD_OK) {
		copy_errors(*_config);
		_http_forbidden = _http && denied;
		if (!has_errors())
			error(rc, "cannot select tagspace: " + target);
		return code(rc);
	}

	if (use) {
		_selection->target = std::move(next);
	}
	return code(tagd::TAGD_OK);
}

tagd::code driver::push_context(tagd::id_view id) {
	if (_session == nullptr)
		own_session(tsp()->new_session());
	auto tc = _session->push_context(id);
	if (tc == tagd::TAGD_OK)
		_context_level++;
	return tc;
}

tagd::code driver::apply_tagdurl_context(const tagd::id_vec& contexts) {
	if (contexts.empty())
		return tagd::TAGD_OK;
	if (!_session)
		own_session(tsp()->new_session());

	const auto previous = _session->context();
	const auto level = _context_level;
	for (const auto& context : contexts) {
		const auto rc = push_context(context);
		if (rc != tagd::TAGD_OK) {
			// A failed list must not leave a partially applied statement context.
			_session->lookup_contexts(previous);
			_context_level = level;
			// HTTP already shares the session errors with this driver.
			if (!has_errors())
				copy_errors(*_session);
			return code(rc);
		}
	}
	return tagd::TAGD_OK;
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
	_set_statement = false;
	_tagspace_arguments = false;
	if (has_errors())
		do_callback();
	_scanner->reset();
	_token = -1;
	_path.clear();

	if (_callback != nullptr)
		_callback->finish();
}

bool driver::is_setup() const {
	return _scanner != nullptr && _parser != nullptr;  // unique_ptr::operator!= with nullptr
}

// looks up a pos type for a tag and returns
// its equivalent token
int driver::lookup_pos(const std::string& s) {
	if (_tagspace_arguments)
		return TOK_TAGSPACE_TARGET;
	// _use and _tagspace are keywords only after SET or SET DELETE, never ordinary IDs.
	if (_set_statement && (_token == TOK_CMD_SET || _token == TOK_CMD_DELETE)) {
		if (s == tagd::HARD_TAG_CONFIG_ID)
			return TOK_SET_CONFIG_ID;
		if (s == tagd::HARD_TAG_USE)
			return TOK_SET_USE;
		if (s == tagd::HARD_TAG_TAGSPACE)
			return TOK_SET_TAGSPACE;
	}
	if (_token == TOK_EQ) // '=' separates object and modifier
		return TOK_MODIFIER;

	int token;
	if (TAGL_TRACE_ON && _session != nullptr) {
		std::cerr << "context: ";
		_session->print_context();
	}
	tagd::part_of_speech pos = tsp()->pos(s, _session);
	TAGL_LOG_TRACE( "pos(" << s << "): " << pos_str(pos) << std::endl )

	// TODO term_pos lookups
	//tagd::part_of_speech term_pos = tsp()->term_pos(s);
	//TAGL_LOG_TRACE( "term_pos(" << s << "): " << pos_list_str(term_pos) << std::endl )

	switch(pos) {
		// Descendants inherit their boolean POS from storage, so spelling does
		// not determine truth (for example, certainly <: yes <: _true).
		case tagd::POS_TRUE:
			token = TOK_TRUE;
			break;
		case tagd::POS_FALSE:
			token = TOK_FALSE;
			break;
		case tagd::POS_TAG:
			token = TOK_TAG;
			break;
		case tagd::POS_TYPE_RELATOR:
			token = TOK_TYPE_RELATOR;
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

tagd::code driver::scan_tagdurl(int cmd, const std::string& path, tagdurl_mode mode) {
	this->init();
	scanner::tagdurl sc(this);
	sc.scan(cmd, path, mode);
	return this->code();
}

tagd::code driver::scan_tagdurl(tagd::http_method method, const std::string& path) {
	this->init();
	scanner::tagdurl sc(this);
	sc.scan(method, path);
	return this->code();
}

text_token driver::store_token_text(const std::string& s) {
	return _scanner->_token_store.store_text(s);
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

tagd::code driver::execute_store(const tagd::url& source, const std::string& predicates) {
	init();
	if (!source.ok()) {
		error(source.code(), "invalid STORE URL");
		finish();
		return code();
	}

	parse_tok(TOK_CMD_STORE, EMPTY_VALUE);
	parse_tok(TOK_URL, source.id());
	_predicate_fragment = true;
	_predicate_fragment_empty = true;
	if (!has_errors() && !predicates.empty()) {
		_scanner->scan(predicates);
		if (!has_errors() && _predicate_fragment_empty)
			error(tagd::TAGL_ERR, "STORE predicates are empty");
	}
	_predicate_fragment = false;

	// finish() supplies the terminator; selection, flags, and lookup context belong to the caller.
	finish();
	return code();
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

void callback::cmd_store(const tagd::abstract_tag& t) {
	tagd::store_result result;
	const auto rc = _driver->tsp()->store(result, t, _driver->session_ptr(), _driver->flags);
	_driver->code(rc);
	if (rc != tagd::TAGD_OK)
		cmd_error();
}

void callback::cmd_delete_storage(const tagd::abstract_tag& t) {
	tagd::filepile_delete_result result;
	const auto rc = _driver->tsp()->delete_storage(result, t, _driver->session_ptr(), _driver->flags);
	_driver->code(rc);
	if (rc != tagd::TAGD_OK)
		cmd_error();
}

void driver::do_callback() {
	struct restore {
		driver& owner;
		~restore() { owner.restore_tagdurl(); }
	} restore{*this};
	if (_cmd == TOK_CMD_GET && _tagdurl_command == TOK_CMD_QUERY)
		_cmd = TOK_CMD_QUERY;
	if (_callback == nullptr)  return;

	if (this->has_errors()) {
		// An error ends the statement. Scanner and parser may both report it; deliver once.
		_set_statement = false;
		_tagspace_arguments = false;
		if (_error_callback_delivered)
			return;

		_error_callback_delivered = true;
		_callback->cmd_error();
		return;
	}

	assert(_tag != nullptr);
	if (_tag == nullptr) {
		this->error(tagd::TAGL_ERR, "callback on NULL tag");
		return;
	}

	/*
	 * Check each resolved operation, not request text or the initial selection:
	 * aliases and earlier SET commands may have changed its actual target.
	 */
	if ((_cmd == TOK_CMD_PUT || _cmd == TOK_CMD_STORE ||
		_cmd == TOK_CMD_DELETE || _cmd == TOK_CMD_DELETE_STORAGE) && !permit_write()) {
		_callback->cmd_error();
		return;
	}
	// HTTP file acquisition authority is deferred; body TAGL must not expose it implicitly.
	if (_http && (_cmd == TOK_CMD_STORE || _cmd == TOK_CMD_DELETE_STORAGE)) {
		error(tagd::TS_NOT_IMPLEMENTED, "HTTP filepile invocation is not implemented");
		_callback->cmd_error();
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
		case TOK_CMD_STORE:
			_callback->cmd_store(*_tag);
			break;
		case TOK_CMD_DELETE_STORAGE:
			_callback->cmd_delete_storage(*_tag);
			break;
		case TOK_CMD_DELETE:
			TAGL_LOG_TRACE( "callback::cmd_delete: " << *_tag << std::endl )
			_callback->cmd_delete(*_tag);
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
	if (_http) {
		_http_forbidden = true;
		return error(tagd::TS_MISUSE, "HTTP cannot execute local include files");
	}

	// Record the absolute path so nested includes and SET locations resolve from this file.
	auto resolved_path = std::filesystem::path(path);
	if (resolved_path.is_relative()) {
		auto base = _path.empty() ? std::filesystem::current_path()
			: std::filesystem::path(_path).parent_path();
		resolved_path = std::filesystem::absolute(base / resolved_path);
	}

	int fd = this->open_rel(path, O_RDONLY);

	if (fd < 0)
		return this->code();

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return this->ferror(tagd::TAGL_ERR, "stat failed: %s", path.c_str());
	}

	if (st.st_size == 0) {
		// TODO create flag FAIL_OPEN_ZERO_LENGTH_FILE
		//D_CERR << "ignoring zero length file: "  << path << std::endl;
		if (fcntl(fd, F_GETFD) != -1)
			close(fd);
		return tagd::TAGD_OK;
	}

	struct evbuffer *input = evbuffer_new();
	tagd::code rc;
	// evbuffer_add_file closes fd for us
	if (evbuffer_add_file(input, fd, 0, st.st_size) == 0) {
		// create object of this type of driver to execute include file
		// TODO figure out how to make this work create new polymorphic object
		// auto *inc_driver = new decltype(this)(_tsp, _callback);
		// Includes share the session, selection, and execution policy of the including driver.
		driver inc_driver(tsp(), _callback, _session);
		inc_driver._selection = _selection;
		inc_driver._config = _config;
		inc_driver._http = _http;
		inc_driver._config_loading = _config_loading;
		// Includes inherit execution flags; changes inside the file remain local.
		inc_driver.flags = flags;
		inc_driver.path(resolved_path.string());
		rc = inc_driver.execute(input);
		if (inc_driver.has_errors())
			this->copy_errors(inc_driver);
	} else {
		rc = this->ferror(tagd::TAGL_ERR, "evbuffer_add_file failed: %s", path.c_str());
	}

	this->bind_callback(_callback);

	evbuffer_free(input);
	return code(rc);

}

tagd::code driver::new_url(const std::string& url, tagd::id_view sub_relator,
	tagd::id_view super_object) {
	this->delete_tag();

	auto *u = sub_relator.empty() && super_object.empty() ? new tagd::url(url)
		: new tagd::url(url, sub_relator, super_object);
	if (!u->ok()) {
		const auto code = u->code();
		delete u;
		return this->ferror(code, "parse URL failed: %s", url.c_str());
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
