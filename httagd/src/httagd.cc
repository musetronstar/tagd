#include "httagd.h"
#include "tagl.h"
#include "parser.h"  // for CMDs
#include <evhtp.h>

const char* evhtp_res_str(int tok) {
	switch (tok) {
#include "evhtp-results.inc"
		default: return "UNKNOWN_RES_CODE";
	}
}

namespace httagd {

void htscanner::scan_tagdurl_path(int cmd, const request& R) {
	std::string path = R.path();
	// path separator defs
	const size_t max_seps = 2;
	size_t sep_i = 0;
	size_t seps[max_seps] = {0, 0};  // offsets of '/' chars

	std::string q_val = R.query_opt(QUERY_OPT_SEARCH);
	auto f_parse_query_terms = [this, &q_val]() {
		this->_driver->parse_tok(TOK_RELATOR, new std::string(HARD_TAG_HAS));
		this->_driver->parse_tok(TOK_TAG, new std::string(HARD_TAG_TERMS));
		this->_driver->parse_tok(TOK_EQ, NULL);
		this->_driver->parse_tok(TOK_QUOTED_STR, new std::string(q_val.c_str()));
	};

	if ((path == "" || path == "/") && cmd == TOK_CMD_GET) {
		if (q_val.empty()) {	// home page
			dynamic_cast<tagl_callback*>(_driver->callback_ptr())->empty();
		}
		else {	// FTS query
			_driver->parse_tok(TOK_CMD_QUERY, NULL);
			_driver->parse_tok(TOK_QUOTED_STR, new std::string(q_val.c_str()));
		}
		return;
	}

	if (path[0] != '/') {
			_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
			return;
	}

	for(size_t i = 0; i < path.size(); i++) {
		if (path[i] == '/') {
			if (sep_i == max_seps) {
				// TODO use error tag
				_driver->error(tagd::TAGL_ERR, "max_seps exceeded");
				return;
			}
			seps[sep_i++] = i;
		}
	}

	size_t num_seps = sep_i;

	if (cmd == TOK_CMD_PUT && num_seps > 1) {
		_driver->error(tagd::TAGL_ERR, "malformed path: trailing '/'");
		return;
	}

	// path segment
	std::string segment;

	// first segment - what is it?
	sep_i = 0;
	if (seps[sep_i+1]) {
		size_t sz = seps[sep_i+1] - seps[sep_i] - 1;
		segment = path.substr((seps[sep_i]+1), sz);
	}
	else {
		// get
		segment = path.substr(seps[sep_i]+1);
	}

	auto f_parse_cmd_query = [this, &cmd]() {
		cmd = TOK_CMD_QUERY;
		this->_driver->parse_tok(cmd, NULL);
		this->_driver->parse_tok(TOK_INTERROGATOR,
			(new std::string(HARD_TAG_INTERROGATOR)) );  // parser deletes
	};

	if (cmd == TOK_CMD_GET) {
		if (num_seps > 1) {
			f_parse_cmd_query();

			// first segment of "*" is a placeholder for super relation, so ignore it
			if (segment != "*") {  // how is it related
				_driver->parse_tok(TOK_SUPER_RELATOR, (new std::string(HARD_TAG_SUPER)));
				this->scan(tagd::uri_decode(segment).c_str());
			}
		} else {
			if (!q_val.empty()) {  // turn into query (like adding a '/' to the end)
				f_parse_cmd_query();
				f_parse_query_terms();
				return;
			} else {
				_driver->parse_tok(cmd, NULL);
				this->scan(tagd::uri_decode(segment).c_str());
			}
		}
	} else { // CMD_PUT
		if (!q_val.empty()) {
			_driver->error(tagd::TAGD_ERR, "illegal use of query terms with PUT method");
			return;
		}
		_driver->parse_tok(cmd, NULL);
		this->scan(tagd::uri_decode(segment).c_str());
	}

	if (++sep_i >= num_seps)
		return;	

	// second segment - how is it related?
	segment = path.substr(seps[sep_i]+1);

	if (!segment.empty()) {
		_driver->parse_tok(TOK_WILDCARD, NULL);
		this->scan(tagd::uri_decode(segment).c_str());
	}

	if (cmd == TOK_CMD_QUERY && !q_val.empty())
		f_parse_query_terms();
}

tagd_code httagl::tagdurl_get(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_GET, R);

	return this->code();
}

tagd_code httagl::tagdurl_put(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_PUT, R);

	return this->code();
}

tagd_code httagl::tagdurl_del(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_DEL, R);

	return this->code();
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
		_res->add(ss.str());
}

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	_TS->put(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		_res->add(ss.str());
	}
}

void tagl_callback::cmd_del(const tagd::abstract_tag& t) {
	_TS->del(t, _driver->flags());
	std::stringstream ss;
	if (_TS->has_error()) {
		_TS->print_errors(ss);
		_res->add(ss.str());
	}
}

void tagl_callback::cmd_query(const tagd::interrogator& q) {
	std::cerr << "tagl_callback::cmd_query:\n";
	std::cerr << q << std::endl;
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q, _driver->flags());
	std::cerr << tagd_code_str(ts_rc) << ", size: " << T.size() << std::endl;

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_TS->print_errors(ss);
	}
	if (ss.str().size())
		_res->add(ss.str());
}

void tagl_callback::cmd_error() {
	std::stringstream ss;
	_driver->print_errors(ss);
	_res->add(ss.str());
}

void tagl_callback::empty() {
	_res->add("This is tagd");
	_res->send_reply(EVHTP_RES_OK);
}

void tagl_callback::finish() {

	if (_res->reply_sent()) return;

	// determine the most severe error and send a corresponding response code,
	// or OK if no error 
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

	if (_req->svr()->args()->opt_trace)
		std::cerr << "res(" << tagd_code_str(most_severe) << "): " << res << " " << evhtp_res_str(res) << std::endl;

	_res->send_reply(res);
}

void main_cb(evhtp_request_t *ev_req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;
	// for now, this request uses the servers tagspace reference
	// TODO allow requests to use other tagspaces (given the request)  
	auto *TS = svr->tagspace();
	bool trace_on = svr->args()->opt_trace;
	
	httagd::request req(svr, ev_req);
	httagd::response res(svr, ev_req);

	std::string opt_view {req.query_opt(QUERY_OPT_VIEW)};
	std::string opt_context {req.query_opt(QUERY_OPT_CONTEXT)};

	// TODO context is part of the request, not global
	// we may need some kind of session id or something
	// to push_context()
	if (!opt_context.empty())
		TS->push_context(opt_context);

	htp_method method = evhtp_request_get_method(ev_req);
	std::string full_path(ev_req->uri->path->full);

	httagl tagl(TS);
	httagd::tagl_callback *CB;
	// determine TAGL::callback pointer based on media type

	if (opt_view.empty())
		opt_view = svr->args()->default_view;

	if (opt_view.empty() || opt_view == "tagl") {
		CB = new httagd::tagl_callback(TS, &req, &res, opt_context);
	} else {
		CB = new httagd::html_callback(TS, &req, &res, opt_context);
	}
	tagl.callback_ptr(CB);

	// route request
	switch(method) {
		case htp_method_GET:
			tagl.tagdurl_get(req);
			tagl.finish();
			break;
		case htp_method_PUT:
			tagl.tagdurl_put(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
			tagl.finish();
			break;
		case htp_method_POST:
			// TODO check path
			tagl.evbuffer_execute(ev_req->buffer_in);
			tagl.finish();
			break;
		case htp_method_DELETE:
			tagl.tagdurl_del(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
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
			res.add(ss.str());
			// TODO 10.4.6 405 Method Not Allowed
			// The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.
			// The response MUST include an Allow header containing a list of valid methods for the requested resource. 
			res.send_reply(EVHTP_RES_METHNALLOWED);
	}

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

	if (!opt_context.empty())
		TS->pop_context();

	delete CB;
}

tagd::code server::start() {
	if (_args->opt_trace) {
		_TS->trace_on();
		TAGL::driver::trace_on((char *)"trace: ");
	}

    evhtp_set_gencb(_htp, httagd::main_cb, this);

	const char *bind_addr = ( _bind_addr == "localhost" ? "0.0.0.0" : _bind_addr.c_str() );

    if (evhtp_bind_socket(_htp, bind_addr, _bind_port, 128) < 0) {
		return this->ferror( tagd::TAGD_ERR,
				"failed to bind socket(%s): %s:%d", strerror(errno), bind_addr, _bind_port );
	}

    event_base_loop(_evbase, 0);

	return tagd::TAGD_OK;
}

} // namespace httagd

