#include <evhtp.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace/sqlite.h"
#include "httagd.h"

bool TRACE_ON = false;

static void main_cb(evhtp_request_t *req, void *arg) {
	tagspace::sqlite *TS = (tagspace::sqlite*)arg;
	httagd::tagl_callback CB(TS, req);
	TAGL::driver tagl(TS, &CB);
	CB.driver(&tagl);

	if (TRACE_ON)
		TAGL::driver::trace_on((char *)"trace: ");

	switch(evhtp_request_get_method(req)) {
		case htp_method_GET:
			tagl.tagdurl_get(req->uri->path->full);			
			tagl.finish();
			break;
		case htp_method_PUT:
			tagl.tagdurl_put(req->uri->path->full);
			tagl.evbuffer_execute(req->buffer_in);
			tagl.finish();
			break;
		case htp_method_POST:
			// TODO check path
			tagl.evbuffer_execute(req->buffer_in);
			tagl.finish();
			break;
/*
		htp_method_HEAD,
		htp_method_DELETE,
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
			tagl.error(tagd::HTTP_ERR, "unsupported method: %d", evhtp_request_get_method(req));
			std::stringstream ss;
			tagl.print_errors(ss);
			evbuffer_add(req->buffer_out, ss.str().c_str(), ss.str().size());
			// TODO 10.4.6 405 Method Not Allowed
			// The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.
			// The response MUST include an Allow header containing a list of valid methods for the requested resource. 
			evhtp_send_reply(req, EVHTP_RES_METHNALLOWED);
	}

	if (!TS->ok())
		tagl.clear_errors();
}

// TODO put in a utility library
int error(const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	va_end (args);

	char *err = tagd::util::csprintf(errfmt, args);
	if (err != NULL)
		std::perror(err);
	return 1;
}

// TODO put in a utility library
bool file_exists(const std::string& fname) {
  struct stat buff;
  return (stat(fname.c_str(), &buff) == 0);
}

int main(int argc, char ** argv) {
	std::string db_fname;

	for(int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--db") == 0) {
			if (++i < argc) {
				if (argv[i][0] == '-')
					db_fname = ":memory:";
				else if (file_exists(argv[i]))
					db_fname = argv[i];
				else
					return error("no such file: %s", argv[i]);
			} else {
				return error("--db option requires database file");
			}
		} else if (strcmp(argv[i], "--trace") == 0) {
			TRACE_ON = true;
		} else {
			return error("unknown argument: %s", argv[i]);
		}
	}

	if (db_fname.empty())
		db_fname = tagspace::util::user_db();

	tagspace::sqlite TS;
	if ( TS.init(db_fname) != tagd::TAGD_OK ) {
		TS.print_errors();
		return 1;
	}

	if (TRACE_ON)
		TS.trace_on();

    const char   * bind_addr   = "0.0.0.0";
    uint16_t bind_port   = 8082;

    evbase_t * evbase = event_base_new();
    evhtp_t  * htp    = evhtp_new(evbase, NULL);

    // general request handler
    evhtp_set_gencb(htp, main_cb, &TS);

    if (evhtp_bind_socket(htp, bind_addr, bind_port, 128) < 0) {
        fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    event_base_loop(evbase, 0);
    return 0;
}

