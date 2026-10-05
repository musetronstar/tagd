#include "httagd-client.h"
#include "httagd.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/util.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <cstring>
#include <set>
#include <sstream>

namespace httagd {

namespace {

constexpr int CLIENT_RESPONSE_CODE_NONE = 0;
constexpr uint16_t CLIENT_DEFAULT_HTTP_PORT = 80;
constexpr uint16_t CLIENT_DEFAULT_HTTPS_PORT = 443;
constexpr size_t CLIENT_READ_BUFFER_SIZE = 4096;
const char HTTP_REQUEST_VERSION[] = "HTTP/1.1";
const char HTTP_HEADER_HOST[] = "Host";
const char HTTP_HEADER_CONNECTION[] = "Connection";
const char HTTP_HEADER_CONNECTION_CLOSE[] = "close";
const char HTTP_HEADER_USER_AGENT[] = "User-Agent";
const char HTTP_HEADER_USER_AGENT_VALUE[] = "tagd user agent";

struct client_transaction {
	event_base *base = nullptr;
	evhttp_connection *conn = nullptr;
	bufferevent *bev = nullptr;
	client *cli = nullptr;
	client_response *res = nullptr;
	bool done = false;
	std::string tls_version;
	std::string tls_cipher;
};

tagd::code fail(
	client& cli,
	client_response& res,
	tagd::code tc,
	const char *fmt,
	...)
{
	va_list args;
	va_start(args, fmt);
	cli.verror(tc, fmt, args);
	va_end(args);

	res.copy_errors(cli);
	return res.code(cli.code());
}

evhttp_cmd_type method_evhttp_cmd(tagd::http_method method) {
	switch (method) {
		case tagd::HTTP_GET:
			return EVHTTP_REQ_GET;
		case tagd::HTTP_HEAD:
			return EVHTTP_REQ_HEAD;
		case tagd::HTTP_POST:
			return EVHTTP_REQ_POST;
		case tagd::HTTP_PUT:
			return EVHTTP_REQ_PUT;
		case tagd::HTTP_DELETE:
			return EVHTTP_REQ_DELETE;
		default:
			return EVHTTP_REQ_GET;
	}
}

const char *http_method_str(tagd::http_method method) {
	switch (method) {
		case tagd::HTTP_GET:    return "GET";
		case tagd::HTTP_HEAD:   return "HEAD";
		case tagd::HTTP_POST:   return "POST";
		case tagd::HTTP_PUT:    return "PUT";
		case tagd::HTTP_DELETE: return "DELETE";
		case tagd::HTTP_UNKNOWN:return "UNKNOWN";
	}

	return "UNKNOWN";
}

std::string host_header_value(const char *host, ev_uint16_t port, ev_uint16_t default_port) {
	std::string value(host);
	if (port != default_port) {
		value.push_back(':');
		value.append(std::to_string(port));
	}

	return value;
}

// Captures TLS version and cipher at handshake completion, before connection teardown.
// arg is client_transaction* stored via SSL_set_ex_data.
void tls_info_cb(const SSL *ssl, int where, int) {
	if (!(where & SSL_CB_HANDSHAKE_DONE)) return;
	auto *tx = static_cast<client_transaction *>(
		SSL_get_ex_data(ssl, 0));
	if (tx == nullptr) return;
	tx->tls_version = SSL_get_version(ssl);
	const SSL_CIPHER *c = SSL_get_current_cipher(ssl);
	if (c != nullptr) tx->tls_cipher = SSL_CIPHER_get_name(c);
}

} // namespace

void client::request_done(evhttp_request *req, void *arg) {
	client_transaction *tx = static_cast<client_transaction *>(arg);
	tx->done = true;

	// libevent reports transport-level failure through the callback without
	// a real HTTP response code, so treat that as deterministic HTTP_ERR.
	if (req == nullptr || evhttp_request_get_response_code(req) == CLIENT_RESPONSE_CODE_NONE) {
		std::string errmsg;
		if (tx->bev != nullptr) {
			char buf[256];
			unsigned long oslerr;
			while ((oslerr = bufferevent_get_openssl_error(tx->bev)) != 0) {
				ERR_error_string_n(oslerr, buf, sizeof(buf));
				if (!errmsg.empty()) errmsg += ": ";
				errmsg += buf;
			}
		}
		if (errmsg.empty())
			errmsg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
		HTTAGD_LOG(tagd::log_level::EMERGENCY, "httagd client request failed: " + errmsg);
		tx->cli->ferror(tagd::HTTP_ERR, "httagd client request failed: %s", errmsg.c_str());
		tx->res->copy_errors(*tx->cli).code(tx->cli->code());
		event_base_loopexit(tx->base, nullptr);
		return;
	}

	{
		std::ostringstream ss;
		ss << "httagd client response status="
		   << evhttp_request_get_response_code(req)
		   << " reason=" << evhttp_request_get_response_code_line(req);
		if (!tx->tls_version.empty()) {
			ss << " tls=" << tx->tls_version;
			if (!tx->tls_cipher.empty())
				ss << " cipher=" << tx->tls_cipher;
		}
		ss << " headers=[" << HTTAGD_HTTP_HEADERS_STR(evhttp_request_get_input_headers(req)) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}

	tx->res->status(evhttp_request_get_response_code(req));

	for (const evkeyval *hdr = evhttp_request_get_input_headers(req)->tqh_first;
		 hdr != nullptr;
		 hdr = hdr->next.tqe_next) {
		tx->res->header(hdr->key, hdr->value);
	}

	const char *loc = evhttp_find_header(evhttp_request_get_input_headers(req), "Location");
	if (loc != nullptr)
		tx->res->location(loc);

	// Drain the libevent input buffer so the response body stays contiguous.
	char buf[CLIENT_READ_BUFFER_SIZE];
	int nread = 0;
	while ((nread = evbuffer_remove(evhttp_request_get_input_buffer(req), buf, sizeof(buf))) > 0) {
		tx->res->append(buf, static_cast<size_t>(nread));
	}

	tx->res->code(tagd::TAGD_OK);
	event_base_loopexit(tx->base, nullptr);
}

tagd::code client::perform_once(const client_request& req, client_response& res) {
	this->clear_errors();
	res.clear();

	evhttp_uri *uri = nullptr;
	event_base *base = nullptr;
	evhttp_connection *conn = nullptr;
	evhttp_request *ev_req = nullptr;
	SSL_CTX *ssl_ctx = nullptr;
	SSL *ssl = nullptr;
	bufferevent *bev = nullptr;
	client_transaction tx;
	const char *host = nullptr;
	const char *path = nullptr;
	const char *query = nullptr;
	struct evkeyvalq *output_headers = nullptr;
	std::string request_uri;
	ev_uint16_t port = CLIENT_DEFAULT_HTTP_PORT;
	ev_uint16_t default_port = CLIENT_DEFAULT_HTTP_PORT;
	bool is_https = false;

	tagd::code tc = tagd::TAGD_OK;

	if (req.method == tagd::HTTP_UNKNOWN) {
		return fail(*this, res, tagd::HTTP_ERR, "httagd client unsupported method");
	}

	uri = evhttp_uri_parse(req.url().c_str());
	if (uri == nullptr) {
		return fail(*this, res, tagd::TAGD_ERR, "httagd client parse url failed: %s", req.url().c_str());
	}

	{
		const char *scheme = evhttp_uri_get_scheme(uri);
		if (scheme == nullptr) {
			tc = fail(*this, res, tagd::TAGD_ERR, "httagd client missing scheme: %s", req.url().c_str());
			goto done;
		}
		if (std::strcmp(scheme, "https") == 0) {
			is_https = true;
			default_port = CLIENT_DEFAULT_HTTPS_PORT;
		} else if (std::strcmp(scheme, "http") != 0) {
			tc = fail(*this, res, tagd::HTTP_ERR,
				"httagd client only supports http and https urls: %s", req.url().c_str());
			goto done;
		}
	}

	host = evhttp_uri_get_host(uri);
	if (host == nullptr || *host == '\0') {
		tc = fail(*this, res, tagd::TAGD_ERR, "httagd client host required: %s", req.url().c_str());
		goto done;
	}

	{
		int p = evhttp_uri_get_port(uri);
		port = static_cast<ev_uint16_t>(p == -1 ? default_port : p);
	}

	base = event_base_new();
	if (base == nullptr) {
		tc = fail(*this, res, tagd::TS_ERR, "httagd client event_base_new failed");
		goto done;
	}

	if (is_https) {
		{
			std::ostringstream ss;
			ss << "httagd client https host=" << host << " port=" << port << " verify=peer";
			HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
		}

		ssl_ctx = SSL_CTX_new(TLS_client_method());
		if (ssl_ctx == nullptr) {
			HTTAGD_LOG(tagd::log_level::EMERGENCY, "httagd client SSL_CTX_new failed");
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client SSL_CTX_new failed");
			goto done;
		}
		if (X509_STORE_set_default_paths(SSL_CTX_get_cert_store(ssl_ctx)) != 1) {
			HTTAGD_LOG(tagd::log_level::EMERGENCY, "httagd client ssl cert store load failed");
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client ssl cert store load failed");
			goto done;
		}
		SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);

		ssl = SSL_new(ssl_ctx);
		if (ssl == nullptr) {
			HTTAGD_LOG(tagd::log_level::EMERGENCY, "httagd client SSL_new failed");
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client SSL_new failed");
			goto done;
		}
		SSL_set_tlsext_host_name(ssl, host);
		SSL_set1_host(ssl, host);
		SSL_set_info_callback(ssl, tls_info_cb);
		SSL_set_ex_data(ssl, 0, &tx);

		bev = bufferevent_openssl_socket_new(base, -1, ssl,
			BUFFEREVENT_SSL_CONNECTING,
			BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
		if (bev == nullptr) {
			SSL_free(ssl);
			ssl = nullptr;
			HTTAGD_LOG(tagd::log_level::EMERGENCY, "httagd client bufferevent create failed");
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client bufferevent create failed");
			goto done;
		}
		bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);

		conn = evhttp_connection_base_bufferevent_new(base, nullptr, bev, host, port);
		if (conn == nullptr) {
			bufferevent_free(bev);
			bev = nullptr;
			HTTAGD_LOG(tagd::log_level::EMERGENCY,
				"httagd client connect failed: " + req.url());
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client connect failed: %s", req.url().c_str());
			goto done;
		}
	} else {
		conn = evhttp_connection_base_new(base, nullptr, host, port);
		if (conn == nullptr) {
			tc = fail(*this, res, tagd::HTTP_ERR, "httagd client connect failed: %s", req.url().c_str());
			goto done;
		}
	}

	tx.base = base;
	tx.conn = conn;
	tx.bev = bev;
	tx.cli = this;
	tx.res = &res;

	ev_req = evhttp_request_new(client::request_done, &tx);
	if (ev_req == nullptr) {
		tc = fail(*this, res, tagd::TS_ERR, "httagd client request allocation failed");
		goto done;
	}

	path = evhttp_uri_get_path(uri);
	query = evhttp_uri_get_query(uri);
	request_uri = (path == nullptr || *path == '\0') ? "/" : path;
	if (query != nullptr && *query != '\0') {
		request_uri.push_back('?');
		request_uri.append(query);
	}

	output_headers = evhttp_request_get_output_headers(ev_req);
	if (evhttp_add_header(output_headers, HTTP_HEADER_HOST, host_header_value(host, port, default_port).c_str()) != 0) {
		tc = fail(*this, res, tagd::TS_ERR, "httagd client add header failed: %s", HTTP_HEADER_HOST);
		evhttp_request_free(ev_req);
		ev_req = nullptr;
		goto done;
	}
	if (evhttp_add_header(output_headers, HTTP_HEADER_CONNECTION, HTTP_HEADER_CONNECTION_CLOSE) != 0) {
		tc = fail(*this, res, tagd::TS_ERR, "httagd client add header failed: %s", HTTP_HEADER_CONNECTION);
		evhttp_request_free(ev_req);
		ev_req = nullptr;
		goto done;
	}
	if (evhttp_add_header(output_headers, HTTP_HEADER_USER_AGENT, HTTP_HEADER_USER_AGENT_VALUE) != 0) {
		tc = fail(*this, res, tagd::TS_ERR, "httagd client add header failed: %s", HTTP_HEADER_USER_AGENT);
		evhttp_request_free(ev_req);
		ev_req = nullptr;
		goto done;
	}

	for (const auto& [key, val] : req.request_headers()) {
		if (evhttp_add_header(output_headers, key.c_str(), val.c_str()) != 0) {
			tc = fail(*this, res, tagd::TS_ERR, "httagd client add header failed: %s", key.c_str());
			evhttp_request_free(ev_req);
			ev_req = nullptr;
			goto done;
		}
	}

	{
		std::ostringstream ss;
		ss << "httagd client request method=" << http_method_str(req.method)
		   << " uri=" << request_uri
		   << ' ' << HTTP_REQUEST_VERSION
		   << " headers=[" << HTTAGD_HTTP_HEADERS_STR(output_headers) << ']';
		HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
	}

	if (evhttp_make_request(conn, ev_req, method_evhttp_cmd(req.method), request_uri.c_str()) != 0) {
		tc = fail(*this, res, tagd::HTTP_ERR, "httagd client make request failed: %s", req.url().c_str());
		evhttp_request_free(ev_req);
		ev_req = nullptr;
		goto done;
	}

	if (event_base_dispatch(base) == -1) {
		tc = fail(*this, res, tagd::HTTP_ERR, "httagd client dispatch failed: %s", req.url().c_str());
		goto done;
	}

	if (!tx.done) {
		tc = fail(*this, res, tagd::HTTP_ERR, "httagd client request incomplete: %s", req.url().c_str());
		goto done;
	}

	tc = res.code();

done:
	if (conn != nullptr)
		evhttp_connection_free(conn);
	else if (bev != nullptr)
		bufferevent_free(bev);
	else if (ssl != nullptr)
		SSL_free(ssl);

	if (ssl_ctx != nullptr)
		SSL_CTX_free(ssl_ctx);

	if (base != nullptr)
		event_base_free(base);

	if (uri != nullptr)
		evhttp_uri_free(uri);

	return tc;
}

tagd::code client::perform(const client_request& req, client_response& res) {
	this->clear_errors();

	client_request current = req;
	std::set<std::string> seen;
	seen.insert(req.url());

	for (int hops = 0; ; ++hops) {
		tagd::code tc = this->perform_once(current, res);
		if (tc != tagd::TAGD_OK) return tc;

		int status = res.status();
		if (status < 300 || status >= 400) return res.code();

		const std::string loc = res.location();
		if (loc.empty()) return res.code();

		if (hops >= req.max_redirects()) {
			return fail(*this, res, tagd::HTTP_ERR,
				"httagd client max redirects (%d) exceeded: %s",
				req.max_redirects(), current.url().c_str());
		}

		if (seen.count(loc)) {
			return fail(*this, res, tagd::HTTP_ERR,
				"httagd client redirect loop detected: %s", loc.c_str());
		}
		seen.insert(loc);

		{
			std::ostringstream ss;
			ss << "httagd client redirect status=" << status << " location=" << loc;
			HTTAGD_LOG(tagd::log_level::DEBUG, ss.str());
		}

		res.clear();
		current.url(loc);
	}
}

} // namespace httagd
