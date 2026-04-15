#include <cassert>
#include <cstring>
#include <iostream>

#include "tagl.h"
#include <event2/buffer.h>

namespace TAGL {

static std::string scanner_debug_quote(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 2);
	out.push_back('"');
	for (char c : s) {
		switch (c) {
			case '\\':
				out.append("\\\\");
				break;
			case '"':
				out.append("\\\"");
				break;
			case '\n':
				out.append("\\n");
				break;
			case '\r':
				out.append("\\r");
				break;
			case '\t':
				out.append("\\t");
				break;
			default:
				out.push_back(c);
		}
	}
	out.push_back('"');
	return out;
}

scanner::scanner(driver *d) : _driver(d), _buf(new char[BUF_SZ]) {
	_buf[0] = '\0';
}

scanner::~scanner() {
	delete [] _buf;
}

void scanner::reset() {
	_line_number = 0;

	_beg = _mark = _cur = _lim = _eof = nullptr;
	_val.clear();
	_token_store.clear();
	_evbuf = nullptr;
	_state = -1;
	_tok = -1;
}

void scanner::log_begin(size_t sz) {
	if (!TAGL_LOG_ENABLED(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG))
		return;

	TAGL_LOG_SCANNER_DEBUG(
		"scanner begin"
		<< " file=" << scanner_debug_quote(_driver->path().empty() ? "<input>" : _driver->path())
		<< " line=" << _line_number
		<< " bytes=" << sz
	);
}

void scanner::log_token(int tok, TokenText val) {
	if (!TAGL_LOG_ENABLED(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG))
		return;

	TAGL_LOG_SCANNER_DEBUG(
		"scanner token"
		<< " line=" << _line_number
		<< " tok=" << token_str(tok)
		<< " lexeme=" << scanner_debug_quote(val.empty() ? std::string() : val.str())
	);
}

void scanner::log_refill(size_t carried, size_t tail, size_t read_sz, bool eof) {
	if (!TAGL_LOG_ENABLED(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG))
		return;

	TAGL_LOG_SCANNER_DEBUG(
		"scanner refill"
		<< " line=" << _line_number
		<< " carried=" << carried
		<< " tail=" << tail
		<< " read=" << read_sz
		<< " eof=" << (eof ? "true" : "false")
	);
}

void scanner::log_error(const std::string& lexeme) {
	if (!TAGL_LOG_ENABLED(HARD_TAG_ROLE_SCANNER, tagd::log_level::DEBUG))
		return;

	TAGL_LOG_SCANNER_DEBUG(
		"scanner error"
		<< " line=" << _line_number
		<< " lexeme=" << scanner_debug_quote(lexeme)
		<< " reason=\"unrecognized token\""
	);
}

const char* scanner::fill() {
// YYFILL(n)  should adjust YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed.
	if (_cur == nullptr) {
		_eof = _cur;
		TAGL_LOG_TRACE( "fill eof. " << std::endl )
	}

	size_t sz, offset;
	assert(_lim >= _beg);
	sz = _lim - _beg;
	assert(sz <= BUF_SZ);
	size_t carried = 0;
	size_t tail_sz = 0;

	if (sz >= BUF_SZ) {
		// buffer is full, so overflow the consumed span into _val and
		// carry forward only the unread tail for continued scanning
		carried = _cur - _beg;
		tail_sz = _lim - _cur;
		_val.append(_beg, carried);
		if (tail_sz > 0)
			memmove(&_buf[0], _cur, tail_sz);
		_beg = &_buf[0];
		// resume scanning at the start of the unread tail we just carried forward
		_cur = &_buf[0];
		sz = offset = tail_sz;
	} else {
		tail_sz = sz;
		memmove(&_buf[0], _beg, sz);
		_cur = &_buf[_cur-_beg];
		_beg = &_buf[0];
		offset = sz;
	}
	_buf[sz] = '\0';
	_mark = _cur;

	size_t read_sz = BUF_SZ - offset;

	if ((sz = evbuffer_remove(_evbuf, &_buf[offset], read_sz)) != 0) {
		if (sz < read_sz) {
			sz += offset;
			if (_buf[sz] != '\0')
				_buf[sz] = '\0';
			_eof = &_buf[sz];
		} else {
			sz += offset;
		}
		_lim = &_buf[sz];
		log_refill(carried, tail_sz, read_sz, _eof != nullptr);
	} else {
		_eof = _lim = &_buf[offset];
		log_refill(carried, tail_sz, read_sz, true);
	}

	return _cur;
}

void scanner::begin_scan(const char *cur, size_t sz) {
	if (sz >= BUF_SZ) {
		_driver->ferror(tagd::TAGL_ERR, "scan size (%d) >= buffer(%d)", sz, BUF_SZ);
		return;
	}

	_line_number = sz ? 1 : 0; // empty content, zero lines
	_beg = _mark = _cur = cur;
	_lim = &_cur[sz];
	log_begin(sz);
}

void scanner::clear_value() {
	if (!_val.empty())
		_val.clear();
}

void scanner::advance_begin() {
	_beg = _cur;
}

void scanner::next_line(size_t inc) {
	_line_number += inc;
}

size_t scanner::line_number() {
	return _line_number;
}

TokenText scanner::store_token_text() {
	const std::string& s = store_value();
	return TokenText{s.c_str(), (int)s.size()};
}

const std::string& scanner::store_value() {
	return (_val.empty()
		? _token_store.store(_beg, (_cur - _beg))
		: _token_store.store(_val.append(_beg, (_cur - _beg))));
}

TokenText scanner::new_value() {
	return store_token_text();
}

void scanner::emit(int tok, TokenText val) {
	_tok = tok;
	log_token(_tok, val);
	_driver->parse_tok(_tok, val);
	advance_begin();
	clear_value();
}

void scanner::emit_tagd_pos_lookup() {
	const std::string& val = store_value();
	emit(_driver->lookup_pos(val), TokenText{val.c_str(), (int)val.size()});
}

void scanner::emit_literal_value(int tok, const char *cval) {
	emit(tok, _token_store.store_text(cval, strlen(cval)));
}

void scanner::emit_lookup_uri_token() {
	const std::string& val = store_value();
	auto pos = _driver->lookup_pos(val);
	emit((pos == TOK_URL ? TOK_HDURI : pos), TokenText{val.c_str(), (int)val.size()});
}

void scanner::emit_tagl_file_token() {
	emit(TOK_TAGL_FILE, new_value());
}

void scanner::emit_quoted_string_token() {
	size_t sz = (_cur - _beg);
	const std::string& s = store_value();

	int tok;
	switch(_driver->_token) {
		case TOK_CMD_QUERY:
		case TOK_INCLUDE:
			tok = TOK_QUOTED_STR;
			break;
		default:
			tok = _driver->lookup_pos(s.substr(1, sz - 2));
	}

	emit(tok, _token_store.store_text(s.substr(1, sz - 2)));
}

void scanner::emit_error() {
	log_error(std::string(_beg, (_cur - _beg)));
	_driver->error(tagd::TAGL_ERR,
		tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, std::string(_beg, (_cur - _beg))));
	if (!_driver->path().empty()) {
		_driver->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, "_file", _driver->path()) );
	}
	if (_line_number) {
		_driver->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(_line_number)) );
	}
}

int scanner::tagdurl::method_command(tagd::http_method method) {
	switch (method) {
		case tagd::HTTP_GET:
		case tagd::HTTP_HEAD:
			return TOK_CMD_GET;
		case tagd::HTTP_POST:
		case tagd::HTTP_PUT:
			return TOK_CMD_PUT;
		case tagd::HTTP_DELETE:
			return TOK_CMD_DEL;
		default:
			return TOK_UNKNOWN;
	}

	return TOK_UNKNOWN;
}

} // namespace TAGL
