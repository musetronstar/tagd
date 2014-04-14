#include "httagd.h"

const char* evhtp_res_str(int tok) {
	switch (tok) {
#include "evhtp-results.inc"
		default: return "UNKNOWN_RES_CODE";
	}
}

namespace httagd {

void tagl_callback::add_http_headers() {
	evhtp_headers_add_header(_req->headers_out,
	evhtp_header_new("Content-Type", _content_type.c_str(), 0, 0));
}

void tagl_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc = _TS->get(T, t.id(), _driver->flags());

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		ss << T << std::endl;
	} else {
		_TS->print_errors(ss);
	}
	if (ss.str().size())
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	_TS->put(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}
}

void tagl_callback::cmd_del(const tagd::abstract_tag& t) {
	_TS->del(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}
}

void tagl_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q, _driver->flags());

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_TS->print_errors(ss);
	}
	if (ss.str().size())
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::cmd_error() {
	std::stringstream ss;
	_driver->print_errors(ss);
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::empty() {
	std::string msg( "This is tagd" );
	evbuffer_add(_req->buffer_out, msg.c_str(), msg.size());
	evhtp_send_reply(_req, EVHTP_RES_OK);
}

void tagl_callback::finish() {
	tagd::code most_severe = _driver->code();

	for (auto e : _driver->errors() ) {
		if (e.code() > most_severe)
			most_severe = e.code();
	}

	for (auto e : _TS->errors() ) {
		if (e.code() > most_severe)
			most_severe = e.code();
	}

	int res;
	switch (most_severe) {
		case tagd::TAGD_OK:
			res = EVHTP_RES_OK;
			break;
		case tagd::TS_NOT_FOUND:
			res = EVHTP_RES_NOTFOUND;
			break;
		case tagd::TS_DUPLICATE:
			// using 409 - Conflict, 422 - Unprocessable Entity might also be exceptable
			res = EVHTP_RES_CONFLICT;
			break;
		default:
			res = EVHTP_RES_SERVERR;
	}

	if (_trace_on)
		std::cerr << "res(" << tagd_code_str(most_severe) << "): " << res << " " << evhtp_res_str(res) << std::endl;

	this->add_http_headers();
	evhtp_send_reply(_req, res);
}

} // namespace httagd

