#include "httagd.h"

const char* evhtp_res_str(int tok) {
	switch (tok) {
#include "evhtp-results.inc"
		default: return "UNKNOWN_RES_CODE";
	}
}

namespace httagd {

void tagl_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc = _TS->get(T, t.id());

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
	tagd::code ts_rc = _TS->put(t);
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}
}

void tagl_callback::cmd_del(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->del(t);
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}
}

void tagl_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

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

void tagl_callback::cmd_error(const TAGL::driver& D) {
	std::stringstream ss;
	D.print_errors(ss);
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::finish(const TAGL::driver& D) {
	tagd::code most_severe = D.code();

	for (auto e : D.errors() ) {
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

	evhtp_send_reply(_req, res);
}

} // namespace httagd

