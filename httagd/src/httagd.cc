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

void tagl_callback::cmd_put(const tagd::abstract_tag& t) {
	_tx->TS->put(t, _driver->flags());
	std::stringstream ss;
	if (_tx->TS->has_error()) {
		_tx->TS->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void tagl_callback::cmd_del(const tagd::abstract_tag& t) {
	_tx->TS->del(t, _driver->flags());
	std::stringstream ss;
	if (_tx->TS->has_error()) {
		_tx->TS->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void tagl_callback::cmd_query(const tagd::interrogator& q) {
	std::cerr << "tagl_callback::cmd_query:\n";
	std::cerr << q << std::endl;
	tagd::tag_set T;
	tagd::code ts_rc = _tx->TS->query(T, q, _driver->flags());
	std::cerr << tagd_code_str(ts_rc) << ", size: " << T.size() << std::endl;

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

void tagl_callback::cmd_error() {
	std::stringstream ss;
	_driver->print_errors(ss);
	_tx->res->add(ss.str());
}

void tagl_callback::empty() {
	_tx->res->add("This is tagd");
	_tx->res->send_reply(EVHTP_RES_OK);
}

void tagl_callback::finish() {
	if (_tx->res->reply_sent()) return;

	_tx->res->send_reply(
		_driver->most_severe(
			_tx->TS->most_severe(tagd::TAGD_OK)
		)
	);
}

std::string request::url() const {
	std::string url;
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

	if ( _ev_req->uri->authority ) {
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
	} else {
		// TODO not sure this is a good way to do it
		url.append( _server->bind_addr() );
		url.append(":").append( std::to_string(_server->bind_port()) );
	}

	if ( _ev_req->uri->path->full )
		url.append( _ev_req->uri->path->full );

	if ( _ev_req->uri->query_raw )
		url.append("?").append( reinterpret_cast<const char *>(_ev_req->uri->query_raw) );

	if ( _ev_req->uri->fragment )
		url.append("#").append( reinterpret_cast<const char *>(_ev_req->uri->fragment) );

	return url;
}


void main_cb(evhtp_request_t *ev_req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;
	// for now, this request uses the servers tagspace reference
	// TODO allow requests to use other tagspaces (given the request)
	auto *TS = svr->TS();
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

	// TODO add server obj to transaction, get rid of viewspace cus server has a ref
	// get rid of context cus its accessible through request
	transaction tx(TS, &req, &res, svr->VS(), opt_context);

	// shared pointer between internal errors structure
	tx.share_errors(
		tagl.share_errors(
			TS->share_errors(
				*(svr->VS())
			)
		)
	);

	if (opt_view.empty())
		opt_view = svr->args()->default_view;

	// determine TAGL::callback pointer based on media type
	if (opt_view.empty() || opt_view == "tagl") {
		CB = new httagd::tagl_callback(&tx);
	} else {
		CB = new httagd::html_callback(&tx);
	}
	tagl.callback_ptr(CB);

	// route request
	switch(method) {
		case htp_method_HEAD:
			req.method = HTTP_HEAD;
			// identical to GET request, but content body not added
			tagl.tagdurl_get(req);
			tagl.finish();
			break;
		case htp_method_GET:
			req.method = HTTP_GET;
			tagl.tagdurl_get(req);
			tagl.finish();
			break;
		case htp_method_PUT:
			req.method = HTTP_PUT;
			tagl.tagdurl_put(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
			tagl.finish();
			break;
		case htp_method_POST:
			req.method = HTTP_POST;
			// TODO check path
			tagl.evbuffer_execute(ev_req->buffer_in);
			tagl.finish();
			break;
		case htp_method_DELETE:
			req.method = HTTP_DELETE;
			tagl.tagdurl_del(req);
			tagl.evbuffer_execute(ev_req->buffer_in);
			tagl.finish();
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

// TODO add to httagd::callback
void output_errors(transaction& tx, tagd::code ret_tc) {
	if (!tx.size())
		tx.ferror(tagd::TS_INTERNAL_ERR, "no errors to output, returned: %s", tagd_code_str(ret_tc));

	if (tx.trace_on) tx.print_errors();

	view vw;
	// get the error_function view
	if (tx.VS->get(vw, error_view_id(tx.req->effective_opt_view())) != tagd::TAGD_OK) {
		// adds plain text errors to response output buffer
		// for errorable objects contained by transaction
		tx.add_errors();
		return;
	}

	// attempts to add error_function response for the errorable object
	// adds plain text of error_function fails
	errorable_function_t f_err = [&tx, &vw](const tagd::errorable& e) -> tagd::code {
		tagd::code tc = vw.error_function(tx, vw, e);
		if (tc != tagd::TAGD_OK) {
			tx.add_errors();  // add all errors plain text
			return tc;
		}

		return tagd::TAGD_OK;
	};

	tx.add_errors(f_err);
}

void html_callback::cmd_get(const tagd::abstract_tag& t) {
	if (_tx->trace_on) std::cerr << "cmd_get()" << std::endl;
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
		output_errors(*_tx, tc);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, get_view_id(_tx->req->effective_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}

	tc = vw.get_function(*_tx, vw, T);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

void html_callback::cmd_query(const tagd::interrogator& q) {
	if (_tx->trace_on) std::cerr << "cmd_query()" << std::endl;

	tagd::tag_set R;
	tagd::code tc = _tx->TS->query(R, q);

	if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND) {
		output_errors(*_tx, tc);
		return;
	}

	view vw;
	tc = _tx->VS->get(vw, query_view_id(_tx->req->effective_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}

	tc = vw.query_function(*_tx, vw, q, R);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

void html_callback::cmd_error() {
	if (_tx->trace_on) std::cerr << "cmd_error()" << std::endl;

	if (_tx->has_error())
		output_errors(*_tx, _tx->code());
}


void html_callback::empty() {
	if (_tx->trace_on) std::cerr << "empty()" <<  std::endl;

	view vw;
	tagd::code tc = _tx->VS->get(vw, empty_view_id(_tx->req->effective_opt_view()));
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}

	tc = vw.empty_function(*_tx, vw);
	if (tc != tagd::TAGD_OK) {
		output_errors(*_tx, tc);
		return;
	}
}

tagd::code tagd_template::expand(const std::string& fname) {
	if (fname.empty()) {
		this->ferror( tagd::TAGD_ERR, "template file required" );
		return tagd::TAGD_ERR;
	}

	if (!ctemplate::LoadTemplate(fname, ctemplate::DO_NOT_STRIP)) {
		this->ferror( tagd::TAGD_ERR, "load template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	// this also would load (if not already loaded), but lets load and expand in seperate
	// steps so we can better trace the source of errors
	if (!ctemplate::ExpandTemplate(fname, ctemplate::DO_NOT_STRIP, _dict, _output)) {
		this->ferror( tagd::TAGD_ERR, "expand template failed: %s" , fname.c_str() );
		return tagd::TAGD_ERR;
	}

	return tagd::TAGD_OK;
}

tagd_template* tagd_template::include(const std::string &id, const std::string &fname) {
	auto d = _dict->AddIncludeDictionary(id);
	d->SetFilename(fname);
	return this->new_sub_template(d);
}

// sets the value of a template marker to <key> and also its
// corresponding <key>_lnk marker to the hyperlink representation of <key>
void tagd_template::set_tag_link (transaction& tx, const std::string& key, const std::string& val) {
	if (val == "*") { // wildcard relator
		// empty link
		this->set_value(key, val);
		return;
	}

	std::string opt_str;

	std::string view_name = tx.req->query_opt_view();
	if (!view_name.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(QUERY_OPT_VIEW);
		opt_str.push_back(('='));
		opt_str.append(tagd::uri_encode(view_name));
	}

	std::string context = tx.req->query_opt_context();
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

} // namespace httagd

