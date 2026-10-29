#include "tagl.h"

/*!include:re2c "../include/scanner.h" */

namespace TAGL {

static bool parse_tagdurl_options(url_query_map_t& values, const std::string& options) {
	const char* YYCURSOR = options.c_str();
	const char* begin = YYCURSOR;

option:
	begin = YYCURSOR;
/*!re2c
	re2c:define:YYCTYPE = "char";
	re2c:yyfill:enable = 0;

	// These spellings mirror QUERY_OPT_SEARCH and QUERY_OPT_CONTEXT; re2c needs literals.
	("q=" | "c=") [^&\000]* {
		values[std::string(begin, 1)] = tagd::url::decode_query_value(
			std::string(begin + 2, YYCURSOR));
		goto option_tail;
	}
	[^=&\000]+              { values[std::string(begin, YYCURSOR)] = ""; goto option_tail; }
	[\000]                  { return true; }
	[^]                     { return false; }
*/
option_tail:
/*!re2c
	"&"                     { goto option; }
	[\000]                  { return true; }
	[^]                     { return false; }
*/
}

static tagd::code parse_tagdurl_options(const std::string& options, url_query_map_t& values,
	tagd::id_vec& contexts, tagd::errorable& errors) {
	values.clear();
	contexts.clear();
	if (!parse_tagdurl_options(values, options))
		return errors.error(tagd::TAGL_ERR, "malformed tagdurl options");

	std::string value;
	if (!tagd::url::query_find(values, value, QUERY_OPT_CONTEXT) || value.empty())
		return tagd::TAGD_OK;

	size_t start = 0;
	while (true) {
		const auto comma = value.find(',', start);
		const auto context = value.substr(start, comma - start);
		if (context.empty()) {
			contexts.clear();
			return errors.error(tagd::TAGL_ERR, "empty context in tagdurl context list");
		}
		contexts.emplace_back(context);
		if (comma == std::string::npos)
			return tagd::TAGD_OK;
		start = comma + 1;
	}
}

tagd::code tagdurl_contexts(const std::string& path, tagd::id_vec& contexts,
	tagd::errorable& errors) {
	const auto query = path.find('?');
	url_query_map_t options;
	// Pre-scan selector resolution and root POST must obey the scanner's option syntax.
	return parse_tagdurl_options(query == std::string::npos ? "" : path.substr(query + 1),
		options, contexts, errors);
}

void scanner::tagdurl::scan(int cmd, const std::string& path, tagdurl_mode mode) {
	_options.clear();
	_contexts.clear();
	_command = cmd == TOK_CMD_STORE ? TOK_CMD_PUT
		: cmd == TOK_CMD_DELETE_STORAGE ? TOK_CMD_DELETE : cmd;
	_mode = mode;
	if (_command != TOK_CMD_GET && _command != TOK_CMD_PUT && _command != TOK_CMD_DELETE) {
		_driver->error(tagd::TS_MISUSE, "unhandled command");
		return;
	}

	const auto query = path.find('?');
	const auto subject_path = path.substr(0, query);
	if (query != std::string::npos
		&& parse_tagdurl_options(path.substr(query + 1), _options, _contexts, *_driver)
			!= tagd::TAGD_OK)
		return;
	if (subject_path.empty() && query == std::string::npos) {
		_driver->error(tagd::TAGL_ERR, "tag id required in path");
		return;
	}
	if (!subject_path.empty() && subject_path.front() != '/') {
		_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
		return;
	}
	if (subject_path == "/" && _options.empty()) {
		_driver->error(tagd::TAGL_ERR, "tag id required in path");
		return;
	}

	this->scanner::scan(tagd::uri_decode(subject_path.empty() ? "" : subject_path.substr(1)));
}

void scanner::tagdurl::scan(tagd::http_method method, const std::string& path) {
	this->scan(method_command(method), path, tagdurl_mode::constrain_body);
}

void scanner::tagdurl::scan(const char *cur, size_t sz) {
	begin_scan(cur, sz);
	if (_driver->has_errors())
		return;

	(void)cur;
	(void)sz;
	// An embedded PUT/STORE resolves its subject through GET grammar; its outer statement writes.
	const int parse_command = _command == TOK_CMD_PUT ? TOK_CMD_GET : _command;
	bool query_started = false;
	std::string subject;
	std::string relation;
	tagd::id_vec types;
	std::vector<std::pair<int, std::string>> tokens;
	// Defer POS lookup and parser actions until syntax, command, and context are valid.
	constexpr int lookup_token = -1;
	auto emit = [&](int token, const std::string& value) {
		tokens.emplace_back(token, value);
	};
	auto emit_lookup_token = [&](const std::string& value) {
		emit(lookup_token, value);
	};
	auto emit_subject = [&](const std::string& value, int token) {
		subject = value;
		emit(parse_command, "");
		emit(token, value);
	};
	auto emit_query_subject = [&]() {
		query_started = true;
		emit(TOK_CMD_QUERY, "");
		emit(TOK_INTERROGATOR, std::string(HARD_TAG_INTERROGATOR));
		if (!subject.empty() && subject != "*") {
			emit(TOK_SUB_RELATOR, std::string(HARD_TAG_SUB));
			emit_lookup_token(subject);
		}
		if (!types.empty()) {
			emit(TOK_TYPE_RELATOR, std::string(tagd::HARD_TAG_TYPE_OF));
			for (size_t i = 0; i < types.size(); ++i) {
				if (i != 0)
					emit(TOK_COMMA, "");
				emit_lookup_token(types[i]);
			}
		}
	};
	auto emit_query_relation_modifier = [&](const std::string& value) {
		emit(TOK_EQ, "");
		emit_lookup_token(value);
	};

#define YYCURSOR _cur
#define YYLIMIT	_lim
#define YYGETSTATE()    _state
#define YYSETSTATE(x)   { _state = (x);  }
#define	YYFILL(n)	{ if(_do_fill && _evbuf && !_eof){ this->fill(); if(_driver->has_errors()) return; } }
#define YYMARKER        _mark
#define YYDEBUG(s,c) { if(TAGL_TRACE_ON) LOG_DEBUG("yydebug: s = " << s << ", c = " << c << std::endl) }

/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	!use:tagl_defs;

	// Keep tag segments loose like TAGL tokens; the following *_tail states
	// decide where a tag ends based on the next structural delimiter.
	TAG_SEG = [^\000 \t\r\n/?&,=]+;

	HDURI                 { emit_subject(std::string(_beg, _cur - _beg), TOK_HDURI); goto complete; }
	EVURI                 { emit_subject(std::string(_beg, _cur - _beg), TOK_EVURI); goto complete; }
	ERRURI                { emit_subject(std::string(_beg, _cur - _beg), TOK_ERRURI); goto complete; }
	URL                   { emit_subject(std::string(_beg, _cur - _beg), TOK_URL); goto complete; }
	","                  { advance_begin(); goto type_object; }
	TAG_SEG               {
		subject.assign(_beg, _cur - _beg);
		advance_begin();
		goto subject_tail;
	}
	[\000]               { goto complete; }
	[^]                  { emit_error(); return; }
*/

subject_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	","                  {
		// TODO: Define '*' with a type list once '*' means "is related to".
		if (subject == "*") {
			emit_error();
			return;
		}
		advance_begin();
		goto type_object;
	}
	"/"                  { emit_query_subject(); advance_begin(); goto query_tail; }
	[\000]               { emit_subject(subject, lookup_token); goto complete; }
	[^]                  { emit_error(); return; }
*/

type_object:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG              { types.emplace_back(_beg, _cur - _beg); advance_begin(); goto type_tail; }
	[^]                  { emit_error(); return; }
*/

type_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	","                  { advance_begin(); goto type_object; }
	"/"                  { emit_query_subject(); advance_begin(); goto query_tail; }
	[\000]               { goto complete; }
	[^]                  { emit_error(); return; }
*/

query_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG              {
		emit(TOK_WILDCARD, "");
		relation.assign(_beg, _cur - _beg);
		advance_begin();
		goto query_rel_tail;
	}
	[\000]               { goto complete; }
	[^]                  { emit_error(); return; }
*/

query_rel_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	"="                  { emit_lookup_token(relation); advance_begin(); goto query_rel_modifier; }
	","                  { emit_lookup_token(relation); emit(TOK_COMMA, ""); advance_begin(); goto query_rel; }
	[\000]               { emit_lookup_token(relation); goto complete; }
	[^]                  { emit_error(); return; }
*/

query_rel:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG              {
		relation.assign(_beg, _cur - _beg);
		advance_begin();
		goto query_rel_tail;
	}
	[^]                  { emit_error(); return; }
*/

query_rel_modifier:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG              {
		emit_query_relation_modifier(std::string(_beg, _cur - _beg));
		advance_begin();
		goto query_rel_value_tail;
	}
	[^]                  { emit_error(); return; }
*/

query_rel_value_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	","                  { emit(TOK_COMMA, ""); advance_begin(); goto query_rel; }
	[\000]               { goto complete; }
	[^]                  { emit_error(); return; }
*/

complete:
	// A malformed path must fail before an otherwise incompatible write command.
	if (!types.empty() && !query_started) {
		_driver->error(tagd::TS_MISUSE, "type lists require a QUERY tagdurl");
		return;
	}
	if (query_started && _command != TOK_CMD_GET) {
		_driver->error(tagd::TS_MISUSE, "QUERY tagdurls require a read command");
		return;
	}

	std::string search;
	if (tagd::url::query_find(_options, search, QUERY_OPT_SEARCH)) {
		if (_command != TOK_CMD_GET) {
			_driver->error(tagd::TS_MISUSE, "search requires QUERY");
			return;
		}
		if (query_started) {
			emit(TOK_RELATOR, std::string(HARD_TAG_HAS));
			emit(TOK_TAG, std::string(HARD_TAG_TERMS));
			emit(TOK_EQ, "");
		} else if (subject.empty()) {
			emit(TOK_CMD_QUERY, "");
		} else {
			_driver->error(tagd::TAGL_ERR, "search requires a QUERY path");
			return;
		}
		emit(TOK_QUOTED_STR, search);
	}

	if (tokens.empty()) {
		_driver->error(tagd::TAGL_ERR, "tag id required in path");
		return;
	}
	if (_command == TOK_CMD_PUT && _mode == tagdurl_mode::constrain_body
		&& !subject.starts_with(tagd::HDURI_SCHEME) && subject.find('/') != std::string::npos) {
		_driver->error(tagd::TAGL_ERR, "only HDURI tag IDs may contain '/' in PUT paths");
		return;
	}
	if (_driver->apply_tagdurl_context(_contexts) != tagd::TAGD_OK)
		return;
	if (_command == TOK_CMD_PUT && _mode == tagdurl_mode::constrain_body) {
		// HTTP composes the body; an HDURI subject may contain its URL's slashes.
		_driver->constrain_tag_id = subject;
		return;
	}
	_driver->cmd(parse_command);
	for (const auto& [token, value] : tokens) {
		_driver->parse_tok(token == lookup_token ? _driver->lookup_pos(value) : token, value);
		if (_driver->has_errors())
			return;
	}
}

} // namespace TAGL
