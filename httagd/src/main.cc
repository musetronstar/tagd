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

bool TRACE_ON = false;

class httagd_callback : public TAGL::callback {
		TAGL::driver *_driver;
		tagspace::tagspace *_TS;
		evbuffer *_output;

	public:
		httagd_callback(tagspace::tagspace*, evbuffer*);
		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void error(const TAGL::driver&);
        void driver(TAGL::driver *drvr) { _driver = drvr; }
};

httagd_callback::httagd_callback(tagspace::tagspace *ts, evbuffer *out) 
	: _TS(ts), _output(out)
{
}

void httagd_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	tagd::code ts_rc = _TS->get(T, t.id());

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		ss << T << std::endl;
	} else {
		_driver->print_errors(ss);
	}
	evbuffer_add(_output, ss.str().c_str(), ss.str().size());
}

void httagd_callback::cmd_put(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->put(t);
	if (ts_rc != tagd::TAGD_OK) {
		std::stringstream ss;
		_driver->print_errors(ss);
		evbuffer_add(_output, ss.str().c_str(), ss.str().size());
	}
}

void httagd_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	tagd::code ts_rc = _TS->query(T, q);

	std::stringstream ss;
	if (ts_rc == tagd::TAGD_OK) {
		tagd::print_tag_ids(T, ss);
		ss << std::endl;
	} else {
		_driver->print_errors(ss);
	}
	evbuffer_add(_output, ss.str().c_str(), ss.str().size());
}

void httagd_callback::error(const TAGL::driver& D) {
	std::stringstream ss;
	_driver->print_errors(ss);
	evbuffer_add(_output, ss.str().c_str(), ss.str().size());
}


/*
static evhtp_res
handle_path(evhtp_request_t *req, evhtp_path_t *path, void *arg) {
	TAGL::driver *tagl = arg;

	switch(evhtp_request_get_method(req)) {
		case htp_method_GET:
			tagl->tagdurl_get(path->full);			
			break;
		case htp_method_PUT:
			tagl->tagdurl_put(path->full);			
			break;
		default:
			evbuffer_add_printf(req->buffer_out, "Unsupported method: %d",
					evhtp_request_get_method(req));
			
	}

//    evbuffer_add_printf(req->buffer_out,
//                        "handle_path() full        = '%s'\n"
//                        "             path        = '%s'\n"
//                        "             file        = '%s'\n"
//                        "             match start = '%s'\n"
//                        "             match_end   = '%s'\n"
//                        "             methno      = '%d'\n",
//                        path->full, path->path, path->file,
//                        path->match_start, path->match_end,
//                        evhtp_request_get_method(req));

    return EVHTP_RES_OK;
}
*/

/*
static evhtp_res
handle_request_fini(evhtp_request_t *req, void *arg) {
	TAGL::driver *tagl = arg;
	tagl->finish();
	delete tagl->callback_ptr();
	delete tagl;

    return EVHTP_RES_OK;
}
*/

/*
static evhtp_res
handle_data(evhtp_request_t *req, evbuf_t *input, void *arg) {
    // size_t sz = evbuffer_get_length(input);
    // evbuffer_add_printf(req->buffer_out,
    //                     "got %zu bytes of data\n", sz);
   
    //char *data = (char *)evbuffer_pullup(input, sz);
    //evbuffer_add_printf(req->buffer_out, "%s\n", data);

	TAGL::driver *tagl = arg;
	tagl->evbuffer_execute(input);

    return EVHTP_RES_OK;
}
*/

/*
static evhtp_res
set_connection_handlers(evhtp_connection_t * conn, void *arg) {
		tagspace::sqlite *TS = arg;

		// both deleted by handle_request_fini
		httagd_callback *CB = new httagd_callback(TS);
		CB->connection(conn);
		TAGL::driver *tagl = new TAGL::driver(TS, CB);
		TAGL::driver::trace_on("trace: ");


//    evhtp_set_hook(&conn->hooks, evhtp_hook_on_header, print_kv, "foo");
//    evhtp_set_hook(&conn->hooks, evhtp_hook_on_headers, print_kvs, "bar");
	evhtp_set_hook(&conn->hooks, evhtp_hook_on_path, handle_path, (void*)tagl);
	evhtp_set_hook(&conn->hooks, evhtp_hook_on_read, handle_data, (void*)tagl);
//    evhtp_set_hook(&conn->hooks, evhtp_hook_on_new_chunk, print_new_chunk_len, NULL);
//    evhtp_set_hook(&conn->hooks, evhtp_hook_on_chunk_complete, print_chunk_complete, NULL);
//    evhtp_set_hook(&conn->hooks, evhtp_hook_on_chunks_complete, print_chunks_complete, NULL);
    //evhtp_set_hook(&conn->hooks, evhtp_hook_on_hostname, print_hostname, NULL);
    evhtp_set_hook(&conn->hooks, evhtp_hook_on_request_fini, handle_request_fini, (void*)tagl);
    //evhtp_set_hook(&conn->hooks, evhtp_hook_on_connection_fini, handle_connection_fini, (void*)tagl);

    return EVHTP_RES_OK;
}  */

static void
main_cb(evhtp_request_t *req, void *arg) {
	tagspace::sqlite *TS = (tagspace::sqlite*)arg;
	httagd_callback CB(TS, req->buffer_out);
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
	}

	if (tagl.code() == tagd::TAGD_OK) {
		evhtp_send_reply(req, EVHTP_RES_OK);
	} else {
		tagl.clear_errors();
		evhtp_send_reply(req, EVHTP_RES_SERVERR);
	}
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

int
main(int argc, char ** argv) {
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

    const char   * bind_addr   = "0.0.0.0";
    uint16_t bind_port   = 8082;

    evbase_t * evbase = event_base_new();
    evhtp_t  * htp    = evhtp_new(evbase, NULL);

    // general request handler
    evhtp_set_gencb(htp, main_cb, &TS);

    /* set a callback to set per-connection hooks (via a post_accept cb) */
    //evhtp_set_post_accept_cb(htp, set_connection_handlers, &TS);

    if (evhtp_bind_socket(htp, bind_addr, bind_port, 128) < 0) {
        fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    event_base_loop(evbase, 0);
    return 0;
}

