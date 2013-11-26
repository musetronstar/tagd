#include "httagd.h"

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

	switch (ts_rc) {
		case tagd::TAGD_OK:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " <<  EVHTP_RES_OK << " EVHTP_RES_OK" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_NOTFOUND << " EVHTP_RES_NOTFOUND" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
}

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->put(t);
	std::stringstream ss;
	if (ts_rc != tagd::TAGD_OK) {
		_TS->print_errors(ss);
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	}

	switch (ts_rc) {
		case tagd::TAGD_OK:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " <<  EVHTP_RES_OK << " EVHTP_RES_OK" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_NOTFOUND << " EVHTP_RES_NOTFOUND" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		case tagd::TS_DUPLICATE:  // using 409 - Conflict, 422 - Unprocessable Entity might also be exceptable
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_CONFLICT << " EVHTP_RES_CONFLICT" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_CONFLICT);
			break;
		default:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
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
		_TS->print_errors(ss);
	}
	if (ss.str().size())
		evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());

	switch (ts_rc) {
		case tagd::TAGD_OK:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " <<  EVHTP_RES_OK << " EVHTP_RES_OK" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_OK);
			break;
		case tagd::TS_NOT_FOUND:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_NOTFOUND << " EVHTP_RES_NOTFOUND" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_NOTFOUND);
			break;
		default:
			if (_trace_on)
				std::cerr << "res(" << tagd_code_str(ts_rc) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
			evhtp_send_reply(_req, EVHTP_RES_SERVERR);
	}
}

void tagl_callback::error(const TAGL::driver& D) {
	std::stringstream ss;
	D.print_errors(ss);
	evbuffer_add(_req->buffer_out, ss.str().c_str(), ss.str().size());
	if (_trace_on)
		std::cerr << "res(" << tagd_code_str(D.code()) << "): " << EVHTP_RES_SERVERR << " EVHTP_RES_SERVERR" << std::endl << ss.str() << std::endl;
	evhtp_send_reply(_req, EVHTP_RES_SERVERR);
}

} // namespace httagd
