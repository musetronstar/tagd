#pragma once

#include <evhtp.h>

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"

namespace httagd {

class tagl_callback : public TAGL::callback {
		TAGL::driver *_driver;
		tagspace::tagspace *_TS;
		evhtp_request_t *_req;

	public:
		tagl_callback(tagspace::tagspace* ts, evhtp_request_t* req)
			: _driver{nullptr}, _TS{ts}, _req{req}
		{}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void error(const TAGL::driver&);
        void driver(TAGL::driver *drvr) { _driver = drvr; }
};

} // namespace httagd
