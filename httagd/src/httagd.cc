#include "httagd.h"
#include <evhtp.h>

const char* evhtp_res_str(int tok) {
	switch (tok) {
#include "evhtp-results.inc"
		default: return "UNKNOWN_RES_CODE";
	}
}

namespace httagd {

void tagl_callback::add_http_headers() {
	evhtp_headers_add_header(_request->_ev_req->headers_out,
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
		evbuffer_add(_request->_ev_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	_TS->put(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_request->_ev_req->buffer_out, ss.str().c_str(), ss.str().size());
	}
}

void tagl_callback::cmd_del(const tagd::abstract_tag& t) {
	_TS->del(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		evbuffer_add(_request->_ev_req->buffer_out, ss.str().c_str(), ss.str().size());
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
		evbuffer_add(_request->_ev_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::cmd_error() {
	std::stringstream ss;
	_driver->print_errors(ss);
	evbuffer_add(_request->_ev_req->buffer_out, ss.str().c_str(), ss.str().size());
}

void tagl_callback::empty() {
	std::string msg( "This is tagd" );
	evbuffer_add(_request->_ev_req->buffer_out, msg.c_str(), msg.size());
	evhtp_send_reply(_request->_ev_req, EVHTP_RES_OK);
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
	evhtp_send_reply(_request->_ev_req, res);
}

void main_cb(evhtp_request_t *req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;
	// for now, this request uses the servers tagspace reference
	// TODO allow requests to use other tagspaces (given the request)  
	auto *TS = svr->_TS;

	bool trace_on = svr->_args->opt_trace;

	// check for template id in query string
	
	httagd::request R(svr, req);
	std::string t_val = R.query_opt("t"); // t = template
	std::string c_val = R.query_opt("c"); // c = context
	
	if (!c_val.empty())
		TS->push_context(c_val);

	htp_method method = evhtp_request_get_method(req);
	std::string full_path(req->uri->path->full);
	bool is_home_page = (method == htp_method_GET)
		&& (full_path.empty() || full_path == "/");

	TAGL::driver tagl(TS);
	httagd::tagl_callback *CB;
	if (!is_home_page && t_val.empty()) {
		CB = new httagd::tagl_callback(TS, &R);
	} else {
		CB = new httagd::template_callback(TS, &R);
	}
	tagl.callback_ptr(CB);

	switch(method) {
		case htp_method_GET:
			if (is_home_page) {
				CB->empty();
			} else {
				tagl.tagdurl_get(full_path);
				tagl.finish();
			}
			break;
		case htp_method_PUT:
			tagl.tagdurl_put(full_path);
			tagl.evbuffer_execute(req->buffer_in);
			tagl.finish();
			break;
		case htp_method_POST:
			// TODO check path
			tagl.evbuffer_execute(req->buffer_in);
			tagl.finish();
			break;
		case htp_method_DELETE:
			tagl.tagdurl_del(full_path);
			tagl.evbuffer_execute(req->buffer_in);
			tagl.finish();
			break;
/*
		htp_method_HEAD,
		htp_method_MKCOL,
		htp_method_COPY,
		htp_method_MOVE,
		htp_method_OPTIONS,
		htp_method_PROPFIND,
		htp_method_PROPPATCH,
		htp_method_LOCK,
		htp_method_UNLOCK,
		htp_method_TRACE,
		htp_method_CONNECT, // RFC 2616 
		htp_method_PATCH,   // RFC 5789 
		htp_method_UNKNOWN,	
  */

		default:
			// TODO use tagd::error
			tagl.ferror(tagd::HTTP_ERR, "unsupported method: %d", method);
			std::stringstream ss;
			tagl.print_errors(ss);
			evbuffer_add(req->buffer_out, ss.str().c_str(), ss.str().size());
			// TODO 10.4.6 405 Method Not Allowed
			// The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.
			// The response MUST include an Allow header containing a list of valid methods for the requested resource. 
			evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
	}

	if (!c_val.empty())
		TS->pop_context();

	if (TS->has_error()) {
		if (trace_on)
			TS->print_errors();
		TS->clear_errors();
	}

	if (tagl.has_error()) {
		if (trace_on)
			tagl.print_errors();
		tagl.clear_errors();
	}

	delete CB;
}

tagd::code server::start() {
	if (_args->opt_trace) {
		_TS->trace_on();
		TAGL::driver::trace_on((char *)"trace: ");
	}

    evhtp_set_gencb(_htp, httagd::main_cb, this);

    if (evhtp_bind_socket(_htp, _bind_addr.c_str(), _bind_port, 128) < 0) {
		return this->ferror( tagd::TAGD_ERR,
				"failed to bind socket(%s): %s:%d", strerror(errno), _bind_addr.c_str(), _bind_port );
	}

    event_base_loop(_evbase, 0);

	return tagd::TAGD_OK;
}

} // namespace httagd

