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
		TAGL::driver *_driver;
		tagspace::tagspace *_TS;
		evhtp_request_t *_req;
		bool _trace_on;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				TAGL::driver *drvr,
				evhtp_request_t* req,
				bool trace = false
			)
			: _TS{ts}, _driver{drvr}, _req{req}, _trace_on{trace}
		{}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error(const TAGL::driver&);
        void finish(const TAGL::driver&);
};

class template_callback : public tagl_callback, public tagd::errorable {
		std::string _opt_tpl;
		std::string _context;

	public:
		template_callback(
				tagspace::tagspace* ts,
				TAGL::driver *drvr,
				evhtp_request_t* req,
				const std::string& opt_tpl,
				const std::string& context,
				bool trace = false
			) : tagl_callback(ts, drvr, req, trace),
			_opt_tpl{opt_tpl}, _context{context}
		{}

		void cmd_get(const tagd::abstract_tag&);
		// void cmd_put(const tagd::abstract_tag&);
		// void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error(const TAGL::driver&);
};

} // namespace httagd
