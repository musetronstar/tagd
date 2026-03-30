#pragma once

namespace tagd {

typedef enum {
	HTTP_UNKNOWN = 0,
	HTTP_GET,
	HTTP_HEAD,
	HTTP_POST,
	HTTP_PUT,
	HTTP_DELETE
} http_method;

} // namespace tagd
