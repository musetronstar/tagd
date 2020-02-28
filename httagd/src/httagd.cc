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

/*
void print_evbuf(struct evbuffer *input, std::ostream &os=std::cout) {
	const size_t BUF_SZ = 512;
	char buf[BUF_SZ];
	size_t sz = 0, read_sz = BUF_SZ - 1;

	os << "-*** input buffer ***-" << std::endl;
	do {
		sz = evbuffer_copyout(input, buf, read_sz); 
		if (sz <= 0) break;
		os << std::string(buf, sz);
		if (sz < read_sz) break;  // EOF
	} while (true);
	os << std::endl
	   << "-********************-" << std::endl;
}

int output_header(evhtp_header_t * header, void* arg) {
    // evbuf_t * buf = (evbuf_t *)arg;
    // evbuffer_add_printf(buf, "print_kvs() key = '%s', val = '%s'\n",
      //                   header->key, header->val);
    printf("%s: %s\n", header->key, header->val);
    return 0;
}

evhtp_res print_kvs(evhtp_request_t * req, evhtp_headers_t * hdrs, void *) {
	printf("-*** headers ***-\n");
    evhtp_headers_for_each(hdrs, output_header, req->buffer_out);
	printf("-************-\n");
    return EVHTP_RES_OK;
}

evhtp_res print_path(evhtp_request_t * req, evhtp_path_t * path, void * arg) {
    // if (ext_body) {
    //     evbuffer_add_printf(req->buffer_out, "ext_body: '%s'\n", ext_body);
    // }

    // evbuffer_add_printf(req->buffer_out,
    //                     "print_path() full        = '%s'\n"
    //                     "             path        = '%s'\n"
    //                     "             file        = '%s'\n"
    //                     "             match start = '%s'\n"
    //                     "             match_end   = '%s'\n"
    //                     "             methno      = '%d'\n",
    //                     path->full, path->path, path->file,
    //                     path->match_start, path->match_end,
    //                     evhtp_request_get_method(req));

	printf(
                        "-*** path ***-\n"
                        "full        = '%s'\n"
                        "path        = '%s'\n"
                        "file        = '%s'\n"
                        "match start = '%s'\n"
                        "match_end   = '%s'\n"
                        "methno      = '%s'\n"
                        "-************-\n",
                        path->full, path->path, path->file,
                        path->match_start, path->match_end,
                        evhtp_method_str(evhtp_request_get_method(req)));

    return EVHTP_RES_OK;
}

evhtp_res set_my_connection_handlers(evhtp_connection_t * conn, void * arg) {
    //struct timeval               tick;
    //struct ev_token_bucket_cfg * tcfg = NULL;

    // evhtp_connection_set_hook(conn, evhtp_hook_on_header, print_kv, "foo");
    // evhtp_connection_set_hook(conn, evhtp_hook_on_headers, print_kvs, "bar");
	evhtp_connection_set_hook(conn, evhtp_hook_on_headers, print_kvs, "baz");
    evhtp_connection_set_hook(conn, evhtp_hook_on_path, print_path, "baz");
    // evhtp_connection_set_hook(conn, evhtp_hook_on_read, print_data, "derp");
    // evhtp_connection_set_hook(conn, evhtp_hook_on_new_chunk, print_new_chunk_len, NULL);
    // evhtp_connection_set_hook(conn, evhtp_hook_on_chunk_complete, print_chunk_complete, NULL);
    // evhtp_connection_set_hook(conn, evhtp_hook_on_chunks_complete, print_chunks_complete, NULL);

    // if (bw_limit > 0) {
    //     tick.tv_sec  = 0;
    //     tick.tv_usec = 500 * 100;

    //     tcfg         = ev_token_bucket_cfg_new(bw_limit, bw_limit, bw_limit, bw_limit, &tick);

    //     bufferevent_set_rate_limit(conn->bev, tcfg);
    // }

    //evhtp_connection_set_hook(conn, evhtp_hook_on_request_fini, test_fini, tcfg);

    return EVHTP_RES_OK;
}
*/


namespace httagd {

void htscanner::scan_tagdurl_path(int cmd, const request& req) {
	std::string path = req.path();
	// path separator defs
	const size_t max_seps = 2;
	size_t seps[max_seps] = {0, 0};  // offsets of '/' chars

	std::string opt_search = req.query_opt_search();
	auto f_parse_search_terms = [this, &opt_search]() {
		this->_driver->parse_tok(TOK_RELATOR, new std::string(HARD_TAG_HAS));
		this->_driver->parse_tok(TOK_TAG, new std::string(HARD_TAG_TERMS));
		this->_driver->parse_tok(TOK_EQ, NULL);
		this->_driver->parse_tok(TOK_QUOTED_STR, new std::string(opt_search));
	};

	if ((path == "" || path == "/") && cmd == TOK_CMD_GET) {
		if (opt_search.empty()) {
			// home page or welcome message
			dynamic_cast<httagd::callback*>(_driver->callback_ptr())->empty();
		}
		else {	// FTS query
			_driver->parse_tok(TOK_CMD_QUERY, NULL);
			_driver->parse_tok(TOK_QUOTED_STR, new std::string(opt_search));
		}
		return;
	}

	if (path[0] != '/') {
			_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
			return;
	}

	size_t num_seps = 1;
	{
		size_t i = 1;
		/* HDURI if path is:
		 *   /hd:rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
		 * we have to advance past the path seps in the HDURI
		 */
		if (path.substr(i, tagd::HDURI_SCHEME.size()) == tagd::HDURI_SCHEME) {
			i = i + tagd::HDURI_SCHEME.size();
			size_t hduri_delim_count = 0;
			for (; i < path.size(); ++i) {
				if (path[i] == tagd::HDURI_DELIM) {
					if (++hduri_delim_count == tagd::HDURI_DELIM_COUNT) {
						i++;
						break;
					}
				}
			}
		}

		// find the offsets of remain path separators
		for(; i < path.size(); ++i) {
			if (path[i] == '/') {
				if (num_seps == max_seps) {
					// TODO use error tag
					_driver->error(tagd::TAGL_ERR, "max_seps exceeded");
					return;
				}
				seps[num_seps++] = i;
			}
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

			// first segment of "*" is a placeholder for sub relation, so ignore it
			if (segment != "*") {  // how is it related
				_driver->parse_tok(TOK_SUB_RELATOR, (new std::string(HARD_TAG_SUB)));
				this->scan(tagd::uri_decode(segment));
			}
		} else {
			if (!opt_search.empty()) {  // turn into query (like adding a '/' to the end)
				f_parse_cmd_query();
				f_parse_search_terms();
				return;
			} else {
				_driver->parse_tok(cmd, NULL);
				auto decoded = tagd::uri_decode(segment);
				this->scan(decoded);
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

		/*\
		|*|	parse the cmd and tag_id for POSTs and DELETEs
		|*| (prepended to the TAGL statement in the body of the http request)
		|*| e.g.:
		|*|   POST /dog
		|*|
		|*|   has legs, can bark
		|*|
		|*|	whereas PUTs only constrain the tag id in the TAGL statement(s)
		|*| e.g.:
		|*|   PUT /dog
		|*|
		|*|   >> dog is_a animal has legs, can bark
		\*/

		auto tag_id = tagd::uri_decode(segment);
		_driver->constrain_tag_id(tag_id);

		if (req.method != HTTP_PUT) {
			_driver->parse_tok(cmd, NULL);
			this->scan(tag_id);
		}
	}

	if (++sep_i >= num_seps)
		return;

	// second segment - how is it related?
	segment = path.substr(seps[sep_i]+1);

	if (!segment.empty()) {
		_driver->parse_tok(TOK_WILDCARD, NULL);
		this->scan(tagd::uri_decode(segment));
	}

	if (cmd == TOK_CMD_QUERY && !opt_search.empty())
		f_parse_search_terms();
}

// translate an HTTP request into a TAGL statement and execute
tagd::code httagl::execute(transaction& tx) {
	// translate evhtp_method into httagd::http_method
	htp_method ev_method = evhtp_request_get_method(tx.req->ev_req());
	switch(ev_method) {
		case htp_method_HEAD:
			tx.req->method = HTTP_HEAD;
			// identical to GET request, but content body not added
			this->tagdurl_get(*tx.req);
			break;
		case htp_method_GET:
			tx.req->method = HTTP_GET;
			this->tagdurl_get(*tx.req);
			break;
		case htp_method_PUT:
			tx.req->method = HTTP_PUT;
			if (tx.req->path().empty() || tx.req->path() == "/") {
				tx.error(tagd::HTTP_ERR, "tag id in path required for HTTP PUT");
			} else {
				this->tagdurl_put(*tx.req);
				TAGL::driver::execute(tx.req->ev_req()->buffer_in);
			}
			break;
		case htp_method_POST:
			tx.req->method = HTTP_POST;
			// if not empty, parse the tagdurl path
			if (!(tx.req->path().empty() || tx.req->path() == "/")) {
				this->tagdurl_put(*tx.req);	// put matches tagdb semantics, not http
			}
			TAGL::driver::execute(tx.req->ev_req()->buffer_in);
			break;
		case htp_method_DELETE:
			tx.req->method = HTTP_DELETE;
			this->tagdurl_del(*tx.req);
			TAGL::driver::execute(tx.req->ev_req()->buffer_in);
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
			tx.req->method = HTTP_UNKNOWN;
			// TODO use tagd::error
			tx.ferror(tagd::HTTP_ERR, "unsupported method: %s", evhtp_method_str(ev_method));
			tx.res->res_code(EVHTP_RES_METHNALLOWED);
			_callback->cmd_error();
	}

	return tx.code();
}

tagd::code httagl::tagdurl_get(const request& req) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_GET, req);

	return this->code();
}

tagd::code httagl::tagdurl_put(const request& req) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_PUT, req);

	return this->code();
}

tagd::code httagl::tagdurl_del(const request& req) {
	this->init();

	dynamic_cast<htscanner*>(_scanner)->scan_tagdurl_path(TOK_CMD_DEL, req);

	return this->code();
}

void callback::default_cmd_put(const tagd::abstract_tag& t) {
	auto ssn = _tx->drvr->session_ptr();
	_tx->tdb->put(t, ssn, _driver->flags());
	std::stringstream ss;
	if (_tx->tdb->has_errors()) {
		_tx->tdb->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void callback::default_cmd_del(const tagd::abstract_tag& t) {
	auto ssn = _tx->drvr->session_ptr();
	_tx->tdb->del(t, ssn, _driver->flags());
	std::stringstream ss;
	if (_tx->tdb->has_errors()) {
		_tx->tdb->print_errors(ss);
		_tx->res->add(ss.str());
	}
}

void callback::default_cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	auto ssn = _tx->drvr->session_ptr();
	tagd::code tc = _tx->tdb->query(T, q, ssn, _driver->flags());

	std::stringstream ss;
	if (tc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_tx->tdb->print_errors(ss);
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
	_tx->res->add("This is tagd\n");
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

evhtp_res response::tagd_code_evhtp_res(tagd::code tc) {
	switch (tc) {
		case tagd::TAGD_OK:
			return EVHTP_RES_OK;	// 200
		case tagd::TAGL_ERR:
		case tagd::TS_AMBIGUOUS:
		case tagd::HTTP_ERR:
			return EVHTP_RES_BADREQ;  // 401
		case tagd::TS_NOT_FOUND:
			return EVHTP_RES_NOTFOUND;	// 404
		case tagd::TS_DUPLICATE:
			// using 409 - Conflict, 422 - Unprocessable Entity might also be exceptable
			return EVHTP_RES_CONFLICT;
		default:
			return EVHTP_RES_SERVERR;	// 500
	}
}

void response::send_reply(tagd::code tc) {
	if (_tx->svr->args()->opt_trace)
		std::cerr << "send_reply => " << tagd::code_str(tc) << std::endl;
	this->send_reply(_res_code >= 0 ? _res_code : tagd_code_evhtp_res(tc));
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

request::request(transaction* tx, evhtp_request_t *ev_req)
	: _tx{tx}, _ev_req{ev_req}
{
	if ( ev_req->uri->query_raw != NULL ) {
		tagd::url::parse_query(
			_query_map, (char*)ev_req->uri->query_raw );
	}

	_path = ev_req->uri->path->full;
}

std::string request::canonical_url() const {
	std::string url;

	// TODO get the hostname from the Host header if not in the parsed uri
	if ( _ev_req->uri->authority && _ev_req->uri->authority->hostname ) {
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
	} /* else {
		// get authority from use Host header
	} */

	assert(!url.empty());

	if ( _ev_req->uri->path->full )
		url.append( _ev_req->uri->path->full );

	if ( _ev_req->uri->query_raw )
		url.append("?").append( reinterpret_cast<const char *>(_ev_req->uri->query_raw) );

	if ( _ev_req->uri->fragment )
		url.append("#").append( reinterpret_cast<const char *>(_ev_req->uri->fragment) );

	return url;
}

std::string request::abs_url() const {
	std::string url;

	if ( _ev_req->uri->path->full )
		url.append( _ev_req->uri->path->full );

	if ( _ev_req->uri->query_raw ) {
		if (url.empty()) url = "/";
		url.append("?").append( reinterpret_cast<const char *>(_ev_req->uri->query_raw) );
	}

	if ( _ev_req->uri->fragment ) {
		if (url.empty()) url = "/";
		url.append("#").append( reinterpret_cast<const char *>(_ev_req->uri->fragment) );
	}

	return url;
}

std::string request::abs_url_view(const std::string& view_name) const {
	std::string url;

	if ( _ev_req->uri->path->full )
		url = _ev_req->uri->path->full;
	else
		url = "/";

	// TODO we may want to replace the _query_map v=<name> attribute and keep others
	// for now just add  v=<view_name>
	url.append("?v=").append(view_name);

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
		_tx->ferror(tagd::TS_INTERNAL_ERR, "no errors to output, returned: %s", tagd::code_str(ret_tc));

	if (_tx->trace_on) _tx->print_errors();

	// get the error_function view
	view vw;
	_tx->vws->report_errors = false;
	tagd::code tc = _tx->vws->get(vw, error_view_id(_tx->req->effective_opt_view()));
	_tx->vws->report_errors = true;

	// call error_function_t for the errorable object
	if (tc == tagd::TAGD_OK)
		tc = vw.error_function(*_tx, vw, *_tx);
	else
		tc = _tx->vws->fallback_error_view.error_function(*_tx, _tx->vws->fallback_error_view, *_tx);

	// add plain text errors if the call failed
	if (tc != tagd::TAGD_OK)
		_tx->add_errors();
}

#define RET_IF_ERR()              \
	if (tc != tagd::TAGD_OK) { \
		output_errors(tc);     \
		delete T;              \
		return;                \
	}

void callback::default_cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag *T;
	tagd::code tc;
	auto ssn = _tx->drvr->session_ptr();

	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url(t.id());
		if (!T->ok())
			tc = _tx->ferror(T->code(), "default_cmd_get parse url failed: %s", t.id().c_str());
		else
			tc = _tx->tdb->get(*T, static_cast<tagd::url *>(T)->hduri(), ssn, _driver->flags());
	} else {
		T = new tagd::abstract_tag();
		tc = _tx->tdb->get(*T, t.id(), ssn, _driver->flags());
	}

	// TODO, if outputting to an iostream is still desirable,
	// but more efficient to ouput to an evbuffer directly,
	// want to use std::ios_base::register_callback
	std::stringstream ss;
	if (tc == tagd::TAGD_OK) {
		ss << *T << std::endl;
	} else {
		_tx->tdb->print_errors(ss);
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
	RET_IF_ERR();

	// no err
	delete T;
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

	tagd::abstract_tag *T;
	tagd::code tc;

	auto ssn = _tx->drvr->session_ptr();
	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url(t.id());
		if (!T->ok())
			tc = _tx->ferror(T->code(), "cmd_get parse url failed: %s", t.id().c_str());
		else
			tc = _tx->tdb->get(*T, static_cast<tagd::url *>(T)->hduri(), ssn, _driver->flags());
	} else {
		T = new tagd::abstract_tag();
		tc = _tx->tdb->get(*T, t.id(), ssn, _driver->flags());
	}
	RET_IF_ERR();

	view vw;
	tc = _tx->vws->get(vw, get_view_id(view_name));
	RET_IF_ERR();

	tc = vw.get_function(*_tx, vw, *T);
	RET_IF_ERR();

	// no err
	delete T;
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
	auto ssn = _tx->drvr->session_ptr();
	tagd::code tc = _tx->tdb->query(R, q, ssn);

	if (tc != tagd::TAGD_OK && tc != tagd::TS_NOT_FOUND) {
		output_errors(tc);
		return;
	}

	view vw;
	tc = _tx->vws->get(vw, query_view_id(view_name));
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
	tagd::code tc = _tx->vws->get(vw, empty_view_id(view_name));
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

tagd::code tagd_template::expand(viewspace& vws, const std::string& fname) {
	if (this->expand(vws.fpath(fname)) != tagd::TAGD_OK)
		return vws.copy_errors(*this).code();

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
		tagd::HDURI u(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else if ( tagd::url::looks_like_url(val) ) {
		tagd::url u(val);
		f_set_vals(u.id(), tagd::uri_encode(u.hduri()));
	} else {
		f_set_vals(val, tagd::uri_encode(val));
	}
}

void main_cb(evhtp_request_t *ev_req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;

	// if (svr->trace_on) print_evbuf(ev_req->buffer_in);

	// for now, this request uses the servers tagdb reference
	// TODO allow requests to use other tagdbs (given the request)
	auto tdb = svr->tdb();
	auto vws = svr->vws();

	// circular dependencies between transaction and request/response pointers
	transaction tx(svr, tdb, nullptr, vws, nullptr, nullptr);
	request req(&tx, ev_req);
	response res(&tx, ev_req);
	tx.req = &req;
	tx.res = &res;

	auto ssn = tdb->get_session();
	if (!req.query_opt_context().empty())
		ssn.push_context(req.query_opt_context());

	callback CB(&tx);
	httagl tagl(tdb, &CB, &ssn);
	tx.drvr = &tagl;

	// all sharing the internal errors pointer of tx
	tx.share_errors(tagl)
	  .share_errors(*tdb)
	  .share_errors(*vws)
	  .share_errors(ssn);

	// route request, parse, call callback method, and write response
	tagl.execute(tx);

	// execute() should call finish() when at the end of buffer
	if (!res.reply_sent())
		tagl.finish();

	if (tx.trace_on && tx.has_errors()) {
		// TODO write to log
		tx.print_errors();
	}

	// tdb will accumulate errors between requests
	if (tdb->has_errors()) {
		// TODO write to log
		tdb->print_errors();
		tdb->clear_errors();
	}
}

tagd::code server::start() {

	if (_args->opt_trace) {
		_tdb->trace_on();
		TAGL::driver::trace_on((char *)"trace: ");
	}

	// for debug printing request data
	// evhtp_set_post_accept_cb(_htp, set_my_connection_handlers, nullptr);
    
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

