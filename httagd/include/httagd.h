#pragma once

#include <evhtp.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

namespace httagd {

class tagl_callback : public TAGL::callback {
	protected:
		tagspace::tagspace *_TS;
		evhtp_request_t *_req;
		bool _trace_on;
		std::string _content_type;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				evhtp_request_t* req,
				bool trace = false
			)
			: _TS{ts}, _req{req}, _trace_on{trace},
			_content_type{"text/plain; charset=utf-8"} {}

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
		std::string _opt_tpl;
		std::string _context;

	public:
		template_callback(
				tagspace::tagspace* ts,
				evhtp_request_t* req,
				const std::string& opt_tpl,
				const std::string& context,
				bool trace = false
			) : tagl_callback(ts, req, trace),
			_opt_tpl{opt_tpl}, _context{context} {
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
