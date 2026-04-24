#pragma once

#include "tagd.h"

#include <event2/http.h>

#include <utility>
#include <string>
#include <vector>

namespace tagd {
class logger;
}

namespace httagd {

constexpr int CLIENT_MAX_REDIRECTS = 10;

class client_request : public tagd::errorable {
	public:
		tagd::http_method method = tagd::HTTP_GET;

	private:
		std::string _url;
		int _max_redirects = CLIENT_MAX_REDIRECTS;
		std::vector<std::pair<std::string, std::string>> _request_headers;

	public:
		client_request() = default;

		explicit client_request(const std::string& url)
			: _url{url} {}

		client_request(tagd::http_method meth, const std::string& url)
			: method{meth}, _url{url} {}

		const std::string& url() const {
			return _url;
		}

		void url(const std::string& value) {
			_url = value;
		}

		int max_redirects() const {
			return _max_redirects;
		}

		void max_redirects(int n) {
			_max_redirects = n;
		}

		// caller-supplied headers appended after default Host/Connection/User-Agent
		void request_header(const std::string& key, const std::string& value) {
			_request_headers.emplace_back(key, value);
		}

		const std::vector<std::pair<std::string, std::string>>& request_headers() const {
			return _request_headers;
		}
};

class client_response : public tagd::errorable {
	private:
		using header_list = std::vector<std::pair<std::string, std::string>>;

		int _status = 0;
		header_list _headers;
		std::string _body;
		std::string _location;

	public:
		using const_header_iterator = header_list::const_iterator;

		int status() const {
			return _status;
		}

		void status(int value) {
			_status = value;
		}

		const header_list& headers() const {
			return _headers;
		}

		void header(const std::string& key, const std::string& value) {
			_headers.emplace_back(key, value);
		}

		const std::string *header_value(const std::string& key) const {
			for (const auto& header : _headers) {
				if (header.first == key)
					return &header.second;
			}

			return nullptr;
		}

		const std::string& body() const {
			return _body;
		}

		void append(const char *buf, size_t len) {
			_body.append(buf, len);
		}

		const std::string& location() const {
			return _location;
		}

		void location(const std::string& value) {
			_location = value;
		}

		void clear() {
			this->clear_errors();
			this->code(tagd::TAGD_OK);
			_status = 0;
			_headers.clear();
			_body.clear();
			_location.clear();
		}
};

class client : public tagd::errorable {
	private:
		static void request_done(evhttp_request *req, void *arg);
		tagd::code perform_once(const client_request&, client_response&);

	public:
		tagd::code perform(const client_request&, client_response&);
};

} // namespace httagd
