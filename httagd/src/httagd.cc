#include "httagd.h"

namespace httagd {

void tagl_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc = _TS->get(T, t.id());

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		ss << T << std::endl;
	} else {
		_driver->print_errors(ss);
	}
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());

	switch (ts_rc) {
		case tagd::TAGD_OK:
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
}

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->put(t);
	if (ts_rc != tagd::TAGD_OK) {
		std::stringstream ss;
		_driver->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}

	switch (ts_rc) {
		case tagd::TAGD_OK:
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
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
		_driver->print_errors(ss);
	}
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());

	switch (ts_rc) {
		case tagd::TAGD_OK:
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
}

void tagl_callback::error(const TAGL::driver& D) {
	std::stringstream ss;
	_driver->print_errors(ss);
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	evhtp_send_reply(_req, EVHTP_RES_SERVERR);
}

} // namespace httagd
