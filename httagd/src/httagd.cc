#include "httagd.h"
#include "tagl.h"
#include "parser.h"  // for CMDs
#include <evhtp.h>

const char* evhtp_res_str(int tok) {
	switch (tok) {
#include "evhtp-results.inc"
	}

	return "UNKNOWN_RES_CODE";
}

const char* evhtp_method_str(int method) {
	switch(method) {
		case htp_method_HEAD:      return "HEAD";
		case htp_method_GET:       return "GET";
		case htp_method_PUT:       return "PUT";
		case htp_method_POST:      return "POST";
		case htp_method_DELETE:    return "DELETE";
		case htp_method_MKCOL:     return "MKCOL";
		case htp_method_COPY:      return "COPY";
		case htp_method_MOVE:      return "MOVE";
		case htp_method_OPTIONS:   return "OPTIONS";
		case htp_method_PROPFIND:  return "PROPFIND";
		case htp_method_PROPPATCH: return "PROPPATCH";
		case htp_method_LOCK:      return "LOCK";
		case htp_method_UNLOCK:    return "UNLOCK";
		case htp_method_TRACE:     return "TRACE";
		case htp_method_CONNECT:   return "CONNECT";
		case htp_method_PATCH:     return "PATCH";
		case htp_method_UNKNOWN:   return "UNKNOWN";
	}

	return "UNKNOWN_METHOD";
}

namespace httagd {

void htscanner::scan_tagdurl_path(int cmd, const request& R) {
	std::string path = R.path();
	// path separator defs
	const size_t max_seps = 2;
	size_t seps[max_seps] = {0, 0};  // offsets of '/' chars

	std::string opt_search = R.query_opt_search();
	auto f_parse_search_terms = [this, &opt_search]() {
		this->_driver->parse_tok(TOK_RELATOR, new std::string(HARD_TAG_HAS));
		this->_driver->parse_tok(TOK_TAG, new std::string(HARD_TAG_TERMS));
		this->_driver->parse_tok(TOK_EQ, NULL);
		this->_driver->parse_tok(TOK_QUOTED_STR, new std::string(opt_search.c_str()));
	};

	if ((path == "" || path == "/") && cmd == TOK_CMD_GET) {
		if (opt_search.empty()) {
			// home page or welcome message
			dynamic_cast<httagd::callback*>(_driver->callback_ptr())->empty();
		}
		else {	// FTS query
			_driver->parse_tok(TOK_CMD_QUERY, NULL);
			_driver->parse_tok(TOK_QUOTED_STR, new std::string(opt_search.c_str()));
		}
		return;
	}

	if (path[0] != '/') {
			_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
			return;
	}

	size_t num_seps = 0;
	for(size_t i = 0; i < path.size(); i++) {
		if (path[i] == '/') {
			if (num_seps == max_seps) {
				// TODO use error tag
				_driver->error(tagd::TAGL_ERR, "max_seps exceeded");
				return;
			}
			seps[num_seps++] = i;
		}
	}

	if (cmd == TOK_CMD_PUT && num_seps > 1) {
		_driver->error(tagd::TAGL_ERR, "malformed path: trailing '/'");
		return;
	}

	// path segment
	std::string segment;

	// first segment tag id - what is it?
	size_t sep_i = 0;
	if (seps[sep_i+1]) { // extract id between the separators: /id/
		size_t sz = seps[sep_i+1] - seps[sep_i] - 1;
		segment = path.substr((seps[sep_i]+1), sz);
	}
	else {
		// GET, PUT, or DEL
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
			if (!opt_search.empty()) {  // turn into query (like adding a '/' to the end)
				f_parse_cmd_query();
				f_parse_search_terms();
				return;
			} else {
				_driver->parse_tok(cmd, NULL);
				this->scan(tagd::uri_decode(segment).c_str());
			}
		}
	} else {
		if(cmd != TOK_CMD_PUT && cmd != TOK_CMD_DEL) {
			_driver->error(tagd::TAGD_ERR, "illegal command");
			return;
		}

		if (!opt_search.empty()) {
			_driver->error(tagd::TAGD_ERR, "illegal use of search terms with method");
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

	if (cmd == TOK_CMD_QUERY && !opt_search.empty())
		f_parse_search_terms();
}

tagd::code httagl::tagdurl_get(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_GET, R);

	return this->code();
}

tagd::code httagl::tagdurl_put(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_PUT, R);

	return this->code();
}

tagd::code httagl::tagdurl_del(const request& R) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_DEL, R);

	return this->code();
}

void callback::default_cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc = _tx->TS->get(T, t.id(), _driver->flags());

	// TODO, if outputting to an iostream is still desirable,
	// but more efficient to ouput to an evbuffer directly,
	// want to use std::ios_base::register_callback
	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		ss << T << std::endl;
	} else {
		_tx->TS->print_errors(ss);
	}

	if (_tx->req->method == HTTP_HEAD) {
		/* even though evhtp will not send content added for HEAD requests,
		 * we will short circuit that by not adding content
		 * _evhtp_create_reply() adds Content-Length header if not exists so lets create one
		 */
		_tx->res->add_header_content_length(ss.str().size());
	} else if (ss.str().size()) {
		_tx->res->add(ss.str());
	}
}

void callback::default_cmd_put(const tagd::abstract_tag& t) {
	_tx->TS->put(t, _driver->flags());
	std::stringstream ss;
	if (_tx->TS->has_errors()) {
		_tx->TS->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void callback::default_cmd_del(const tagd::abstract_tag& t) {
	_tx->TS->del(t, _driver->flags());
	std::stringstream ss;
	if (_tx->TS->has_errors()) {
		_tx->TS->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void callback::default_cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _tx->TS->query(T, q, _driver->flags());

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_tx->TS->print_errors(ss);
	}
	if (ss.str().size())
		_tx->res->add(ss.str());
}

void callback::default_cmd_error() {
	std::stringstream ss;
	_driver->print_errors(ss);
	_tx->res->add(ss.str());
}

void callback::default_empty() {
	_tx->res->add("This is tagd");
}

void callback::finish() {
	if (_tx->res->reply_sent()) {
		std::cerr << "reply already sent" << std::endl;
		return;
	}

	_tx->res->send_reply(
		_tx->most_severe(tagd::TAGD_OK)
	);
}

void response::send_reply(evhtp_res res) {
	if (_reply_sent) {
		std::cerr << "reply already sent" << std::endl;
		return;
	}
	this->add_header("Content-Type", content_type);
	if (_tx->svr->args()->opt_trace)
		std::cerr << "send_reply(" << res << "): " << evhtp_res_str(res) << std::endl;
	evhtp_send_reply(_ev_req, res);
	_res_code = res;
	_reply_sent = true;
}

void response::add_header(const std::string &k, const std::string &v) {
	if (_tx->svr->args()->opt_trace)
		std::cerr << "add_header(" << '"' << k << '"' << ", " << '"' << v << '"' << ")" << std::endl;

/* from evhtp.h
* evhtp_header_new
* @param key null terminated string
* @param val null terminated string
* @param kalloc if set to 1, the key will be copied, if 0 no copy is done.
* @param valloc if set to 1, the val will be copied, if 0 no copy is done.
*/
	evhtp_headers_add_header(
		_ev_req->headers_out,
		// params 3,4 must be set, else strings from previous calls can get intermingled
		evhtp_header_new(k.c_str(), v.c_str(), 1, 1)
	);
}


std::string request::url() const {
	std::string url;

	// if we have an authority, we must supply a protocol
	// otherwise, use just a path
	if ( _ev_req->uri->authority ) {
		switch ( _ev_req->uri->scheme ) {
			case htp_scheme_ftp:
				url.append("ftp://");
				break;
			case htp_scheme_https:
				url.append("https://");
				break;
			case htp_scheme_nfs:
				url.append("nfs://");
				break;
			case htp_scheme_http:
			case htp_scheme_none:
			case htp_scheme_unknown:
			default:
				url.append("http://");
		}

		if ( _ev_req->uri->authority->username )
			url.append( _ev_req->uri->authority->username );

		if ( _ev_req->uri->authority->password )
			url.append(":").append( _ev_req->uri->authority->password );

		if ( _ev_req->uri->authority->username )
			url.append("@");

		if ( _ev_req->uri->authority->hostname )
			url.append( _ev_req->uri->authority->hostname );

		if ( _ev_req->uri->authority->port && _ev_req->uri->authority->port != 80 )
			url.append( std::to_string(_ev_req->uri->authority->port) );
	}

	if ( _ev_req->uri->path->full )
		url.append( _ev_req->uri->path->full );

	if ( _ev_req->uri->query_raw )
		url.append("?").append( reinterpret_cast<const char *>(_ev_req->uri->query_raw) );

	if ( _ev_req->uri->fragment )
		url.append("#").append( reinterpret_cast<const char *>(_ev_req->uri->fragment) );

	return url;
}

std::string request::effective_opt_view() const {
	std::string view_opt = this->query_opt(QUERY_OPT_VIEW);
	if (view_opt.empty()) {
		if (!_tx->svr->args()->default_view.empty())
			return _tx->svr->args()->default_view;  // user supplied default
		else
			return DEFAULT_VIEW;  // hard-coded default
	}
	return view_opt;
}

void callback::output_errors(tagd::code ret_tc) {
	if (!_tx->size())
		_tx->ferror(tagd::TS_INTERNAL_ERR, "no errors to output, returned: %s", tagd_code_str(ret_tc));

	if (_tx->trace_on) _tx->print_errors();

	// get the error_function view
	view vw;
	_tx->VS->report_errors = false;
	tagd::code tc = _tx->VS->get(vw, error_view_id(_tx->req->effective_opt_view()));
	_tx->VS->report_errors = true;

	// call error_function_t for the errorable object
	if (tc == tagd::TAGD_OK)
		tc = vw.error_function(*_tx, vw, *_tx);
	else
		tc = _tx->VS->fallback_error_view.error_function(*_tx, _tx->VS->fallback_error_view, *_tx);

	// add plain text errors if the call failed
	if (tc != tagd::TAGD_OK)
		_tx->add_errors();
}

void callback::cmd_get(const tagd::abstract_tag& t) {
	if (_tx->trace_on) std::cerr << "cmd_get()" << std::endl;

	std::string view_name = _tx->req->effective_opt_view();
	if (view_name == DEFAULT_VIEW)
		return this->default_cmd_get(t);

	/*
	 * In the case of HEAD requests, we are still adding
	 * content to the buffer just like GET requests.
	 * This is because evhtp will count the bytes and add
	 * a Content-Length header for us, even though the content
	 * will not be sent to the client.
	 * TODO surely, there must be a less wasteful way to do this.
	 */

	tagd::abstract_tag T;
	tagd::code tc;
	if (t.pos() == tagd::POS_URL) {
		tc = _tx->TS->get(static_cast<tagd::url&>(T), t.id());
	} else {
		tc = _tx->TS->get(T, t.id());
	}

	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, get_view_id(view_name));
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}

	tc = vw.get_function(*_tx, vw, T);
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}
}

void callback::cmd_put(const tagd::abstract_tag& t) {
	return this->default_cmd_put(t);

	// TODO get handler and call put_function
}

void callback::cmd_del(const tagd::abstract_tag& t) {
	return this->default_cmd_del(t);

	// TODO get handler and call del_function
}


void callback::cmd_query(const tagd::interrogator& q) {
	if (_tx->trace_on) std::cerr << "cmd_query()" << std::endl;

	std::string view_name = _tx->req->effective_opt_view();
	if (view_name == DEFAULT_VIEW)
		return this->default_cmd_query(q);

	tagd::tag_set R;
	tagd::code tc = _tx->TS->query(R, q);

	if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND) {
		output_errors(tc);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, query_view_id(view_name));
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}

	tc = vw.query_function(*_tx, vw, q, R);
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}
}

void callback::cmd_error() {
	if (_tx->trace_on) std::cerr << "cmd_error()" << std::endl;

	if (_tx->req->effective_opt_view() == DEFAULT_VIEW)
		return this->default_cmd_error();

	output_errors(_tx->code());
}


void callback::empty() {
	if (_tx->trace_on) std::cerr << "empty()" <<  std::endl;

	std::string view_name = _tx->req->effective_opt_view();
	if (view_name == DEFAULT_VIEW)
		return this->default_empty();

	view vw;
	tagd::code tc = _tx->VS->get(vw, empty_view_id(view_name));
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}

	tc = vw.empty_function(*_tx, vw);
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return;
	}
}

tagd::code tagd_template::load(tagd::errorable& E, const std::string& fname) {
	if (fname.empty()) {
		E.ferror( tagd::TAGD_ERR, "template file required" );
		return tagd::TAGD_ERR;
	}

	if (!ctemplate::LoadTemplate(fname, ctemplate::DO_NOT_STRIP)) {
		E.ferror( tagd::TAGD_ERR, "load template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	return tagd::TAGD_OK;
}

tagd::code tagd_template::expand(const std::string& fname) {
	if (tagd_template::load(*this, fname) != tagd::TAGD_OK)
		return this->code();

	// this also would load (if not already loaded), but lets load and expand in seperate
	// steps so we can better trace the source of errors
	if (!ctemplate::ExpandTemplate(fname, ctemplate::DO_NOT_STRIP, _dict, _output)) {
		this->ferror( tagd::TAGD_ERR, "expand template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	return tagd::TAGD_OK;
}

tagd::code tagd_template::expand(viewspace& VS, const std::string& fname) {
	if (this->expand(VS.fpath(fname)) != tagd::TAGD_OK)
		return VS.copy_errors(*this).code();

	return tagd::TAGD_OK;
}

tagd_template* tagd_template::include(const std::string &id, const std::string &fname) {
	auto d = _dict->AddIncludeDictionary(id);
	d->SetFilename(fname);
	return this->new_sub_template(d);
}

// sets the value of a template marker to <key> and also its
// corresponding <key>_lnk marker to the hyperlink representation of <key>
void tagd_template::set_tag_link(const url_query_map_t& query_map, const std::string& key, const std::string& val) {
	if (val == "*") { // wildcard relator
		// empty link
		this->set_value(key, val);
		return;
	}

	std::string opt_str;

	std::string view_name;
	tagd::url::query_find(query_map, view_name, QUERY_OPT_VIEW);
	if (!view_name.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(QUERY_OPT_VIEW);
		opt_str.push_back(('='));
		opt_str.append(tagd::uri_encode(view_name));
	}

	std::string context;
	tagd::url::query_find(query_map, context, QUERY_OPT_CONTEXT);
	if (!context.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(QUERY_OPT_CONTEXT);
		opt_str.push_back(('='));
		opt_str.append(tagd::uri_encode(context));
	}

	// sets template key and key_lnk with their corresponding values
	auto f_set_vals = [this, &key, &opt_str](const std::string& key_val, const std::string& lnk_val) {
		this->set_value(
			std::string(key).append("_lnk"),
			std::string("/").append(lnk_val).append(opt_str)
		);
		this->set_value(key, key_val);
	};

	if ( tagd::url::looks_like_hduri(val) ) {
		tagd::url u;
		u.init_hduri(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else if ( tagd::url::looks_like_url(val) ) {
		tagd::url u;
		u.init(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else {
		f_set_vals(val, tagd::util::esc_and_quote(val));
	}
}

void main_cb(evhtp_request_t *ev_req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;

	// for now, this request uses the servers tagspace reference
	// TODO allow requests to use other tagspaces (given the request)
	auto *TS = svr->TS();
	auto *VS = svr->VS();

	// circular dependencies between transaction and request/respsonse pointers
	transaction tx(svr, TS, VS, nullptr, nullptr);
	request req(&tx, ev_req);
	response res(&tx, ev_req);
	tx.req = &req;
	tx.res = &res;

	// TODO context is part of the request, not global
	// we may need some kind of session id or something to push_context()
	if (!req.query_opt_context().empty())
		TS->push_context(req.query_opt_context());

	callback CB(&tx);
	httagl tagl(TS, &CB);

	// all sharing the internal errors pointer of tx
	tx.share_errors(tagl)
	  .share_errors(*TS)
	  .share_errors(*VS);

	htp_method ev_method = evhtp_request_get_method(ev_req);

	// route request
	switch(ev_method) {
		case htp_method_HEAD:
			req.method = HTTP_HEAD;
			// identical to GET request, but content body not added
			tagl.tagdurl_get(req);
			break;
		case htp_method_GET:
			req.method = HTTP_GET;
			tagl.tagdurl_get(req);
			break;
		case htp_method_PUT:
			req.method = HTTP_PUT;
			tagl.tagdurl_put(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
			break;
		case htp_method_POST:
			req.method = HTTP_POST;
			// TODO check path
			tagl.evbuffer_execute(ev_req->buffer_in);
			break;
		case htp_method_DELETE:
			req.method = HTTP_DELETE;
			tagl.tagdurl_del(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
			break;
/*
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
			tx.ferror(tagd::HTTP_ERR, "unsupported method: %s", evhtp_method_str(ev_method));
			res.res_code(EVHTP_RES_METHNALLOWED);
			CB.cmd_error();
			// TODO 10.4.6 405 Method Not Allowed
			// The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.
			// The response MUST include an Allow header containing a list of valid methods for the requested resource.
	}

	// evbuffer_execute() will call finish() when at the end of buffer
	if (!res.reply_sent())
		tagl.finish();

	if (!req.query_opt_context().empty())
		TS->pop_context();

	if (tx.trace_on && tx.has_errors())
		tx.print_errors();

	// TS will accumulate errors between requests
	// TODO don't allow this
	TS->clear_errors();
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

