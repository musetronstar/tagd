#pragma once

#include <evhtp.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include "tagsh.h"
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

namespace httagd {

class httagd_args : public cmd_args {
	public:
		std::string tpl_dir;
		std::string bind_addr;
		uint16_t bind_port;

		httagd_args () : bind_port{0} {
			_cmds["--tpl-dir"] = {
				[this](char *val) {
						this->tpl_dir = val;
				}, true
			};

			_cmds["--bind-addr"] = {
				[this](char *val) {
						this->bind_addr = val;
				}, true
			};

			_cmds["--bind-port"] = {
				[this](char *val) {
						this->bind_port = static_cast<uint16_t>(
								atoi( static_cast<const char*>(val) )
							);
				}, true
			};
		}
};

void main_cb(evhtp_request_t *, void *);

class tagl_callback;

class server : public tagsh, public tagd::errorable {
	friend void main_cb(evhtp_request_t *, void *);
	friend class tagl_callback;
	friend class template_callback;

	protected:
		httagd_args *_args;
		std::string _bind_addr;
		uint16_t _bind_port;	
		evbase_t *_evbase;
		evhtp_t  *_htp;

		void init() {
			_evbase = event_base_new();
			_htp = evhtp_new(_evbase, NULL);	
		}
	public:
		server(tagspace::sqlite *ts)
			: tagsh(ts), _args{nullptr}, _bind_addr{"0.0.0.0"}, _bind_port{2112}
		{ this->init(); }

		server(tagspace::sqlite *ts, httagd_args *args)
			: tagsh(ts), _args{args}, _bind_addr{"0.0.0.0"}, _bind_port{2112} 
		{
			_bind_addr = (!args->bind_addr.empty() ? args->bind_addr : "0.0.0.0");
			_bind_port = (args->bind_port ? args->bind_port : 2112);
			this->init();
		}

		tagd::code start();
};

class request {
	friend class tagl_callback;
	friend class template_callback;

	protected:
		server *_server;
		evhtp_request_t *_ev_req;
		url_query_map_t _query_map;

	public:
		request(server* S, evhtp_request_t *req)
			: _server{S}, _ev_req{req}
		{
			if ( req->uri->query_raw != NULL ) {
				tagd::url::parse_query(
					_query_map, (char*)req->uri->query_raw );
			}
		}

		std::string query_opt(const std::string &opt) {
			std::string val;
			tagd::url::query_find(_query_map, val, opt);
			return val;
		}
};

class tagl_callback : public TAGL::callback {
	protected:
		tagspace::tagspace *_TS;
		request *_request;
		evhtp_request_t *_ev_req;
		bool _trace_on;
		std::string _content_type;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				request* req
			) : _TS{ts}, _request{req}, _ev_req{req->_ev_req},
				_content_type{"text/plain; charset=utf-8"}
			{
				_trace_on = req->_server->_args->opt_trace;
			}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
        virtual void empty();  // welcome message or home page
        void finish();
		void add_http_headers();
};

class template_callback : public tagl_callback, public tagd::errorable {
	public:
		template_callback( tagspace::tagspace* ts, request* req )
				: tagl_callback(ts, req) 
			{
				_content_type = "text/html; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		// void cmd_put(const tagd::abstract_tag&);
		// void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
		void empty();
};


} // namespace httagd
