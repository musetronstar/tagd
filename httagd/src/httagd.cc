#include "httagd.h"
#include "tagl.h"
#include "parser.h"  // for CMDs
#include <evhtp.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>

// functions stat, open, close
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static tagd::logger *HTTAGD_LOGGER = nullptr;

struct httagd_header_log_ctx {
	std::ostringstream *ss;
};

static int HTTAGD_LOG_EVHTP_HEADER(evhtp_header_t *hdr, void *arg) {
	auto *ctx = static_cast<httagd_header_log_ctx *>(arg);
	if (ctx->ss->tellp() > 0)
		*(ctx->ss) << "; ";
	*(ctx->ss) << hdr->key << ": " << hdr->val;
	return 0;
}

const char* evhtp_method_str(int method);

void HTTAGD_SET_LOGGER(tagd::logger *log) {
	HTTAGD_LOGGER = log;
}

bool HTTAGD_LOG_ENABLED(tagd::log_level lvl) {
	if (HTTAGD_LOGGER == nullptr)
		return false;

	return static_cast<int>(lvl) <= static_cast<int>(HTTAGD_LOGGER->level(std::string(HARD_TAG_ROLE_HTTAGD)));
}

void HTTAGD_LOG(tagd::log_level lvl, const std::string& msg) {
	if (HTTAGD_LOGGER == nullptr)
		return;

	HTTAGD_LOGGER->log(std::string(HARD_TAG_ROLE_HTTAGD), lvl, std::string("-- ").append(msg));
}

std::string HTTAGD_HTTP_HEADERS_STR(const struct evkeyvalq *headers) {
	std::ostringstream ss;
	if (headers == nullptr)
		return ss.str();

	for (const evkeyval *hdr = headers->tqh_first; hdr != nullptr; hdr = hdr->next.tqe_next) {
		if (ss.tellp() > 0)
			ss << "; ";
		ss << hdr->key << ": " << hdr->value;
	}

	return ss.str();
}

std::string HTTAGD_HTTP_HEADERS_STR(const evhtp_headers_t *headers) {
	std::ostringstream ss;
	if (headers == nullptr)
		return ss.str();

	httagd_header_log_ctx ctx{&ss};
	evhtp_headers_for_each(const_cast<evhtp_headers_t *>(headers), HTTAGD_LOG_EVHTP_HEADER, &ctx);
	return ss.str();
}

static void HTTAGD_LOG_ERRORS(tagd::log_level lvl, const tagd::errorable& err) {
	if (!HTTAGD_LOG_ENABLED(lvl) || !err.has_errors())
		return;

	std::stringstream ss;
	err.print_errors(ss);
	auto msg = ss.str();
	while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
		msg.pop_back();

	if (!msg.empty())
		HTTAGD_LOG(lvl, msg);
}

static void HTTAGD_LOG_REQUEST(tagd::log_level lvl, int method, const std::string& path, const evhtp_headers_t *headers, tagd::code tc) {
	if (!HTTAGD_LOG_ENABLED(lvl))
		return;

	std::stringstream ss;
	ss << "httagd request method="
	   << evhtp_method_str(method)
	   << " path=" << path
	   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(headers) << ']';
	HTTAGD_LOG(lvl, ss.str());
	HTTAGD_LOG(lvl, std::string("httagd code=").append(tagd::code_str(tc)));
}

#define HTTAGD_LOG_DEBUG(MSG) \
	do { \
		if (HTTAGD_LOG_ENABLED(tagd::log_level::DEBUG)) { \
			std::ostringstream _httagd_debug_os; \
			_httagd_debug_os << MSG; \
			HTTAGD_LOG(tagd::log_level::DEBUG, _httagd_debug_os.str()); \
		} \
	} while (0)

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

namespace {
constexpr std::string_view tagl_content_types[]{"text/plain", "application/x-www-form-urlencoded"};

bool accepts_tagl_content(const char* header) {
	std::string media = header ? header : "";
	media.resize(media.find(';') == std::string::npos ? media.size() : media.find(';'));
	const auto last = media.find_last_not_of(" \t");
	media = last == std::string::npos ? "" : media.substr(0, last + 1);
	media.erase(0, media.find_first_not_of(" \t"));
	for (char& c : media) {
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	}
	if (media.empty())
		return true;
	for (const auto type : tagl_content_types) {
		if (media == type)
			return true;
	}
	return false;
}

std::string accepted_tagl_content_types() {
	std::string header;
	for (const auto type : tagl_content_types) {
		if (!header.empty())
			header += ", ";
		header += type;
	}
	return header;
}
}

// Select the request operation; TAGL owns body grammar and command restrictions.
tagd::code httagl::execute(transaction& tx) {
	// translate evhtp_method into tagd::http_method
	htp_method ev_method = evhtp_request_get_method(tx.req->ev_req());
	// Addressed mutations require write authority before any body is parsed.
	if ((ev_method == htp_method_PUT || ev_method == htp_method_PATCH || ev_method == htp_method_DELETE
		|| (ev_method == htp_method_POST && !tx.req->has_root_path()))
		&& !permit_write()) {
		_callback->cmd_error();
		return code();
	}

	// Reject unsupported formats before applying contexts or parsing statements.
	if (ev_method == htp_method_PUT || ev_method == htp_method_POST || ev_method == htp_method_PATCH) {
		const auto* header = evhtp_header_find(tx.req->ev_req()->headers_in, "Content-Type");
		if (!accepts_tagl_content(header)) {
			const auto accepted = accepted_tagl_content_types();
			tx.res->res_code(EVHTP_RES_UNSUPPORTED);
			if (ev_method == htp_method_PATCH)
				tx.res->add_header("Accept-Patch", accepted);
			return error(tagd::TS_MISUSE, "unsupported content type; accepted TAGL types: " + accepted);
		}
	}

	switch(ev_method) {
		case htp_method_HEAD:
			tx.req->method = tagd::HTTP_HEAD;
			// HEAD executes GET semantics; the response suppresses its body.
			this->tagdurl_get(*tx.req);
			break;
		case htp_method_GET:
			tx.req->method = tagd::HTTP_GET;
			this->tagdurl_get(*tx.req);
			break;
		case htp_method_PUT:
		case htp_method_PATCH:
			tx.req->method = ev_method == htp_method_PUT ? tagd::HTTP_PUT : tagd::HTTP_PATCH;
			if (tx.req->has_root_path())
				return error(tagd::TS_MISUSE, "tag id in path required for PUT/PATCH");
			if (tagdurl_put(*tx.req) != tagd::TAGD_OK)
				return code();
			if (ev_method == htp_method_PATCH) {
				const auto identity = tagd::canonical_url_id(constrain_tag_id);
				const auto pos = tsp()->pos(identity, session_ptr());
				if (has_errors())
					return code();
				if (pos == tagd::POS_UNKNOWN)
					return error(tagd::TS_NOT_FOUND, "PATCH subject does not exist: " + constrain_tag_id);
			}
			TAGL::driver::execute(tx.req->ev_req()->buffer_in);
			break;
		case htp_method_POST:
			tx.req->method = tagd::HTTP_POST;
			if (!tx.req->has_root_path()) {
				if (tagdurl_put(*tx.req) != tagd::TAGD_OK)
					return code();
			} else if (apply_tagdurl_context(tx.req->query_contexts()) != tagd::TAGD_OK) {
				return code();
			}
			TAGL::driver::execute(tx.req->ev_req()->buffer_in);
			break;
		case htp_method_DELETE:
			tx.req->method = tagd::HTTP_DELETE;
			if (tagdurl_del(*tx.req) != tagd::TAGD_OK)
				return code();
			if (evbuffer_get_length(tx.req->ev_req()->buffer_in))
				return error(tagd::TS_MISUSE, "DELETE does not accept request content");
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
		htp_method_UNKNOWN,
  */

		default:
			tx.req->method = tagd::HTTP_UNKNOWN;
			// TODO use tagd::error
			tx.ferror(tagd::HTTP_ERR, "unsupported method: %s", evhtp_method_str(ev_method));
			tx.res->res_code(EVHTP_RES_METHNALLOWED);
			_callback->cmd_error();
	}

	return tx.code();
}

tagd::code httagl::tagdurl_get(const request& req) {
	return this->scan_request_tagdurl(req);
}

tagd::code httagl::tagdurl_put(const request& req) {
	const auto rc = scan_request_tagdurl(req);
	if (rc == tagd::TAGD_OK)
		http_body(req.method == tagd::HTTP_PUT ? TAGL::http_body_commands::replace
			: TAGL::http_body_commands::merge);
	return rc;
}

tagd::code httagl::tagdurl_del(const request& req) {
	return this->scan_request_tagdurl(req);
}

tagd::code httagl::scan_request_tagdurl(const request& req) {
	this->init();
	return TAGL::driver::scan_tagdurl(req.method, req.operation_path(), req.address());
}

void callback::default_cmd_put(const tagd::abstract_tag& t) {
	auto ssn = _tx->drvr->session_ptr();
	// HTTP merges succeed when the requested state already holds, regardless of SET.
	const auto flags = _driver->flags | tagd::F_IGNORE_DUPLICATES;
	const auto tc = _tx->tsp()->put(t, ssn, flags);
	if (tc != tagd::TAGD_OK)
		output_errors(tc);
}

void callback::default_cmd_delete(const tagd::abstract_tag& t) {
	auto ssn = _tx->drvr->session_ptr();
	const auto tc = _tx->tsp()->del(t, ssn, _driver->flags);
	if (tc != tagd::TAGD_OK)
		output_errors(tc);
}

void callback::default_cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	auto ssn = _tx->drvr->session_ptr();
	const auto tc = _tx->tsp()->query(T, q, ssn, _driver->flags);
	// An empty QUERY result has a status, but must not add a batch-stopping error.
	const bool empty_result = tc == tagd::TS_NOT_FOUND && T.empty() && !_tx->size();
	if (tc != tagd::TAGD_OK && !empty_result) {
		output_errors(tc);
		return;
	}

	if (T.empty())
		return;

	std::stringstream ss;
	tagd::print_tag_ids(T, ss);
	ss << std::endl;
	_tx->res->add(ss.str());
}

void callback::default_empty() {
	_tx->res->add("This is tagd\n");
}

void callback::finish() {
	assert(!_tx->res->reply_sent());  // should only be called once
	if (_tx->res->reply_sent()) {
		LOG_ERROR( "reply already sent" << std::endl )
		return;
	}

	if (!_tx->res->header_content_type_added())
		_tx->res->add_header_content_type(DEFAULT_CONTENT_TYPE);

	if (_tx->drvr->http_forbidden())
		_tx->res->res_code(EVHTP_RES_FORBIDDEN);
	_tx->res->send_reply(
		_tx->most_severe(_tx->drvr->session_ptr()->code())
	);
}

evhtp_res tagd_code_evhtp_res(tagd::code tc) {
	switch (tc) {
		case tagd::TAGD_OK:
			return EVHTP_RES_OK;	// 200
		case tagd::TAGL_ERR:
		case tagd::HTTP_ERR:
		case tagd::TS_MISUSE:
		// URL syntax errors come from the request's own path or body text.
		case tagd::URI_ERR_SCHEME:
		case tagd::URL_EMPTY:
		case tagd::URL_MAX_LEN:
		case tagd::URL_ERR_SCHEME:
		case tagd::URL_ERR_HOST:
		case tagd::URL_ERR_PORT:
		case tagd::URL_ERR_PATH:
		case tagd::URL_ERR_USER:
		case tagd::HDURI_ERR:
			return EVHTP_RES_BADREQ;  // 400
		case tagd::TS_NOT_FOUND:
			return EVHTP_RES_NOTFOUND;	// 404
		case tagd::TS_CONTRADICTION:
		case tagd::TS_DUPLICATE:
		case tagd::TS_AMBIGUOUS:
			// using 409 - Conflict, 422 - Unprocessable Entity might also be exceptable
			return EVHTP_RES_CONFLICT;
		default:
			return EVHTP_RES_SERVERR;	// 500
	}
}

void response::send_reply(tagd::code tc) {
	HTTAGD_LOG_DEBUG("httagd send_reply code=" << tagd::code_str(tc));
	this->send_ev_reply(_res_code >= 0 ? _res_code : tagd_code_evhtp_res(tc));
}

void response::send_ev_reply(evhtp_res res) {
	assert(!_reply_sent);  // we shouldn't be calling this more than once
	if (_reply_sent) {
		LOG_ERROR( "reply already sent" << std::endl )
		return;
	}

	// HEAD keeps GET's representation length but never sends a body, including QUERY/errors.
	if (evhtp_request_get_method(_ev_req) == htp_method_HEAD) {
		const auto length = evbuffer_get_length(_ev_req->buffer_out);
		if (evhtp_header_find(_ev_req->headers_out, "Content-Length") == nullptr)
			add_header_content_length(length);
		evbuffer_drain(_ev_req->buffer_out, length);
	}

	// Browsers render the declared type, so TAGL text holding markup never runs as HTML.
	add_header("X-Content-Type-Options", "nosniff");

	HTTAGD_LOG_DEBUG("httagd send_ev_reply res=" << res << " name=" << evhtp_res_str(res));
	{
		std::ostringstream ss;
		ss << "httagd response status=" << res
		   << " name=" << evhtp_res_str(res)
		   << " body_size=" << evbuffer_get_length(_ev_req->buffer_out)
		   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(_ev_req->headers_out) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}

	evhtp_send_reply(_ev_req, res);
	_res_code = res;
	_reply_sent = true;
}

void response::add_header(const std::string &k, const std::string &v) {
	HTTAGD_LOG_DEBUG("httagd add_header key=\"" << k << "\" value=\"" << v << "\"");

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

	// TODO we may want to replace the request's parsed v=<name> option and keep others
	// for now just add  v=<view_name>
	url.append("?v=").append(TAGL::encode_tagdurl_option(view_name));

	return url;
}

std::string transaction::effective_opt_view() const {
	std::string view_opt = this->req->query_opt(TAGL::QUERY_OPT_VIEW);
	if (view_opt.empty()) {
		if (!this->svr->configuration().default_view().empty())
			return this->svr->configuration().default_view();  // user supplied default
		else
			return DEFAULT_VIEW;  // hard-coded default
	}
	return view_opt;
}

bool callback::resolve_view(view& result, view_action action) {
	const auto tc = _tx->vws->resolve(result, {_tx->effective_opt_view(), action});
	if (tc != tagd::TAGD_OK) {
		output_errors(tc);
		return false;
	}

	return true;
}

void callback::output_errors(tagd::code ret_tc) {
	// The driver may redispatch the shared errors after a command already rendered them.
	if (_errors_rendered)
		return;
	_errors_rendered = true;

	// A failing handler may return only a code; retain it when creating the diagnostic.
	if (!_tx->size())
		_tx->ferror(ret_tc == tagd::TAGD_OK ? tagd::TS_INTERNAL_ERR : ret_tc,
			"operation failed: %s", tagd::code_str(ret_tc));

	HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, *_tx);

	const auto selected = _tx->effective_opt_view();
	const auto* stored = _tx->vws->find(error_view_id(selected));
	// Built-in TAGL supplies its own ERROR representation, even with an app fallback.
	if (!stored && selected != DEFAULT_VIEW) {
		// Configuration remains the source of truth, including local runtime edits.
		const auto fallback = _tx->svr->configuration().fallback_error_view();
		stored = _tx->vws->find(error_view_id(fallback));
	}
	view vw = stored ? *stored : default_error_view;
	const auto tc = vw.error_function(*_tx, vw, *_tx);

	// add plain text errors if the call failed
	if (tc != tagd::TAGD_OK)
		_tx->add_errors();
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
			tc = _tx->tsp()->get(*T, static_cast<tagd::url *>(T)->hduri(), ssn, _driver->flags);
	} else {
		T = new tagd::abstract_tag();
		tc = _tx->tsp()->get(*T, t.id(), ssn, _driver->flags);
	}

	// TODO, if outputting to an iostream is still desirable,
	// but more efficient to ouput to an evbuffer directly,
	// want to use std::ios_base::register_callback
	std::stringstream ss;
	if (tc == tagd::TAGD_OK) {
		ss << *T << std::endl;
	} else {
		output_errors(tc);
	}

	if (ss.str().size()) {
		_tx->res->add(ss.str());
	}

	// no err
	delete T;
}

#define RET_IF_ERR()              \
	if (tc != tagd::TAGD_OK) { \
		this->output_errors(tc);     \
		delete T;              \
		return;                \
	}

void callback::cmd_get(const tagd::abstract_tag& t) {
	HTTAGD_LOG_DEBUG("httagd cmd_get");

	view vw;
	if (!resolve_view(vw, view_action::GET))
		return;
	if (vw.action() == view_action::UNDEF)
		return this->default_cmd_get(t);

	/*
	 * Render GET's representation so HEAD retains the same metadata.
	 * response::send_ev_reply suppresses the body for every view and error path.
	 * TODO: Avoid rendering HEAD bodies when their metadata can be obtained directly.
	 */

	tagd::abstract_tag *T;
	tagd::code tc;

	auto ssn = _tx->drvr->session_ptr();
	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url(t.id());
		if (!T->ok())
			tc = _tx->ferror(T->code(), "cmd_get parse url failed: %s", t.id().c_str());
		else
			tc = _tx->tsp()->get(*T, static_cast<tagd::url *>(T)->hduri(), ssn, _driver->flags);
	} else {
		T = new tagd::abstract_tag();
		tc = _tx->tsp()->get(*T, t.id(), ssn, _driver->flags);
	}
	RET_IF_ERR();

	tc = vw.get_function(*_tx, vw, *T);
	RET_IF_ERR();

	// no err
	delete T;
}

void callback::cmd_put(const tagd::abstract_tag& t) {
	const bool patch = _tx->req->method == tagd::HTTP_PATCH;
	view vw;
	if (!resolve_view(vw, patch ? view_action::GET : view_action::PUT))
		return;

	if (!patch && vw.action() == view_action::UNDEF)
		return this->default_cmd_put(t);

	auto ssn = _tx->drvr->session_ptr();
	auto tc = _tx->tsp()->put(t, ssn, _driver->flags | tagd::F_IGNORE_DUPLICATES);
	if (tc == tagd::TAGD_OK && patch)
		return mutation_response(t);

	// The representation describes committed storage; rendering cannot roll it back.
	if (tc == tagd::TAGD_OK)
		tc = vw.put_function(*_tx, vw, t);

	if (tc != tagd::TAGD_OK)
		output_errors(tc);
}

void callback::cmd_replace(const tagd::abstract_tag& t) {
	view vw;
	if (!resolve_view(vw, view_action::GET))
		return;
	tagd::replace_outcome outcome{};
	// The outcome controls HTTP status; TAGL's duplicate setting cannot turn success into 409.
	const auto rc = _tx->tsp()->replace(outcome, t, _driver->session_ptr(),
		_driver->flags | tagd::F_IGNORE_DUPLICATES);
	if (rc != tagd::TAGD_OK)
		return output_errors(rc);
	if (_tx->req->method == tagd::HTTP_PUT)
		mutation_response(t, outcome == tagd::replace_outcome::created);
}

void callback::mutation_response(const tagd::abstract_tag& t, bool created) {
	const bool render = _tx->req->query_map().contains(TAGL::QUERY_OPT_VIEW)
		|| _tx->effective_opt_view() != DEFAULT_VIEW;
	// A selected view renders the committed tag using GET, including retained system facts.
	if (render)
		cmd_get(t);
	if (!_tx->has_errors())
		_tx->res->res_code(created ? EVHTP_RES_CREATED : render ? EVHTP_RES_OK : EVHTP_RES_NOCONTENT);
}

void callback::cmd_delete(const tagd::abstract_tag& t) {
	const bool patch = _tx->req->method == tagd::HTTP_PATCH;
	view vw;
	if (!resolve_view(vw, patch ? view_action::GET : view_action::DELETE))
		return;

	if (!patch && vw.action() == view_action::UNDEF)
		return this->default_cmd_delete(t);

	auto ssn = _tx->drvr->session_ptr();
	auto tc = _tx->tsp()->del(t, ssn, _driver->flags);
	if (tc == tagd::TAGD_OK && patch)
		return mutation_response(t);

	// As with PUT, a failed deletion has no success representation.
	if (tc == tagd::TAGD_OK)
		tc = vw.delete_function(*_tx, vw, t);

	if (tc != tagd::TAGD_OK)
		output_errors(tc);
}


void callback::cmd_query(const tagd::interrogator& q) {
	HTTAGD_LOG_DEBUG("httagd cmd_query");

	view vw;
	if (!resolve_view(vw, view_action::QUERY))
		return;
	if (vw.action() == view_action::UNDEF)
		return this->default_cmd_query(q);

	tagd::tag_set R;
	auto ssn = _tx->drvr->session_ptr();
	tagd::code tc = _tx->tsp()->query(R, q, ssn, _driver->flags);
	// App QUERY handlers represent empty sets too; genuine diagnostics still fail.
	const bool empty_result = tc == tagd::TS_NOT_FOUND && R.empty() && !_tx->size();
	if (tc != tagd::TAGD_OK && !empty_result) {
		this->output_errors(tc);
		return;
	}

	tc = vw.query_function(*_tx, vw, q, R);
	if (tc != tagd::TAGD_OK) {
		this->output_errors(tc);
		return;
	}
}

void callback::cmd_error() {
	HTTAGD_LOG_DEBUG("httagd cmd_error");

	this->output_errors(_tx->code());
}


void callback::empty() {
	HTTAGD_LOG_DEBUG("httagd empty");

	view vw;
	if (!resolve_view(vw, view_action::EMPTY))
		return;
	if (vw.action() == view_action::UNDEF)
		return this->default_empty();

	const auto tc = vw.empty_function(*_tx, vw);
	if (tc != tagd::TAGD_OK) {
		this->output_errors(tc);
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
void tagd_template::set_tag_link(const TAGL::tagdurl_option_map& query_map, const std::string& key,
	const std::string& val, const std::string& selector) {
	if (val == "*") { // wildcard relator
		// empty link
		this->set_value(key, val);
		return;
	}

	std::string opt_str;

	std::string view_name;
	if (const auto found = query_map.find(TAGL::QUERY_OPT_VIEW); found != query_map.end())
		view_name = found->second;
	if (!view_name.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(TAGL::QUERY_OPT_VIEW);
		opt_str.push_back(('='));
		opt_str.append(TAGL::encode_tagdurl_option(view_name));
	}

	std::string context;
	if (const auto found = query_map.find(TAGL::QUERY_OPT_CONTEXT); found != query_map.end())
		context = found->second;
	if (!context.empty()) {
		opt_str.push_back(opt_str.empty() ? '?' : '&');
		opt_str.append(TAGL::QUERY_OPT_CONTEXT);
		opt_str.push_back(('='));
		opt_str.append(TAGL::encode_tagdurl_option(context));
	}

	// TODO: Consider HTTP resolution redirects or _resolve_redirect; preserve aliases and _ here.
	// sets template key and key_lnk with their corresponding values
	auto f_set_vals = [this, &key, &opt_str, &selector](const std::string& key_val, const std::string& lnk_val) {
		this->set_value(
			std::string(key).append("_lnk"),
			std::string("/").append(selector.empty() ? "_/" : TAGL::encode_tagdurl_tag_id(selector) + "/")
				.append(lnk_val).append(opt_str)
		);
		this->set_value(key, key_val);
	};

	// Only URL spellings require core parsing; ordinary labels keep their nominal identity.
	if (tagd::url::looks_like_hduri(val) || tagd::url::looks_like_url(val)
		|| tagd::url::looks_like_mailto(val)) {
		const tagd::url u(val);
		f_set_vals(u.ok() ? u.id() : val, TAGL::encode_tagdurl_tag_id(u.ok() ? u.hduri() : val));
	} else {
		f_set_vals(val, TAGL::encode_tagdurl_tag_id(val));
	}
}

void main_cb(evhtp_request_t *ev_req, void *arg) {
	httagd::server *svr = (httagd::server*)arg;

	// Every request begins with the server default; selection never leaks to another request.
	auto tsp = svr->tsp();
	auto vws = svr->vws();

	request req(ev_req);
	response res(ev_req);
	/*
	 * TODO: Define the request tagspace protocol with httagd-client and add
	 * authentication and permissions for target access and shared bindings.
	 * Enforce _config write protection against each operation's actual target.
	 */
	const auto method = evhtp_request_get_method(ev_req);

	{
		std::ostringstream ss;
		ss << "httagd request method="
		   << evhtp_method_str(evhtp_request_get_method(ev_req))
		   << " path=" << req.path()
		   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(ev_req->headers_in) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}

	// nullptr driver becuase circular depends httagl
	transaction tx(svr, &req, &res, tsp, nullptr, vws);

	auto ssn = tsp->get_session();

	callback CB(&tx);
	httagl tagl(tsp, &CB, &ssn);
	tagl.configure(svr->configuration(), true);
	// DELETE may change ordinary data, but only POST/PUT may create shared alias bindings.
	tagl.http_aliases(method == htp_method_POST || method == htp_method_PUT);
	tagl.http_mutations(method == htp_method_POST || method == htp_method_PUT
		|| method == htp_method_PATCH || method == htp_method_DELETE);
	tx.drvr = &tagl;

	// all sharing the internal errors pointer of tx
	tx.share_errors(tagl)
	  .share_errors(*tsp)
	  .share_errors(*vws)
	  .share_errors(ssn);

	auto parse_rc = req.parse_target(tagl);
	if (parse_rc == tagd::TAGD_OK && req.has_root_path()
		&& req.query_map().contains(TAGL::QUERY_OPT_URL)) {
		// Root handlers otherwise bypass tagdurl subject validation.
		parse_rc = tagl.error(tagd::TAGL_ERR, "_url option requires one :_url: placeholder");
	}

	// Root reads without a search are EMPTY, not incomplete tagdurl statements.
	const bool root_read = req.has_root_path()
		&& (method == htp_method_GET || method == htp_method_HEAD);
	const bool root_empty = root_read && !req.query_map().contains(TAGL::QUERY_OPT_SEARCH);
	if (parse_rc == tagd::TAGD_OK && root_empty) {
		req.method = method == htp_method_HEAD ? tagd::HTTP_HEAD : tagd::HTTP_GET;
		// EMPTY still uses the request's validated referent context.
		parse_rc = tagl.apply_tagdurl_context(req.query_contexts());
	}

	// Shared parsing determines the operation; transport policy cannot change its meaning.
	if (parse_rc == tagd::TAGD_OK && !req.has_root_path()) {
		const auto& address = req.address();
		auto rc = tagd::TAGD_OK;
		const bool read = method == htp_method_GET || method == htp_method_HEAD;
		if (address.description
			&& (method == htp_method_PUT || method == htp_method_PATCH || method == htp_method_DELETE)) {
			// TODO: Consider tagspace creation through the HTTP service in future development.
			rc = tagl.error(tagd::TAGL_ERR,
				tagd::predicate(HARD_TAG_CAUSED_BY, tagd::HARD_TAG_TAGSPACE, address.selector));
		}
		std::string operation;
		if (rc == tagd::TAGD_OK)
			rc = tagl.select_tagdurl(address, operation, read);
		if (rc == tagd::TAGD_OK) {
			req.addressed_path(operation);
			if (method == htp_method_POST || method == htp_method_PUT
				|| method == htp_method_PATCH || method == htp_method_DELETE)
				tagl.fix_http_target();
		}
		parse_rc = rc;
	}

	/*
	 * TODO: Address HTTP headers and caching systematically, including
	 * Cache-Control: no-cache for /_/... and target-aware validators. A changed
	 * target or alias must not reuse another tagspace's same-ID representation;
	 * validators alone do not prevent fresh-cache reuse.
	 */
	if (parse_rc != tagd::TAGD_OK || tagl.has_errors())
		tagl.do_callback();
	else if (root_empty)
		CB.empty();
	else
		tagl.execute(tx);

	// execute() should call finish() when at the end of buffer
	if (!res.reply_sent())
		tagl.finish();

	tagd::code tc = tx.most_severe(tx.drvr->session_ptr()->code());
	HTTAGD_LOG_REQUEST(
			tc == tagd::TAGD_OK ? tagd::log_level::INFO : tagd::log_level::ERROR,
			evhtp_request_get_method(ev_req), req.path(), ev_req->headers_in, tc);

	HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, tx);

	/*
	 * TODO tbd, server and other long running objects
	 * should not be errorable.  They should log as the errors occur
	 */
	// tsp will accumulate errors between requests, so clear
	if (tsp->has_errors()) {
		HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, *tsp);
		tsp->clear_errors();
	}
}

// open file given by path and add to buf
tagd::code response::add_file(const std::string& path, tagd::errorable* err) {
	auto f_ferror =
		[err](tagd::code tc, const char* msg, const char *arg) -> tagd::code {
			return err == nullptr ? tc : err->ferror(tc, msg, arg);
		};

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return f_ferror(tagd::TS_NOT_FOUND, "failed to open: %s", path.c_str());

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return f_ferror(tagd::TS_NOT_FOUND, "stat failed: %s", path.c_str());
	}

	if (st.st_size == 0) {
		// TODO create flag FAIL_OPEN_ZERO_LENGTH_FILE
		if (fcntl(fd, F_GETFD) != -1)
			close(fd);
		return tagd::TAGD_OK;
	}

	// evbuffer_add_file closes fd for us; ret 0 is successful, -1 not
	if (evbuffer_add_file(_ev_req->buffer_out, fd, 0, st.st_size) != 0)
		return f_ferror(tagd::TAGD_ERR, "evbuffer_add_file failed: %s", path.c_str());

	return tagd::TAGD_OK;
}

static void
file_cb(evhtp_request_t * evreq, void * arg) {
	httagd::server *svr = (httagd::server*)arg;
	request req(evreq);
	response res(evreq);
	{
		std::ostringstream ss;
		ss << "httagd request method="
		   << evhtp_method_str(evhtp_request_get_method(evreq))
		   << " path=" << req.path()
		   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(evreq->headers_in) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}
	base_transaction tx(svr, &req, &res);
	tagd::code tc;
	size_t pos;

	// req.path() in the form of: /_file/path/to/file.ext
	// shift leading "/_file" from request path - becomes file path
	pos = tagd::file::dir_shift_pos(req.path());
	assert(pos != std::string::npos);
	if (pos == std::string::npos) {
		tc = tx.ferror(tagd::TAGD_ERR, "dir_shift_pos failed: %s", req.path().c_str());
		HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, tx);
		res.send_reply(tc);
		return;
	}

	auto path = tagd::io::concat_dir(svr->configuration().www_dir(), req.path().substr(pos));

	HTTAGD_LOG_DEBUG("httagd file request path=" << req.path());
	HTTAGD_LOG_DEBUG("httagd file system path=" << path);

	// add content_type given file extension if possible
	pos = tagd::file::ext_pos(path);
	if (pos != std::string::npos) {
		auto ext = path.substr(pos);
		const char *media_type = tagd::file::ext_media_type(ext);
		if(media_type != nullptr)
			res.add_header_content_type(media_type);

		HTTAGD_LOG_DEBUG("httagd file media_type=" << media_type);
	}

	tc = res.add_file(path, &tx);

	if (tc != tagd::TAGD_OK) {
		HTTAGD_LOG_REQUEST(tagd::log_level::ERROR,
				evhtp_request_get_method(evreq), req.path(), evreq->headers_in, tc);
		HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, tx);

		// prevent fuzzing - don't print internal details
		tx.clear_errors();

		tx.ferror(tc, "request failed: %s", req.path().c_str());
		std::stringstream ss;
		tx.print_errors(ss);
		res.add(ss.str());
	}
	else
		HTTAGD_LOG_REQUEST(tagd::log_level::INFO,
				evhtp_request_get_method(evreq), req.path(), evreq->headers_in, tc);

	res.send_reply(tc);
}

// This route masks a tagspace named favicon.ico; a missing file still returns 404.
static void
favicon_cb(evhtp_request_t * evreq, void * arg) {
	httagd::server *svr = (httagd::server*)arg;

	request req(evreq);
	response res(evreq);
	{
		std::ostringstream ss;
		ss << "httagd request method="
		   << evhtp_method_str(evhtp_request_get_method(evreq))
		   << " path=" << req.path()
		   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(evreq->headers_in) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}
	base_transaction tx(svr, &req, &res);

	res.add_header_content_type("image/x-icon");
	tagd::code tc = res.add_file(svr->configuration().favicon(), &tx);
	if (tc != tagd::TAGD_OK) {
		HTTAGD_LOG_REQUEST(tagd::log_level::ERROR,
				evhtp_request_get_method(evreq), req.path(), evreq->headers_in, tc);
		HTTAGD_LOG_ERRORS(tagd::log_level::ERROR, tx);
	}

	res.send_reply(tc);
}

server::~server() {
	evhtp_free(_htp);
	event_base_free(_evbase);
}

static evhtp_res keep_request_query(evhtp_request_t* req, evhtp_headers_t*, void*) {
	/*
	 * libevhtp treats a form body as query_raw when its query list is null.
	 * An empty list prevents that fallback; libevhtp owns and frees the list.
	 * TAGL alone interprets request-target options and consumes the TAGL body.
	 */
	if (req->uri->query == nullptr)
		req->uri->query = evhtp_query_new();
	return req->uri->query == nullptr ? EVHTP_RES_FATAL : EVHTP_RES_OK;
}

static evhtp_res set_request_query_hook(evhtp_connection_t* connection, void*) {
	// Let libevhtp allocate its hook storage, then use the typed C++ callback field.
	if (evhtp_connection_set_hook(connection, evhtp_hook_on_headers, nullptr, nullptr) != 0)
		return EVHTP_RES_FATAL;
	connection->hooks->on_headers = keep_request_query;
	return EVHTP_RES_OK;
}

tagd::code server::start() {
	// libevhtp closes the connection once a body reaches its limit; allow exactly the maximum.
	evhtp_set_max_body_size(_htp, MAX_REQUEST_BODY_SIZE + 1);
	evhtp_set_post_accept_cb(_htp, set_request_query_hook, nullptr);
	// Debug hooks must be chained from set_request_query_hook; replacing it loses query isolation.
	// evhtp_set_post_accept_cb(_htp, set_my_connection_handlers, nullptr);
	evhtp_set_cb(_htp, "/_file", file_cb, this);
	evhtp_set_cb(_htp, "/favicon.ico", favicon_cb, this);
	evhtp_set_gencb(_htp, httagd::main_cb, this);

	const auto address = this->bind_addr();
	const auto port = this->bind_port();
	const char *bind_addr = (address == "localhost" ? "127.0.0.1" : address.c_str());
	if (evhtp_bind_socket(_htp, bind_addr, port, 128) < 0) {
		return this->ferror( tagd::TAGD_ERR,
				"failed to bind socket(%s): %s:%d", strerror(errno), bind_addr, port );
	}

	if (HTTAGD_LOG_ENABLED(tagd::log_level::NOTICE)) {
		std::stringstream ss;
		ss << "httagd start bind_addr=" << bind_addr
		   << " bind_port=" << port;
		HTTAGD_LOG(tagd::log_level::NOTICE, ss.str());
		HTTAGD_LOG(tagd::log_level::NOTICE,
				std::string("httagd code=").append(tagd::code_str(tagd::TAGD_OK)));
	}

	event_base_loop(_evbase, 0);

	return tagd::TAGD_OK;
}

} // namespace httagd
