#pragma once

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include "tagsh.h"
#include "views.h"

#include <cstring>
#include <iostream>
#include <map>
#include <vector>
#include <evhtp.h>
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

void HTTAGD_SET_LOGGER(tagd::logger *);
bool HTTAGD_LOG_ENABLED(tagd::log_level);
void HTTAGD_LOG(tagd::log_level, const std::string&);
std::string HTTAGD_HTTP_HEADERS_STR(const evhtp_headers_t *);
std::string HTTAGD_HTTP_HEADERS_STR(const struct evkeyvalq *);

namespace httagd {

evhtp_res tagd_code_evhtp_res(tagd::code);

class httagd_args : public cmd_args {
public:
	httagd_args() {
		config_option("--tpl-dir", tagd::HARD_TAG_CONFIG_TPL_DIR);
		config_option("--www-dir", tagd::HARD_TAG_CONFIG_WWW_DIR);
		config_option("--default-view", tagd::HARD_TAG_CONFIG_DEFAULT_VIEW);
		config_option("--bind-addr", tagd::HARD_TAG_CONFIG_BIND_ADDR);
		config_option("--bind-port", tagd::HARD_TAG_CONFIG_BIND_PORT);
		_cmds["--trace"] = {[this](char*) {
			this->error(tagd::TAGD_ERR,
				"httagd: --trace has been retired; use --log-level with _role:httagd:debug");
		}, false};
		cmd_handler help_handler = {
			[this](char *) {
				std::cout
				<< "httagd" 																<< std::endl
				<< "httagd [options]" 													<< std::endl
				<< "---------------" 													<< std::endl
				<< "  --config FILE    alternate TAGL configuration file" << std::endl
				<< "  --config-id ID   canonical configuration profile override" << std::endl
				<< "  --home DIR       tagd home (default ~/.tagd)" << std::endl
				<< "  --tagspace ID    select ID/alias, overriding profile _use (fallback: _default)" << std::endl
				<< "  --db PATH        direct storage file or file URL; disables home lookup" << std::endl
				<< "  --create         create database/tagspace; fail if it exists" << std::endl
				<< "  -f <tagl file>" 													<< std::endl
				<< "  --file" 															<< std::endl
				<< "		execute tagl file, multiple flags will be processed in order" 	<< std::endl
				<< "  -n" 																<< std::endl
				<< "  --noshell" 														<< std::endl
				<< "		execute then exit with no shell" 							<< std::endl
				<< "  --dump" 															<< std::endl
				<< "		dump tagspace and exit" 												<< std::endl
				<< "  --log-level <level>"												<< std::endl
				<< "		set log level or role overrides, e.g. warning,_role:httagd:debug" << std::endl
				<< "  --tpl-dir <dir>" 													<< std::endl
				<< "  --www-dir <dir>" 													<< std::endl
				<< "  --default-view <view>" 											<< std::endl
				<< "  --bind-addr <addr>" 												<< std::endl
				<< "  --bind-port <port>" 												<< std::endl
				<< std::endl;
				std::exit(EXIT_SUCCESS);
			}, false
		};
		_cmds["--help"] = help_handler;
		_cmds["-h"] = help_handler;
	}
};

class server : public tagsh, public tagd::errorable {
	protected:
		viewspace *_vws;     // borrowed

		evbase_t *_evbase;  // owned here; freed in ~server()
		evhtp_t  *_htp;     // owned here; freed in ~server()

		void init() {
			_evbase = event_base_new();
			_htp = evhtp_new(_evbase, NULL);
		}
	public:
		~server();

		server(tagd::tagspace *tsp, viewspace *vs, tagd::config& cfg)
			: tagsh(tsp, cfg), _vws{vs}
		{
			this->init();
		}

		std::string bind_addr() const {
			return _config.bind_addr();
		}

		uint16_t bind_port() const {
			return _config.bind_port();
		}

		tagd::tagspace* tsp() {
			return _driver.tsp();
		}

		viewspace* vws() {
			return _vws;
		}

		tagd::config& configuration() {
			return _config;
		}

		tagd::code start();
};

class base_transaction;
class transaction;

static const std::string DEFAULT_CONTENT_TYPE{"text/plain; charset=utf-8"};

class response {
	protected:
		evhtp_request_t *_ev_req;
		/*
		 * res_code sent
		 * when set to >= 0 before sending,
		 * send this code instead of translated tagd::code to EVHTP_RES_*
		 */
		int _res_code = -1;
		bool _reply_sent = false;
		bool _header_content_type_added = false;

	public:
		response() = delete;

		response(evhtp_request_t *req) : _ev_req{req} {}

		bool reply_sent() const {
			return _reply_sent;
		}

		bool header_content_type_added() const {
			return _header_content_type_added;
		}

		int res_code() const {
			return _res_code;
		}

		void res_code(int c) {
			if (!_reply_sent)
				_res_code = c;
		}

		void add(const std::string& s) {
			evbuffer_add(_ev_req->buffer_out, s.c_str(), s.size());
		}

		void add(const char* s, size_t sz) {
			evbuffer_add(_ev_req->buffer_out, s, sz);
		}

		evbuffer* output_buffer() {
			return _ev_req->buffer_out;
		}

		void send_reply(tagd::code c);
		void send_ev_reply(evhtp_res);

		void add_header(const std::string &key, const std::string &val);

		void add_header_content_type(const std::string& content_type) {
			this->add_header("Content-Type", content_type);
			_header_content_type_added = true;
		}
		void add_header_content_length(size_t sz) {
			this->add_header("Content-Length", std::to_string(sz));
		}

		tagd::code add_file(const std::string& path, tagd::errorable *err=nullptr);

		void add_error_str(const tagd::errorable &err) {
			std::stringstream ss;
			err.print_errors(ss);
			this->add(ss.str());
			// callback::finish() will send the error response
		}

};

class request {
	public:
		tagd::http_method method = tagd::HTTP_UNKNOWN;

	protected:
		evhtp_request_t *_ev_req = nullptr;
		std::string _path;
		std::optional<std::string> _operation_path;
		std::string _tagspace_selector{"_"};
		url_query_map_t _query_map;

	public:
		request(evhtp_request_t *ev_req)
			: _ev_req{ev_req},
			_path{_ev_req->uri->path->full}
		{
			if ( ev_req->uri->query_raw != NULL ) {
				tagd::url::parse_query(
					_query_map, (char*)ev_req->uri->query_raw );
			}
		}

		request(tagd::http_method meth, const std::string path)
			: method{meth}, _path{path} {}  // for Tester

		std::string query_opt(const std::string &opt) const {
			std::string val;
			tagd::url::query_find(_query_map, val, opt);
			return val;
		}

		std::string query_opt_search() const {
			return this->query_opt(TAGL::QUERY_OPT_SEARCH);
		}

		std::string query_opt_view() const {
			return this->query_opt(TAGL::QUERY_OPT_VIEW);
		}

		std::string query_opt_context() const {
			return this->query_opt(TAGL::QUERY_OPT_CONTEXT);
		}

		const url_query_map_t &query_map() const {
			return _query_map;
		}

		const std::string& path() const {
			return _path;
		}

		void addressed_path(const std::string& operation, const std::string& selector) {
			_operation_path = operation.substr(0, operation.find('?'));
			_tagspace_selector = selector;
		}
		const std::string& tagspace_selector() const { return _tagspace_selector; }
		const std::string& operation_path() const {
			return _operation_path ? *_operation_path : _path;
		}
		bool has_root_path() const {
			return operation_path().empty() || operation_path() == "/";
		}

		std::string tagdurl() const {
			std::string url = operation_path();
			std::string opt_search = this->query_opt_search();
			if (!opt_search.empty()) {
				url.push_back(url.find('?') == std::string::npos ? '?' : '&');
				url.append(TAGL::QUERY_OPT_SEARCH);
				url.push_back('=');
				url.append(tagd::uri_encode(opt_search));
			}

			std::string opt_context = this->query_opt_context();
			if (!opt_context.empty()) {
				url.push_back(url.find('?') == std::string::npos ? '?' : '&');
				url.append(TAGL::QUERY_OPT_CONTEXT);
				url.push_back('=');
				url.append(tagd::uri_encode(opt_context));
			}

			return url;
		}

		evhtp_request_t *ev_req() const {
			return _ev_req;
		}

		// full request url with scheme://authority: http://somehost.com/tag?...
		std::string canonical_url() const;

		// absolute request url without authority: /tag?...
		std::string abs_url() const;

		// absolute request url with tag view: /tag?v=<view_name>
		std::string abs_url_view(const std::string&) const;
};

class httagl;

class htscanner : public TAGL::scanner {
	public:
		htscanner(httagl *d) : TAGL::scanner((TAGL::driver*)d) {}
};

class httagl : public TAGL::driver {
	public:
		httagl(tagd::tagspace *tsp, tagd::tagspace_session *ssn)
			: TAGL::driver(tsp, new htscanner(this), ssn) {
				_own_scanner = true;
			}
		httagl(tagd::tagspace *tsp, TAGL::callback *cb, tagd::tagspace_session *ssn)
			: TAGL::driver(tsp, new htscanner(this), cb, ssn) {
				_own_scanner = true;
			}

		// undo name hiding so we can use overridden methods across scope
		using TAGL::driver::execute;
		tagd::code execute(transaction&);

		tagd::code tagdurl_get(const request&);
		tagd::code tagdurl_put(const request&);
		tagd::code tagdurl_del(const request&);
		tagd::code scan_request_tagdurl(const request&);
		// The tagdurl scanner validates the path; HTTP owns its composition with the body.
		tagd::code prepare_constrained_body_subject(const request&);
};

// holds members passed to every callback
class base_transaction : public tagd::errorable {
	public:
		server *svr = nullptr;
		request *req = nullptr;
		response *res = nullptr;
		base_transaction(
			server *sv,
			request *r,
			response *s
			) : svr{sv}, req{r}, res{s} {}
};

class transaction : public base_transaction	{
	private:
		// The initial target is only a construction fallback; the driver owns selection.
		tagd::tagspace *_initial_tsp;
	public:
		httagl *drvr;
		tagd::tagspace* tsp() const { return drvr ? drvr->tsp() : _initial_tsp; }
		httagd::viewspace *vws;

		transaction(
			server *sv,
			request *r,
			response *s,
			tagd::tagspace *td,
			httagl *dr,
			httagd::viewspace *vs
		) : base_transaction(sv, r, s),
			_initial_tsp{td}, drvr{dr}, vws{vs}
		{}

		// opt view if not empty, or the default if not empty, or "tagl"
		std::string effective_opt_view() const;

		// adds errors to response ouput buffer in plain text
		void add_errors() const {
			if (this->has_errors())
				res->add_error_str(*this);
		}
};

class callback : public TAGL::callback {
	protected:
		transaction* _tx;
		bool _errors_rendered = false;

		bool resolve_view(view&, view_action);

	public:
		callback(transaction* tx) : _tx{tx} {}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_delete(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
		void cmd_error();
		void finish();
		/*
		 * welcome message or home page
		 * virtual because it get late binded
		 */
		virtual void empty();

		// methods that handle DEFAULT_VIEW which are not in the tagspace
		void default_cmd_get(const tagd::abstract_tag&);
		void default_cmd_put(const tagd::abstract_tag&);
		void default_cmd_delete(const tagd::abstract_tag&);
		void default_cmd_query(const tagd::interrogator&);
		void default_empty();  // welcome message or home page

		transaction* tx() {
			return _tx;
		}

		/*
		 * Render transaction errors once, using the selected view's ERROR handler,
		 * the configured fallback, or built-in TAGL.
		 */
		void output_errors(tagd::code);
};

// ExpandEmitter class allows ctemplate to output directly to an evbuffer
class  evbuffer_emitter : public ctemplate::ExpandEmitter {
	private:
		evbuffer* _buf;

	public:
		evbuffer_emitter(evbuffer* b) : _buf{b} {}

		virtual void Emit(char c) {
			evbuffer_add(_buf, &c, sizeof(char));
		}

		virtual void Emit(const std::string& s) {
			evbuffer_add(_buf, s.c_str(), s.size());
		}

		virtual void Emit(const char* s) {
			evbuffer_add(_buf, s, strlen(s));
		}

		virtual void Emit(const char* s, size_t l) {
			evbuffer_add(_buf, s, l);
		}
};

/*\
|*| Class for using mustache style templates.
|*| We are currently wrapping ctemplate::TemplateDictionary so,
|*| that in case we decide to replace it, all the logic will be encapsulated here.
\*/
	class tagd_template : public tagd::errorable {
	protected:
		ctemplate::TemplateDictionary* _dict;  // conditionally owned: borrowed by wrapper instances, deleted by root-owned templates
		ctemplate::ExpandEmitter* _output;     // conditionally owned: mirrors _dict so borrowed emitters are not reclaimed here
		bool _owner;  // gates destruction of _dict and _output: true for self-allocated roots, false for borrowed objects
		std::string _output_str;

		// reference to dynamically created objects own by this
		std::vector<tagd_template *> _sub_templates;

		tagd_template(ctemplate::TemplateDictionary *t, ctemplate::ExpandEmitter *o) :
			_dict{t},
			_output{o},
			_owner{false} {}

	public:

		tagd_template(const std::string &id) :
			_dict{new ctemplate::TemplateDictionary(id)},
			_output{new ctemplate::StringEmitter(&_output_str)},
			_owner{true} {}

		tagd_template(const std::string &id, evbuffer* b) :
			_dict{new ctemplate::TemplateDictionary(id)},
			_output{new evbuffer_emitter(b)},
			_owner{true} {}

		// deal with undefined default constructors
		tagd_template(tagd_template *haha) :
			_dict{haha->_dict},
			_output{haha->_output},
			_owner{false} {}

		virtual ~tagd_template() {
			if (_owner) {
				delete _dict;
				delete _output;
			}

			for (auto s: _sub_templates)
				if (s) delete s;
		}

		/*
		 * load template filename
		 * static so files can be pre-loaded without instanciating
		 */
		static tagd::code load(tagd::errorable& E, const std::string&);

		// expand template filename into ouput
		tagd::code expand(const std::string&);

		// expand template filname; add errors to viewspace if they occur
		tagd::code expand(viewspace&, const std::string&);

		// output of the last expansion
		const std::string& output_str() const {
			return _output_str;
		}

		// include dictionary subsection given id and template file
		tagd_template* include(const std::string &, const std::string&);

		// add dictionary subsection given id
		tagd_template* add_section(const std::string &id) {
			return this->new_sub_template(_dict->AddSectionDictionary(id));
		}

		// show subsection given marker
		void show_section(const std::string &id) {
			_dict->ShowSection(id);
		}

		// set key in template to value
		void set_value(const std::string &k, const std::string &v) {
			_dict->SetValue(k, v);
		}

		// set key in template to int value
		void set_int_value(const std::string &k, long v) {
			_dict->SetIntValue(k, v);
		}

		// set marker key in template to value if marker found and value non-empty
		void set_value_show_section(const std::string &k, const std::string &v, const std::string &id) {
			_dict->SetValueAndShowSection(k, v, id);
		}

		void set_tag_link(
				const url_query_map_t&,
				const std::string&,  // key
				const std::string&, // val
				const std::string& selector = {});

		void set_tag_link(
				transaction& tx,
				const std::string& key,
				const std::string& val) {
			set_tag_link(tx.req->query_map(), key, val, tx.req->tagspace_selector());
		}

		void set_relator_link (
				transaction& tx,
				const std::string& key,
				const std::string& val) {
											   // * = wildcard relator
			set_tag_link(tx, key, (val.empty() ? "*" : val));
		}


	protected:
		/*
		 * ctemplate::ShowSection() and others return ctemplate::TemplateDictionary*
		 * pointers owned by them.  Similary we will own sub-templates created by this
		 */
		tagd_template* new_sub_template(ctemplate::TemplateDictionary *t) {
			auto s = new tagd_template(t, _output);
			_sub_templates.push_back(s);
			return s;
		}
};

} // namespace httagd
