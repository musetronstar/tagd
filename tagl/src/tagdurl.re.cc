#include "tagl.h"

/*!include:re2c "../include/scanner.h" */

namespace TAGL {

void scanner::tagdurl::scan(int cmd, const std::string& path) {
	if (path.empty() || path == "/") {
		_driver->error(tagd::TAGL_ERR, "tag id required in path");
		return;
	}

	if (path[0] == '?') {
		if (cmd == TOK_CMD_GET) {
			_driver->cmd(cmd);
			this->scanner::scan(path);
		} else {
			_driver->error(tagd::TS_MISUSE, "unhandled command");
		}
		return;
	}

	if (path[0] != '/') {
		_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
		return;
	}

	if (cmd == TOK_CMD_PUT) {
		if (path.find('?') != std::string::npos) {
			_driver->error(tagd::TS_MISUSE, "unhandled command");
			return;
		}

		const std::string id = tagd::uri_decode(path.substr(1));
		if (id.find('/') != std::string::npos) {
			_driver->error(tagd::TAGL_ERR, "malformed path: trailing '/'");
			return;
		}

		_driver->constrain_tag_id = id;
		return;
	}

	if (cmd == TOK_CMD_GET || cmd == TOK_CMD_DEL) {
		_driver->cmd(cmd);
		this->scanner::scan(tagd::uri_decode(path.substr(1)));
	} else {
		_driver->error(tagd::TS_MISUSE, "unhandled command");
	}
}

void scanner::tagdurl::scan(tagd::http_method method, const std::string& path) {
	this->scan(method_command(method), path);
}

void scanner::tagdurl::scan(const char *cur, size_t sz) {
	begin_scan(cur, sz);
	if (_driver->has_errors())
		return;

	(void)cur;
	(void)sz;
	bool query_started = false;
	std::string subject;
	std::string relation;

	auto decode_query_value = [](std::string s) {
		s = tagd::uri_decode(s);
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '+')
				s[i] = ' ';
		}
		return s;
	};

	auto emit_lookup_token = [this](const std::string& s) {
		_driver->parse_tok(_driver->lookup_pos(s), s);
	};

	auto emit_subject = [this](const std::string& s, int tok) {
		_driver->parse_tok(_driver->cmd(), EMPTY_VALUE);
		_driver->parse_tok(tok, s);
	};

	auto emit_lookup_subject = [&, this](const std::string& s) {
		_driver->parse_tok(_driver->cmd(), EMPTY_VALUE);
		emit_lookup_token(s);
	};

	auto emit_query_subject = [&, this](const std::string& s) {
		query_started = true;
		_driver->parse_tok(TOK_CMD_QUERY, EMPTY_VALUE);
		_driver->parse_tok(TOK_INTERROGATOR, HARD_TAG_INTERROGATOR.data());
		if (s != "*") {
			_driver->parse_tok(TOK_SUB_RELATOR, HARD_TAG_SUB.data());
			emit_lookup_token(s);
		}
	};

	auto emit_query_relation = [&, this](const std::string& s) {
		emit_lookup_token(s);
	};

	auto emit_query_relation_modifier = [&, this](const std::string& s) {
		_driver->parse_tok(TOK_EQ, EMPTY_VALUE);
		emit_lookup_token(s);
	};

	auto emit_query_relations_begin = [this]() {
		_driver->parse_tok(TOK_WILDCARD, EMPTY_VALUE);
	};

	auto emit_query_option_search = [&, this](const std::string& s) {
		_driver->parse_tok(TOK_RELATOR, HARD_TAG_HAS.data());
		_driver->parse_tok(TOK_TAG, HARD_TAG_TERMS.data());
		_driver->parse_tok(TOK_EQ, EMPTY_VALUE);
		_driver->parse_tok(TOK_QUOTED_STR, decode_query_value(s));
	};

	auto emit_root_search = [&, this](const std::string& s) {
		query_started = true;
		_driver->parse_tok(TOK_CMD_QUERY, EMPTY_VALUE);
		_driver->parse_tok(TOK_QUOTED_STR, decode_query_value(s));
	};

	auto emit_query_option_context = [&, this](const std::string& s) {
		(void)_driver->push_context(decode_query_value(s));
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

	HDURI                 { emit_subject(std::string(_beg, _cur - _beg), TOK_HDURI); return; }
	EVURI                 { emit_subject(std::string(_beg, _cur - _beg), TOK_EVURI); return; }
	ERRURI                { emit_subject(std::string(_beg, _cur - _beg), TOK_ERRURI); return; }
	URL                   { emit_subject(std::string(_beg, _cur - _beg), TOK_URL); return; }
	"?"                  { advance_begin(); goto query_opts; }
	TAG_SEG               {
		subject.assign(_beg, _cur - _beg);
		advance_begin();
		goto subject_tail;
	}
	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/

subject_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	"?"                  {
		if (_driver->cmd() != TOK_CMD_GET) {
			_driver->error(tagd::TS_MISUSE, "unhandled command");
			return;
		}
		emit_lookup_subject(subject);
		advance_begin();
		goto query_opts;
	}
	"/"                  { emit_query_subject(subject); advance_begin(); goto query_tail; }
	[\000]               { emit_lookup_subject(subject); return; }
	[^]                  { emit_error(); return; }
*/

query_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG              {
		emit_query_relations_begin();
		relation.assign(_beg, _cur - _beg);
		advance_begin();
		goto query_rel_tail;
	}
	"?"                  { advance_begin(); goto query_opts; }
	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/

query_rel_tail:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	"="                  { emit_query_relation(relation); advance_begin(); goto query_rel_modifier; }
	","                  { emit_query_relation(relation); _driver->parse_tok(TOK_COMMA, EMPTY_VALUE); advance_begin(); goto query_rel; }
	"?"                  { emit_query_relation(relation); advance_begin(); goto query_opts; }
	[\000]               { emit_query_relation(relation); return; }
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

	","                  { _driver->parse_tok(TOK_COMMA, EMPTY_VALUE); advance_begin(); goto query_rel; }
	"?"                  { advance_begin(); goto query_opts; }
	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/

query_opts:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	// re2c rules cannot reference the C++ QUERY_OPT_* constants directly,
	// so the TAGL-mapped query opts are named here explicitly.
	QUERY_OPT_SEARCH_PAIR = "q=" [^&\000]*;
	QUERY_OPT_CONTEXT_PAIR = "c=" [^&\000]*;

	QUERY_OPT_SEARCH_PAIR {
		const std::string value(
			_beg + QUERY_OPT_SEARCH.size() + 1,
			_cur - _beg - QUERY_OPT_SEARCH.size() - 1);
		if (query_started)
			emit_query_option_search(value);
		else
			emit_root_search(value);
		advance_begin();
		goto query_opt_tail;
	}
	QUERY_OPT_CONTEXT_PAIR {
		emit_query_option_context(
			std::string(_beg + QUERY_OPT_CONTEXT.size() + 1,
				_cur - _beg - QUERY_OPT_CONTEXT.size() - 1));
		advance_begin();
		goto query_opt_tail;
	}
	[^=&\000]+           { advance_begin(); goto query_opt_tail; }
	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/

query_opt_tail:
/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	"&"                  { advance_begin(); goto query_opts; }
	[\000]               { return; }
	[^]                  { emit_error(); return; }
*/
}

} // namespace TAGL
