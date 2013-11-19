#pragma once

#include <evhtp.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include <ctemplate/template.h>

namespace httagd {

class tagl_callback : public TAGL::callback {
		TAGL::driver *_driver;
		tagspace::tagspace *_TS;
		evhtp_request_t *_req;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				TAGL::driver *drvr,
				evhtp_request_t* req
			)
			: _TS{ts}, _driver{drvr}, _req{req}
		{}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void error(const TAGL::driver&);
};

class template_callback : public TAGL::callback {
		TAGL::driver *_driver;
		tagspace::tagspace *_TS;
		evhtp_request_t *_req;
		std::string _tpl_file;
		std::string _context;

	public:
		template_callback(
				tagspace::tagspace* ts,
				TAGL::driver *drvr,
				evhtp_request_t* req,
				const std::string& tpl_file,
				const std::string& context
			)
			: _TS{ts}, _driver{drvr}, _req{req}, _tpl_file{tpl_file}, _context{context}
		{}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void error(const TAGL::driver&);
};


} // namespace httagd
