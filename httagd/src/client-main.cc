#include <iostream>
#include <utility>
#include <vector>

#include "httagd.h"
#include "httagd-client.h"

namespace {

const char CLIENT_VERBOSE_OPT[] = "-v";
const char CLIENT_METHOD_OPT[] = "-m";
const char CLIENT_METHOD_LONG_OPT[] = "--method";
const char CLIENT_HEADER_OPT[] = "-H";
const char CLIENT_METHOD_GET[] = "GET";
const char CLIENT_METHOD_HEAD[] = "HEAD";

int usage() {
	std::cerr
		<< "httagd-client" << std::endl
		<< "httagd-client [-v] [-m|--method GET|HEAD] [-H 'Key: Value']... <http-url>" << std::endl
		<< "----------------------------------------------------" << std::endl
		<< "  print the response body for one HTTP request (GET by default)" << std::endl;

	return tagd::TAGD_ERR;
}

bool parse_method(const std::string& value, tagd::http_method& method) {
	if (value == CLIENT_METHOD_GET) {
		method = tagd::HTTP_GET;
		return true;
	}

	if (value == CLIENT_METHOD_HEAD) {
		method = tagd::HTTP_HEAD;
		return true;
	}

	return false;
}

using header_pair = std::pair<std::string, std::string>;

bool parse_args(
	int argc, char **argv,
	bool& verbose,
	tagd::http_method& method,
	std::vector<header_pair>& request_headers,
	const char *&url)
{
	verbose = false;
	method = tagd::HTTP_GET;
	url = nullptr;

	int argi = 1;
	while (argi < argc) {
		const std::string arg = argv[argi];
		if (arg == CLIENT_VERBOSE_OPT) {
			verbose = true;
			++argi;
			continue;
		}

		if (arg == CLIENT_METHOD_OPT || arg == CLIENT_METHOD_LONG_OPT) {
			++argi;
			if (argi >= argc)
				return false;

			if (!parse_method(argv[argi], method))
				return false;

			++argi;
			continue;
		}

		if (arg == CLIENT_HEADER_OPT) {
			++argi;
			if (argi >= argc)
				return false;

			const std::string hdr = argv[argi];
			const auto sep = hdr.find(": ");
			if (sep == std::string::npos)
				return false;

			request_headers.emplace_back(hdr.substr(0, sep), hdr.substr(sep + 2));
			++argi;
			continue;
		}

		break;
	}

	if (argc != argi + 1)
		return false;

	url = argv[argi];
	return true;
}

} // namespace

int main(int argc, char **argv) {
	tagd::tagspace_install_logger_validator();

	bool verbose = false;
	tagd::http_method method = tagd::HTTP_GET;
	std::vector<header_pair> request_headers;
	const char *url = nullptr;
	if (!parse_args(argc, argv, verbose, method, request_headers, url))
		return usage();

	tagd::logger log(std::cerr);
	if (verbose) {
		log.level(tagd::log_level::EMERGENCY);
		log.level(std::string(HARD_TAG_ROLE_HTTAGD), tagd::log_level::DEBUG);
		HTTAGD_SET_LOGGER(&log);
	}

	httagd::client cli;
	httagd::client_request req(method, url);
	for (const auto& [key, val] : request_headers)
		req.request_header(key, val);
	httagd::client_response res;

	tagd::code tc = cli.perform(req, res);
	if (tc != tagd::TAGD_OK) {
		cli.print_errors();
		return tc;
	}

	std::cout << res.body();
	return tagd::TAGD_OK;
}
