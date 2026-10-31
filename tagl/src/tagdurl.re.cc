#include "tagl.h"
#include <algorithm>

/*!include:re2c "../include/scanner.h" */

namespace TAGL {

// POS lookup is deferred until the complete path and context have been validated.
constexpr int lookup_token = -1;

/*!rules:re2c:tagdurl_defs
	TAG_SEG = [^\000 \t\r\n"/?&,=]+;
*/

struct tagdurl_id {
	int token;
	size_t size;
};

static bool tagdurl_id_byte(char c) {
	// This is TAG_SEG's byte set, also used after the sixth event separator.
	return c != '\0' && std::string_view(" \t\r\n\"/?&,=").find(c) == std::string_view::npos;
}

static std::optional<tagdurl_id> lex_tagdurl_id(std::string_view input) {
	int token = lookup_token;
	size_t prefix = 0;
	for (const auto& [scheme, kind] : {
		std::pair{std::string_view(tagd::HDURI_SCHEME), TOK_HDURI},
		std::pair{std::string_view(tagd::EVURI_SCHEME), TOK_EVURI},
		std::pair{std::string_view(tagd::ERRURI_SCHEME), TOK_ERRURI}}) {
		if (input.starts_with(scheme)) {
			token = kind;
			prefix = scheme.size();
			break;
		}
	}

	size_t end = prefix;
	if (token != lookup_token) {
		/*
		 * A reserved prefix commits to its field grammar. Failure must never retry
		 * a shorter plain token: repeated prefixes would rescan suffixes quadratically.
		 */
		const auto separators = token == TOK_HDURI ? tagd::HDURI_DELIM_COUNT
			: tagd::EVURI_NUM_ELEMS - 1;
		for (size_t field = 0; field < separators; ++field) {
			while (end < input.size() && input[end] != tagd::HDURI_DELIM) {
				if (input[end] == '\0' || std::string_view(" \t\r\n\"").find(input[end])
					!= std::string_view::npos)
					return std::nullopt;
				++end;
			}
			if (end == input.size())
				return std::nullopt;
			++end;
		}
	}

	if (token == TOK_HDURI) {
		// Core URL parsing owns scheme syntax; the HDURI token ends where its scheme ends.
		if (end == input.size() || !tagd::url_scheme_initial(input[end]))
			return std::nullopt;
		while (end < input.size() && tagd::url_scheme_byte(input[end])) {
			++end;
		}
		if (end < input.size() && tagdurl_id_byte(input[end]))
			return std::nullopt;
	} else {
		while (end < input.size() && tagdurl_id_byte(input[end])) {
			++end;
		}
	}

	if (end == 0)
		return std::nullopt;
	return tagdurl_id{token, end};
}

size_t tagdurl_query_offset(const std::string& path) {
	bool modifier = false;
	for (size_t offset = 0; offset < path.size();) {
		if (path[offset] == '?')
			return offset;
		if (!tagdurl_id_byte(path[offset])) {
			// Outside structured fields, '=' only separates a relation from its modifier text.
			modifier = path[offset] == '=';
			++offset;
			continue;
		}

		// Modifier values are text, so a reserved prefix there starts no structured fields.
		if (modifier) {
			while (offset < path.size() && tagdurl_id_byte(path[offset])) {
				++offset;
			}
			modifier = false;
			continue;
		}

		const auto id = lex_tagdurl_id(std::string_view(path).substr(offset));
		// The scanner reports malformed IDs; splitting must stop instead of retrying suffixes.
		if (!id)
			return std::string::npos;
		offset += id->size;
	}
	return std::string::npos;
}

static bool reserved_uri_prefix(std::string_view text) {
	const auto colon = text.find(':');
	if (colon != 2 && colon != 3)
		return false;

	std::string prefix(text.substr(0, colon + 1));
	for (auto& c : prefix) {
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	}
	return prefix == tagd::HDURI_SCHEME || prefix == tagd::EVURI_SCHEME
		|| prefix == tagd::ERRURI_SCHEME;
}

static bool decode_tagdurl_id(std::string& decoded, const std::string& encoded, int token) {
	if (token == lookup_token) {
		if (!tagd::uri_decode_checked(decoded, encoded))
			return false;
		// Encoded or recased prefixes cannot acquire a second canonical decoding layer.
		return !reserved_uri_prefix(decoded);
	}

	const size_t prefix = token == TOK_ERRURI ? tagd::ERRURI_SCHEME.size()
		: tagd::HDURI_SCHEME.size();
	const size_t separators = token == TOK_HDURI ? tagd::HDURI_DELIM_COUNT
		: tagd::EVURI_NUM_ELEMS - 1;
	decoded = encoded.substr(0, prefix);
	size_t start = prefix;
	for (size_t field = 0; field <= separators; ++field) {
		const auto end = encoded.find(tagd::HDURI_DELIM, start);
		// Exactly the final field must have no remaining separator.
		if ((end == std::string::npos) != (field == separators))
			return false;

		const auto value = encoded.substr(start, end - start);
		// This complete field is a presence marker, never an encoded NUL byte.
		if (token == TOK_HDURI && field == tagd::HDURI_PASS
			&& value == tagd::HDURI_EMPTY_PASSWORD) {
			decoded += value;
		} else {
			std::string raw;
			if (!tagd::uri_decode_checked(raw, value))
				return false;
			decoded += tagd::encode_hduri_delim(raw);
		}

		if (field != separators)
			decoded += tagd::HDURI_DELIM;
		start = end + 1;
	}
	return true;
}

// RFC 3986 path bytes: pchar plus the path separator.
constexpr std::string_view http_path_punctuation = "-._~!$&'()*+,;=:@/";

static bool http_path_byte(unsigned char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9') || http_path_punctuation.find(c)
			!= std::string_view::npos;
}

std::string encode_tagdurl_tag_id(tagd::id_view id) {
	const bool hduri = id.starts_with(tagd::HDURI_SCHEME);
	const bool event = id.starts_with(tagd::EVURI_SCHEME) || id.starts_with(tagd::ERRURI_SCHEME);
	const size_t fields = hduri ? tagd::HDURI_DELIM_COUNT : tagd::EVURI_NUM_ELEMS - 1;
	// Canonical IDs have an exact separator count; only the final event field has ID boundaries.
	const bool structured = (hduri || event)
		&& static_cast<size_t>(std::count(id.begin(), id.end(), tagd::HDURI_DELIM)) == fields;
	const size_t escape_size = tagd::HDURI_PERCENT_ENCODED.size();
	size_t field = 0;
	std::string result;
	result.reserve(id.size());

	for (size_t i = 0; i < id.size(); ++i) {
		if (structured && id[i] == tagd::HDURI_DELIM)
			++field;
		const auto escape = id.substr(i, escape_size);
		const bool empty_password = hduri && field == tagd::HDURI_PASS && escape == tagd::HDURI_EMPTY_PASSWORD
			&& i > 0 && id[i - 1] == tagd::HDURI_DELIM && i + escape_size < id.size()
			&& id[i + escape_size] == tagd::HDURI_DELIM;
		// Canonical field escapes already represent one layer; transport must not add another.
		if (structured && (escape == tagd::HDURI_DELIM_ENCODED
			|| escape == tagd::HDURI_PERCENT_ENCODED || empty_password)) {
			result += escape;
			i += escape_size - 1;
			continue;
		}

		const bool field_allows_delimiters = structured && (!event || field < fields);
		tagd::percent_encode(result, id.substr(i, 1), [field_allows_delimiters](unsigned char c) {
			const bool id_delimiter = std::string_view("/,=&").find(c) != std::string_view::npos;
			return http_path_byte(c) && (field_allows_delimiters || !id_delimiter);
		});
	}
	return result;
}

std::string encode_tagdurl_option(std::string_view value) {
	auto result = tagd::percent_encode(value, [](unsigned char c) {
		return (http_path_byte(c) || c == '?' || c == ' ') && c != '&' && c != '+';
	});
	// Original '+' bytes are already escaped, so only raw spaces become form separators.
	std::replace(result.begin(), result.end(), ' ', '+');
	return result;
}

tagd::code parse_tagdurl_options(const std::string& options, tagdurl_option_map& values,
	tagd::id_vec& contexts, tagd::errorable& errors) {
	values.clear();
	contexts.clear();

	size_t start = 0;
	while (start < options.size()) {
		const auto end = options.find('&', start);
		const auto equal = options.find('=', start);
		if (equal == std::string::npos || equal >= end)
			return errors.error(tagd::TAGL_ERR, "malformed tagdurl option");

		const auto name = options.substr(start, equal - start);
		if (name != QUERY_OPT_SEARCH && name != QUERY_OPT_CONTEXT && name != QUERY_OPT_VIEW
			&& name != QUERY_OPT_URL)
			return errors.error(tagd::TAGL_ERR, "unknown tagdurl option: " + name);
		if (values.contains(name))
			return errors.error(tagd::TAGL_ERR, "duplicate tagdurl option: " + name);

		// _url owns the remaining raw bytes; later '&key=' text belongs to that URL.
		if (name == QUERY_OPT_URL) {
			values.emplace(name, options.substr(equal + 1));
			break;
		}

		auto encoded = options.substr(equal + 1, end - equal - 1);
		std::replace(encoded.begin(), encoded.end(), '+', ' ');
		std::string value;
		if (!tagd::uri_decode_checked(value, encoded))
			return errors.error(tagd::TAGL_ERR, "invalid escape in tagdurl option: " + name);

		values.emplace(name, std::move(value));
		if (end == std::string::npos)
			break;

		start = end + 1;
		if (start == options.size())
			return errors.error(tagd::TAGL_ERR, "empty tagdurl option");
	}

	const auto found = values.find(QUERY_OPT_CONTEXT);
	if (found == values.end() || found->second.empty())
		return tagd::TAGD_OK;

	const auto& value = found->second;
	start = 0;
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

void scanner::tagdurl::scan(int cmd, const std::string& path, tagdurl_mode mode) {
	const auto query = tagdurl_query_offset(path);
	tagdurl_address address;
	if (query != std::string::npos) {
		address.query = path.substr(query);
		if (parse_tagdurl_options(address.query.substr(1), address.options, address.contexts,
				*_driver) != tagd::TAGD_OK)
			return;
	}

	this->scan(cmd, path.substr(0, query), address, mode);
}

void scanner::tagdurl::scan(int cmd, const std::string& subject_path,
	const tagdurl_address& address, tagdurl_mode mode) {
	_options = address.options;
	_contexts = address.contexts;
	_command = cmd == TOK_CMD_STORE ? TOK_CMD_PUT
		: cmd == TOK_CMD_DELETE_STORAGE ? TOK_CMD_DELETE : cmd;
	_mode = mode;
	if (_command != TOK_CMD_GET && _command != TOK_CMD_PUT && _command != TOK_CMD_DELETE) {
		_driver->error(tagd::TS_MISUSE, "unhandled command");
		return;
	}

	const bool query = !address.query.empty();
	if (_mode == tagdurl_mode::resolve_subject && _options.contains(QUERY_OPT_VIEW)) {
		// TODO: TASKS.d/2026-10-30-TAGL-views-templates-and-pipe-operators.md
		_driver->error(tagd::TS_NOT_IMPLEMENTED, "TAGL tagdurl views are not implemented");
		return;
	}
	if (subject_path.empty() && !query) {
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

	this->scanner::scan(subject_path.empty() ? "" : subject_path.substr(1));
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
	int subject_token = lookup_token;
	std::optional<size_t> subject_index;
	std::string relation;
	int relation_token = lookup_token;
	std::vector<std::pair<int, std::string>> types;
	std::vector<std::pair<int, std::string>> tokens;
	// Defer POS lookup and parser actions until syntax, command, and context are valid.
	auto emit = [&](int token, const std::string& value) {
		tokens.emplace_back(token, value);
	};
	auto read_id = [&](std::string& value, int& token) {
		const auto id = lex_tagdurl_id(std::string_view(_beg, _lim - _beg));
		if (!id) {
			emit_error();
			return false;
		}
		_cur = _beg + id->size;
		value.assign(_beg, id->size);
		token = id->token;
		return true;
	};
	auto emit_subject = [&](const std::string& value, int token) {
		subject = value;
		emit(parse_command, "");
		subject_index = tokens.size();
		emit(token, value);
	};
	auto emit_query_subject = [&]() {
		query_started = true;
		emit(TOK_CMD_QUERY, "");
		emit(TOK_INTERROGATOR, std::string(HARD_TAG_INTERROGATOR));
		if (!subject.empty() && subject != "*") {
			emit(TOK_SUB_RELATOR, std::string(HARD_TAG_SUB));
			emit(subject_token, subject);
		}
		if (!types.empty()) {
			emit(TOK_TYPE_RELATOR, std::string(tagd::HARD_TAG_TYPE_OF));
			for (size_t i = 0; i < types.size(); ++i) {
				if (i != 0)
					emit(TOK_COMMA, "");
				emit(types[i].first, types[i].second);
			}
		}
	};
	auto emit_query_relation_modifier = [&](const std::string& value) {
		emit(TOK_EQ, "");
		emit(TOK_MODIFIER, value);
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

	!use:tagdurl_defs;

	","                  { advance_begin(); goto type_object; }
	TAG_SEG                {
		if (!read_id(subject, subject_token)) return;
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
	[\000]               { emit_subject(subject, subject_token); goto complete; }
	[^]                  { emit_error(); return; }
*/

type_object:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG               {
		types.emplace_back();
		if (!read_id(types.back().second, types.back().first)) return;
		advance_begin();
		goto type_tail;
	}
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

	TAG_SEG               {
		emit(TOK_WILDCARD, "");
		if (!read_id(relation, relation_token)) return;
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

	"="                  { emit(relation_token, relation); advance_begin(); goto query_rel_modifier; }
	","                  { emit(relation_token, relation); emit(TOK_COMMA, ""); advance_begin(); goto query_rel; }
	[\000]               { emit(relation_token, relation); goto complete; }
	[^]                  { emit_error(); return; }
*/

query_rel:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG               {
		if (!read_id(relation, relation_token)) return;
		advance_begin();
		goto query_rel_tail;
	}
	[^]                  { emit_error(); return; }
*/

query_rel_modifier:
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:yyfill:enable   = 1;

	TAG_SEG               {
		// Modifier values are text; tagdurl_query_offset lexes them the same way.
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

	const auto search_option = _options.find(QUERY_OPT_SEARCH);
	if (search_option != _options.end()) {
		const auto& search = search_option->second;
		if (_command != TOK_CMD_GET) {
			_driver->error(tagd::TS_MISUSE, "search requires QUERY");
			return;
		}
		if (query_started) {
			emit(TOK_RELATOR, std::string(HARD_TAG_HAS));
			emit(TOK_TAG, std::string(HARD_TAG_TERMS));
			emit(TOK_EQ, "");
		} else if (subject.empty()) {
			if (search.empty()) {
				_driver->error(tagd::TAGL_ERR, "root search requires nonempty terms");
				return;
			}
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

	// Decode only complete tokens: escaped delimiters cannot become path structure.
	size_t placeholders = 0;
	const auto url_option = _options.find(QUERY_OPT_URL);
	for (auto& [token, value] : tokens) {
		if (token != lookup_token && token != TOK_MODIFIER
			&& token != TOK_HDURI && token != TOK_EVURI && token != TOK_ERRURI)
			continue;

		if (token == lookup_token && value == ":_url:") {
			if (++placeholders > 1 || url_option == _options.end()) {
				_driver->error(tagd::TAGL_ERR, "one :_url: placeholder and _url option required");
				return;
			}
			const tagd::url supplied(url_option->second);
			if (!supplied.ok()) {
				_driver->error(tagd::TAGL_ERR, "invalid _url value");
				return;
			}
			value = supplied.hduri();
			token = TOK_HDURI;
		}

		std::string decoded;
		const bool valid = token == TOK_MODIFIER ? tagd::uri_decode_checked(decoded, value)
			: decode_tagdurl_id(decoded, value, token);
		if (!valid || (token != TOK_MODIFIER
			&& tagd::valid_tag_id(decoded) != tagd::tag_id_rule::valid)) {
			_driver->error(tagd::TAGL_ERR,
				tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, value));
			return;
		}

		value = std::move(decoded);
		// Only actual URL spellings need core classification; ordinary labels do not.
		if (token == lookup_token
			&& (tagd::url::looks_like_url(value) || tagd::url::looks_like_mailto(value)))
			token = TOK_URL;
	}

	if (url_option != _options.end() && placeholders != 1) {
		_driver->error(tagd::TAGL_ERR, "_url option requires one :_url: placeholder");
		return;
	}

	if (_driver->apply_tagdurl_context(_contexts) != tagd::TAGD_OK)
		return;

	if (_command == TOK_CMD_PUT && _mode == tagdurl_mode::constrain_body) {
		// Earlier checks route every accepted PUT through subject_tail; never read an absent index.
		if (!subject_index) {
			_driver->error(tagd::TAGL_ERR, "tag id required in path");
			return;
		}

		// HTTP composes the body using the same decoded identity as GET.
		_driver->constrain_tag_id = tokens[*subject_index].second;
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
