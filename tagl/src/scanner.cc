#include <cassert>
#include <cstring>
#include <iostream>

#include "tagl.h"
#include <event2/buffer.h>

namespace TAGL {

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
	_evbuf = nullptr;
	_state = -1;
	_tok = -1;
}

void scanner::print_buf() {
	LOG_DEBUG( "print_buf:" << std::endl )
	size_t sz = _lim - _buf;
	for(size_t i=0; i<=sz; ++i) {
		LOG_DEBUG( _buf[i] )
	}
	LOG_DEBUG( std::endl )
	LOG_DEBUG( "print_buf _beg(" << (_beg-_buf) << "): " << ((int)*_beg) << ", " << *_beg << std::endl )
	LOG_DEBUG( "print_buf _cur(" << (_cur-_buf) << "): " << ((int)*_cur) << ", " << *_cur << std::endl )
	LOG_DEBUG( "print_buf _lim(" << (_lim-_buf) << "): " << ((int)*_lim) << ", " << *_lim << std::endl )
	if (_eof == nullptr)
		LOG_DEBUG( "print_buf _eof: NULL" << std::endl )
	else
		LOG_DEBUG( "print_buf _eof(" << (_eof-_buf) << "): " << ((int)*_eof) << ", " << *_eof << std::endl )
	LOG_DEBUG( "print_buf _val(" << _val.size() << "): `" << _val << "'" << std::endl )
}

const char* scanner::fill() {
	if (TAGL_TRACE_ON) {
		LOG_DEBUG( "fill ln: " << _line_number << std::endl )
		LOG_DEBUG( "fill _cur(" << (_cur-_buf) << "): " << ((int)*_cur) << ", " << *_cur << std::endl )
		print_buf();
	}

// YYFILL(n)  should adjust YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed.
	if (_cur == nullptr) {
		_eof = _cur;
		TAGL_LOG_TRACE( "fill eof. " << std::endl )
	}

	size_t sz, offset;
	assert(_lim >= _beg);
	sz = _lim - _beg;
	assert(sz <= BUF_SZ);

	if (sz >= BUF_SZ) {
		// buffer is full, so overflow into a std::string _val (TODO perhaps an evbuffer)
		// _cur is at the end of the buffer, so append everything up to _cur into _val
		// and copy _cur to the beginning of the buffer for scanning
		size_t apnd_sz = BUF_SZ - 1;
		_val.append(_buf, apnd_sz);
		if (TAGL_TRACE_ON) {
			LOG_DEBUG( "fill _val.append(" << apnd_sz << "): `" << std::string(_buf, apnd_sz) << "'" << std::endl )
			LOG_DEBUG( "fill    new _val(" << _val.size() << "): `" << _val << "'" << std::endl )
		}
		_buf[0] = *_cur;
		_beg = _cur = &_buf[0];
		sz = offset = 1;
	} else {
		memmove(&_buf[0], _beg, sz);
		_cur = &_buf[_cur-_beg];
		_beg = &_buf[0];
		offset = sz;
	}
	_buf[sz] = '\0';
	_mark = _cur;

	if (TAGL_TRACE_ON) {
		if (!_val.empty())
			LOG_DEBUG( "fill val: `" << _val << "'" << std::endl )
		LOG_DEBUG( "fill buf(" << sz << "): `" << std::string(_buf, sz) << "'" << std::endl )
	}

	size_t read_sz = BUF_SZ - offset;
	if (TAGL_TRACE_ON) {
		LOG_DEBUG( "fill sz: " << sz << std::endl )
		LOG_DEBUG( "fill offset: " << offset << std::endl )
		LOG_DEBUG( "fill read_sz: " << read_sz << std::endl )
	}

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
		if (TAGL_TRACE_ON)
			LOG_DEBUG( "filled(" << sz << "): `" << std::string(_beg, sz) << "'" << std::endl )
	} else {
		_eof = _lim = &_buf[offset];
	}

	if (TAGL_TRACE_ON)
		print_buf();

	return _cur;
}

void scanner::begin_scan(const char *cur, size_t sz) {
	if (TAGL_TRACE_ON)
		LOG_DEBUG( "scan(" << &cur << "): " << std::string(cur, sz) << std::endl )

	if (sz >= BUF_SZ) {
		_driver->ferror(tagd::TAGL_ERR, "scan size (%d) >= buffer(%d)", sz, BUF_SZ);
		return;
	}

	_line_number = sz ? 1 : 0; // empty content, zero lines
	_beg = _mark = _cur = cur;
	_lim = &_cur[sz];
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

std::string* scanner::new_value() {
	return (_val.empty()
		? new std::string(_beg, (_cur - _beg))
		: new std::string(_val.append(_beg, (_cur - _beg))));
}

void scanner::emit(int tok, std::string *val) {
	_tok = tok;
	_driver->parse_tok(_tok, val);
	advance_begin();
	clear_value();
}

void scanner::emit_tagd_pos_lookup() {
	std::string *val = new_value();
	emit(_driver->lookup_pos(*val), val);
}

void scanner::emit_lookup_uri_token() {
	std::string *val = new_value();
	auto pos = _driver->lookup_pos(*val);
	emit((pos == TOK_URL ? TOK_HDURI : pos), val);
}

void scanner::emit_tagl_file_token() {
	emit(TOK_TAGL_FILE, new_value());
}

void scanner::emit_quoted_string_token() {
	size_t sz = (_cur - _beg);
	_val.append(_beg, sz);

	int tok;
	switch(_driver->_token) {
		case TOK_CMD_QUERY:
		case TOK_INCLUDE:
			tok = TOK_QUOTED_STR;
			break;
		default:
			tok = _driver->lookup_pos(_val.substr(1, sz - 2));
	}

	emit(tok, new std::string(_val.substr(1, sz - 2)));
}

void scanner::emit_error() {
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

} // namespace TAGL
