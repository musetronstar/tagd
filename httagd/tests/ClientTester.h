#include <cxxtest/TestSuite.h>

#include "httagd-client.h"

#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/thread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <future>
#include <string>
#include <thread>

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))

namespace {

uint16_t unused_local_port() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	TS_ASSERT(fd >= 0);

	sockaddr_in sin{};
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = 0;

	int rc = bind(fd, reinterpret_cast<sockaddr *>(&sin), sizeof(sin));
	TS_ASSERT_EQUALS(rc, 0);

	socklen_t len = sizeof(sin);
	rc = getsockname(fd, reinterpret_cast<sockaddr *>(&sin), &len);
	TS_ASSERT_EQUALS(rc, 0);

	uint16_t port = ntohs(sin.sin_port);
	close(fd);

	return port;
}

class client_test_server {
	private:
		event_base *_base = nullptr;
		evhttp *_http = nullptr;
		evhttp_bound_socket *_bound = nullptr;
		std::thread _thread;
		uint16_t _port = 0;

		static void redirect_cb(evhttp_request *req, void *arg) {
			const char *loc = static_cast<const char *>(arg);
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc);
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		static void redirect_to_abs_cb(evhttp_request *req, void *arg) {
			auto *srv = static_cast<client_test_server *>(arg);
			std::string loc = "http://127.0.0.1:" + std::to_string(srv->port()) + "/dog";
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc.c_str());
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		static void redirect_to_b_cb(evhttp_request *req, void *arg) {
			auto *srv = static_cast<client_test_server *>(arg);
			std::string loc = "http://127.0.0.1:" + std::to_string(srv->port()) + "/redirect-b";
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc.c_str());
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		static void redirect_to_dog_cb(evhttp_request *req, void *arg) {
			auto *srv = static_cast<client_test_server *>(arg);
			std::string loc = "http://127.0.0.1:" + std::to_string(srv->port()) + "/dog";
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc.c_str());
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		static void redirect_loop_a_cb(evhttp_request *req, void *arg) {
			auto *srv = static_cast<client_test_server *>(arg);
			std::string loc = "http://127.0.0.1:" + std::to_string(srv->port()) + "/redirect-loop-b";
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc.c_str());
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		static void redirect_loop_b_cb(evhttp_request *req, void *arg) {
			auto *srv = static_cast<client_test_server *>(arg);
			std::string loc = "http://127.0.0.1:" + std::to_string(srv->port()) + "/redirect-loop-a";
			evhttp_add_header(evhttp_request_get_output_headers(req), "Location", loc.c_str());
			evhttp_send_reply(req, HTTP_MOVEPERM, "Moved Permanently", nullptr);
		}

		// Reflects each incoming request header as X-Request-<Key> so tests can
		// verify that caller-supplied headers are actually sent and arrive in order.
		static void echo_cb(evhttp_request *req, void *) {
			evkeyvalq *out = evhttp_request_get_output_headers(req);
			for (const evkeyval *h = evhttp_request_get_input_headers(req)->tqh_first;
				 h != nullptr;
				 h = h->next.tqe_next) {
				std::string key = "X-Request-";
				key += h->key;
				evhttp_add_header(out, key.c_str(), h->value);
			}
			evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
		}

		static void dog_cb(evhttp_request *req, void *) {
			evhttp_add_header(evhttp_request_get_output_headers(req), "X-Tagd-Test", "dog");

			if (evhttp_request_get_command(req) == EVHTTP_REQ_HEAD) {
				evhttp_send_reply(req, HTTP_OK, "OK", nullptr);
				return;
			}

			if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
				evhttp_send_reply(req, HTTP_BADMETHOD, "Method Not Allowed", nullptr);
				return;
			}

			evbuffer *out = evbuffer_new();
			TS_ASSERT_DIFFERS(out, nullptr);
			if (out == nullptr) {
				evhttp_send_reply(req, HTTP_SERVUNAVAIL, "Unavailable", nullptr);
				return;
			}

			evbuffer_add_printf(out, "dog kind_of mammal\n");
			evhttp_send_reply(req, HTTP_OK, "OK", out);
			evbuffer_free(out);
		}

		void init_port() {
			int fd = evhttp_bound_socket_get_fd(_bound);
			TS_ASSERT(fd >= 0);

			sockaddr_in sin;
			socklen_t len = sizeof(sin);
			int rc = getsockname(fd, reinterpret_cast<sockaddr *>(&sin), &len);
			TS_ASSERT_EQUALS(rc, 0);

			_port = ntohs(sin.sin_port);
		}

	public:
		client_test_server() {
			evthread_use_pthreads();

			_base = event_base_new();
			TS_ASSERT_DIFFERS(_base, nullptr);

			_http = evhttp_new(_base);
			TS_ASSERT_DIFFERS(_http, nullptr);

			evhttp_set_cb(_http, "/dog", dog_cb, nullptr);
			evhttp_set_cb(_http, "/echo", echo_cb, nullptr);
			_bound = evhttp_bind_socket_with_handle(_http, "127.0.0.1", 0);
			TS_ASSERT_DIFFERS(_bound, nullptr);

			init_port();

			evhttp_set_cb(_http, "/redirect-to-dog", redirect_to_abs_cb, this);
			evhttp_set_cb(_http, "/redirect-a", redirect_to_b_cb, this);
			evhttp_set_cb(_http, "/redirect-b", redirect_to_dog_cb, this);
			evhttp_set_cb(_http, "/redirect-loop-a", redirect_loop_a_cb, this);
			evhttp_set_cb(_http, "/redirect-loop-b", redirect_loop_b_cb, this);

			std::promise<void> started;
			std::future<void> ready = started.get_future();
			_thread = std::thread([this, started = std::move(started)]() mutable {
				started.set_value();
				event_base_dispatch(_base);
			});
			ready.get();
		}

		~client_test_server() {
			if (_base != nullptr)
				event_base_loopexit(_base, nullptr);

			if (_thread.joinable())
				_thread.join();

			if (_http != nullptr)
				evhttp_free(_http);

			if (_base != nullptr)
				event_base_free(_base);
		}

		uint16_t port() const { return _port; }

		std::string url(const std::string& path) const {
			return "http://127.0.0.1:" + std::to_string(_port) + path;
		}

		std::string url_https(const std::string& path) const {
			return "https://127.0.0.1:" + std::to_string(_port) + path;
		}
};

} // namespace

class ClientTester : public CxxTest::TestSuite {
	public:
		void test_perform_get_returns_status_and_body() {
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(server.url("/dog"));
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(res.code()), "TAGD_OK")
			TS_ASSERT_EQUALS(res.status(), HTTP_OK)
			TS_ASSERT(!res.headers().empty())
			TS_ASSERT_DIFFERS(res.header_value("Date"), nullptr)
			TS_ASSERT_DIFFERS(res.header_value("Content-Length"), nullptr)
			TS_ASSERT_DIFFERS(res.header_value("Content-Type"), nullptr)
			TS_ASSERT_DIFFERS(res.header_value("X-Tagd-Test"), nullptr)
			TS_ASSERT_EQUALS(*res.header_value("X-Tagd-Test"), "dog")
			TS_ASSERT_EQUALS(res.body(), "dog kind_of mammal\n")
		}

		void test_perform_head_returns_status_and_headers_without_body() {
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(tagd::HTTP_HEAD, server.url("/dog"));
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(res.code()), "TAGD_OK")
			TS_ASSERT_EQUALS(res.status(), HTTP_OK)
			TS_ASSERT(res.body().empty())
			TS_ASSERT_DIFFERS(res.header_value("Date"), nullptr)
			TS_ASSERT_DIFFERS(res.header_value("X-Tagd-Test"), nullptr)
			TS_ASSERT_EQUALS(*res.header_value("X-Tagd-Test"), "dog")
		}

		void test_perform_https_tls_error_is_deterministic() {
			// Connecting to a plain HTTP server via https must fail at TLS handshake,
			// not with "scheme not supported".
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(tagd::HTTP_GET, server.url_https("/dog"));
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "HTTP_ERR")
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(res.code()), "HTTP_ERR")
			TS_ASSERT(cli.has_errors())
			TS_ASSERT(res.has_errors())
			TS_ASSERT_EQUALS(res.status(), 0)
			TS_ASSERT(res.body().empty())
			TS_ASSERT_DIFFERS(cli.last_error().message().find("httagd client request failed:"),
				std::string::npos)
			TS_ASSERT_EQUALS(cli.last_error().message(), res.last_error().message())
		}

		void test_perform_connect_failure_is_deterministic() {
			const uint16_t port = unused_local_port();

			httagd::client cli;
			httagd::client_request req(
				tagd::HTTP_GET,
				std::string("http://127.0.0.1:") + std::to_string(port) + "/dog");
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "HTTP_ERR")
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(res.code()), "HTTP_ERR")
			TS_ASSERT(cli.has_errors())
			TS_ASSERT(res.has_errors())
			TS_ASSERT_EQUALS(res.status(), 0)
			TS_ASSERT(res.body().empty())
			TS_ASSERT(!cli.last_error().message().empty())
			TS_ASSERT_EQUALS(cli.last_error().message(), res.last_error().message())
			TS_ASSERT_DIFFERS(cli.last_error().message().find("httagd client request failed:"),
				std::string::npos)
		}

		void test_perform_redirect_is_followed() {
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(server.url("/redirect-to-dog"));
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
			TS_ASSERT_EQUALS(TAGD_CODE_STRING(res.code()), "TAGD_OK")
			TS_ASSERT_EQUALS(res.status(), HTTP_OK)
			TS_ASSERT_EQUALS(res.body(), "dog kind_of mammal\n")
		}

		void test_perform_max_redirects_exceeded_is_deterministic() {
			// max_redirects=1: first hop (a→b) is followed, second hop (b→dog) fails
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(server.url("/redirect-a"));
			req.max_redirects(1);
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "HTTP_ERR")
			TS_ASSERT(cli.has_errors())
			TS_ASSERT(res.has_errors())
			TS_ASSERT_DIFFERS(cli.last_error().message().find("max redirects"),
				std::string::npos)
		}

		void test_perform_get_sends_caller_request_headers_in_order() {
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(server.url("/echo"));
			req.request_header("X-Test", "dog");
			req.request_header("X-Order", "first");
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK")
			TS_ASSERT_EQUALS(res.status(), HTTP_OK)

			TS_ASSERT_DIFFERS(res.header_value("X-Request-X-Test"), nullptr)
			TS_ASSERT_EQUALS(*res.header_value("X-Request-X-Test"), "dog")
			TS_ASSERT_DIFFERS(res.header_value("X-Request-X-Order"), nullptr)
			TS_ASSERT_EQUALS(*res.header_value("X-Request-X-Order"), "first")

			// headers must arrive in insertion order
			const auto& hdrs = res.headers();
			auto it_test = std::find_if(hdrs.begin(), hdrs.end(),
				[](const auto& h){ return h.first == "X-Request-X-Test"; });
			auto it_order = std::find_if(hdrs.begin(), hdrs.end(),
				[](const auto& h){ return h.first == "X-Request-X-Order"; });
			TS_ASSERT_DIFFERS(it_test, hdrs.end())
			TS_ASSERT_DIFFERS(it_order, hdrs.end())
			TS_ASSERT(it_test < it_order)
		}

		void test_perform_redirect_loop_is_detected() {
			client_test_server server;
			httagd::client cli;
			httagd::client_request req(server.url("/redirect-loop-a"));
			httagd::client_response res;

			tagd::code tc = cli.perform(req, res);

			TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "HTTP_ERR")
			TS_ASSERT(cli.has_errors())
			TS_ASSERT(res.has_errors())
			TS_ASSERT_DIFFERS(cli.last_error().message().find("redirect loop"),
				std::string::npos)
		}
};
