#pragma once

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"
#include "tagsh.h"

#include <cstring>
#include <map>
#include <vector>
#include <evhtp.h>
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

namespace httagd {

class httagd_args : public cmd_args {
	public:
		std::string tpl_dir;
		std::string default_view;
		std::string bind_addr;
		uint16_t bind_port;

		httagd_args () : bind_port{0} {
			_cmds["--tpl-dir"] = {
				[this](char *val) {
						this->tpl_dir = val;
				},
				true
			};

			_cmds["--default-view"] = {
				[this](char *val) {
						this->default_view = val;
				},
				true
			};

			_cmds["--bind-addr"] = {
				[this](char *val) {
						this->bind_addr = val;
				},
				true
			};

			_cmds["--bind-port"] = {
				[this](char *val) {
						this->bind_port = static_cast<uint16_t>(
								atoi( static_cast<const char*>(val) )
							);
				},
				true
			};
		}
};


class server : public tagsh, public tagd::errorable {
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
			: tagsh(ts), _args{nullptr}, _bind_addr{"localhost"}, _bind_port{2112}
		{ this->init(); }

		server(tagspace::sqlite *ts, httagd_args *args)
			: tagsh(ts), _args{args}
		{
			_bind_addr = (!args->bind_addr.empty() ? args->bind_addr : "localhost");
			_bind_port = (args->bind_port ? args->bind_port : 2112);
			this->init();
		}

		const std::string& bind_addr() const {
			return _bind_addr;
		}

		uint16_t bind_port() const {
			return _bind_port;
		}

		tagspace::sqlite* tagspace() {
			return _TS;
		}

		httagd_args * args() {
			return _args;
		}

		tagd::code start();
};


class response {
	protected:
		server *_server;
		evhtp_request_t *_ev_req;
		bool _reply_sent;

	public:
		std::string content_type;
		response(server* S, evhtp_request_t *req)
			: _server{S}, _ev_req{req}, _reply_sent{false}
		{}

		bool reply_sent() const {
			return _reply_sent;
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

		void send_reply(evhtp_res r) {
			if (_reply_sent) {
				std::cerr << "reply already sent" << std::endl;
				return;
			}
			this->add_header("Content-Type", content_type);
			if (_server->args()->opt_trace)
				std::cerr << "send_reply(" << r << ")" << std::endl;
			evhtp_send_reply(_ev_req, r);
			_reply_sent = true;
		}

		void add_header(const std::string &k, const std::string &v) {
			if (_server->args()->opt_trace)
				std::cerr << "add_header(" << '"' << k << '"' << ", " << '"' << v << '"' << ")" << std::endl;

/* from evhtp.h
 * evhtp_header_new
 * @param key null terminated string
 * @param val null terminated string
 * @param kalloc if set to 1, the key will be copied, if 0 no copy is done.
 * @param valloc if set to 1, the val will be copied, if 0 no copy is done.
 */
			evhtp_headers_add_header(
				_ev_req->headers_out,
				// params 3,4 must be set, else strings from previous calls can get intermingled
				evhtp_header_new(k.c_str(), v.c_str(), 1, 1)
			);
		}

		void add_header_content_length(size_t sz) {
			this->add_header("Content-Length", std::to_string(sz));
		}

		void send_error_str(const tagd::errorable &err) {
			std::stringstream ss;
			err.print_errors(ss);
			this->add(ss.str());
			this->send_reply(EVHTP_RES_SERVERR);
		}

};

// request url query options
const std::string QUERY_OPT_SEARCH{"q"};    // full text search
const std::string QUERY_OPT_VIEW{"v"};		// view name
const std::string QUERY_OPT_CONTEXT{"c"};   // tagspace context

typedef enum {
	MEDIA_TYPE_TEXT_TAGL,
	MEDIA_TYPE_TEXT_HTML
} media_type_t;

// supported HTTP methods
typedef enum {
	HTTP_GET = 0,
	HTTP_HEAD,
	HTTP_POST,
	HTTP_PUT,
	HTTP_DELETE
} http_method;

class request {
	private:
		url_query_map_t _query_map;

	protected:
		server *_server;
		evhtp_request_t *_ev_req;
		std::string _path; // for testing

	public:
		http_method method;

		request(server* S, evhtp_request_t *ev_req)
			: _server{S}, _ev_req{ev_req}
		{
			if ( ev_req->uri->query_raw != NULL ) {
				tagd::url::parse_query(
					_query_map, (char*)ev_req->uri->query_raw );
			}

			_path = ev_req->uri->path->full;
		}

		request(const std::string &path)	// for testing
			: _server{nullptr}, _ev_req{nullptr}, _path{path} {}

		server *svr() {
			return _server;
		}

		std::string query_opt(const std::string &opt) const {
			std::string val;
			tagd::url::query_find(_query_map, val, opt);
			return val;
		}

		const url_query_map_t &query_map() const {
			return _query_map;
		}

		std::string path() const {
			return _path;
		}

		std::string url() const;
};

class httagl;

class htscanner : public TAGL::scanner {
	public:
		htscanner(httagl *d) : TAGL::scanner((TAGL::driver*)d) {}
		void scan_tagdurl_path(int cmd, const request&);
};

class httagl : public TAGL::driver {
	public:
		httagl(tagspace::tagspace *ts)
			: TAGL::driver(ts, new htscanner(this)) {}
		httagl(tagspace::tagspace *ts, TAGL::callback *cb)
			: TAGL::driver(ts, new htscanner(this), cb) {}
		~httagl() { delete _scanner; }
		tagd_code tagdurl_get(const request&);
		tagd_code tagdurl_put(const request&);
		tagd_code tagdurl_del(const request&);
};

struct transaction;
struct view;
class tagd_template;

typedef std::function<int(transaction&, const view&, tagd_template&)> home_handler_t;
typedef std::function<int(transaction&, const view&, tagd_template&, const tagd::abstract_tag&)> tag_handler_t;
typedef std::function<int(transaction&, const view&, tagd_template&, const tagd::abstract_tag&, size_t *num_children)> tree_handler_t;
typedef std::function<int(transaction&, const view&, tagd_template&, const tagd::interrogator&, const tagd::tag_set&)> interrogator_handler_t;
typedef std::function<void(transaction&, const view&, tagd_template&, const tagd::errorable& E)> error_handler_t;

struct view {
	std::string name;  // view query option: v=name
	std::string tpl_fname;

	enum {
		UNKNOWN,
		HOME,
		TAG,
		TREE,
		RELATIONS,
		BROWSE,
		QUERY,
		HEADER,
		FOOTER,
		ERROR
	} type;  // specifies the union handler

	union handler {
		tag_handler_t tag_handler;
		tree_handler_t tree_handler;
		interrogator_handler_t interrogator_handler;
		error_handler_t error_handler;
		~handler(){}
	};
};

const view UNKNOWN_VIEW{"", "", view::UNKNOWN};

typedef std::map< std::string, view > view_map_t;

const std::string HOME_VIEW{"home.html"};
const std::string TAG_VIEW{"tag.html"};
const std::string TREE_VIEW{"tree.html"};
const std::string BROWSE_VIEW{"browse.html"};
const std::string RELATIONS_VIEW{"relations.html"};
const std::string QUERY_VIEW{"query.html"};
const std::string ERROR_VIEW{"error.html"};
const std::string HEADER_VIEW{"header.html"};
const std::string FOOTER_VIEW{"footer.html"};


// container of views, templates and handlers
class router : public tagd::errorable {
		tagspace::tagspace *_TS;
		request *_req;
		response *_res;
		std::string _tpl_dir;
		std::string _context;
		view_map_t _views;

	public:
		router(tagspace::tagspace* ts, request *req, response *res) :
			_TS{ts}, _req{req}, _res{res}, _tpl_dir{req->svr()->args()->tpl_dir},
			_context{req->query_opt(QUERY_OPT_CONTEXT)}
		{
			if (_tpl_dir.empty()) {
				_tpl_dir = "./tpl/";
			} else {
				if ( _tpl_dir[_tpl_dir.size()-1] != '/' )
					_tpl_dir.push_back('/');
			}

			// TODO consider using a memory-only tagspace to store these details
			_views[HOME_VIEW] = {HOME_VIEW, "home.html.tpl", view::HOME};
			_views[TAG_VIEW] = {TAG_VIEW, "tag.html.tpl", view::TAG};
			_views[TREE_VIEW] = {TREE_VIEW, "tree.html.tpl", view::TREE};
			_views[BROWSE_VIEW] = {BROWSE_VIEW, "browse.html.tpl", view::BROWSE};
			_views[RELATIONS_VIEW ] = {RELATIONS_VIEW, "relations.html.tpl", view::RELATIONS};
			_views[QUERY_VIEW] = {QUERY_VIEW, "query.html.tpl", view::QUERY};
			_views[ERROR_VIEW] = {ERROR_VIEW, "error.html.tpl", view::ERROR};
			_views[HEADER_VIEW] = {HEADER_VIEW, "header.html.tpl", view::HEADER};
			_views[FOOTER_VIEW] = {FOOTER_VIEW, "footer.html.tpl", view::FOOTER};
		}

		/* TODO maybe
		void put ( const view& tpl ) {
			_views[tpl.val] = tpl;
		}  */

		view get(const std::string&);

		std::string fpath(const view &tpl) {
			return std::string(_tpl_dir)  // has trailing '/'
					.append( tpl.tpl_fname );
		}

		//void expand_template(const view& tpl, const tagd_template& D);
		void expand_header(const std::string& title);
		void expand_footer();
		int expand_home(const view& tpl, tagd_template& D);
		int expand_tag(const view& tpl, const tagd::abstract_tag& t, tagd_template& D);
		int expand_browse(const view& tpl, const tagd::abstract_tag& t, tagd_template& D);
		int expand_relations(const view& tpl, const tagd::abstract_tag& t, tagd_template& D);
		int expand_tree(const view& tpl, const tagd::abstract_tag& t, tagd_template& D, size_t *num_children=nullptr);
		int expand_query(const view& tpl, const tagd::interrogator& q, const tagd::tag_set& R, tagd_template& D);
		void expand_error(const view& tpl, const tagd::errorable& E);
};


// holds members passed to every callback
struct transaction {
	tagspace::tagspace *TS;
	request *req;
	response *res;
	httagd::router *router;
	std::string context;

	bool has_error() const {
		return (
			TS->has_error() ||
			router->has_error()
		);
	}

	void send_errors_res() {
		if (TS->has_error()) res->send_error_str(*TS);
		if (router->has_error()) res->send_error_str(*router);
	}
};


class tagl_callback : public TAGL::callback {
	protected:
		tagspace::tagspace *_TS;
		request *_req;
		response *_res;
		router _router;
		transaction _transaction;

	public:
		tagl_callback(
				tagspace::tagspace* ts,
				request* req,
				response* res,
				const std::string& ctx
			) : _TS{ts}, _req{req}, _res{res},
				_router(ts, req, res), _transaction{ts, req, res, &_router, ctx}
			{
				_res->content_type = "text/plain; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
        void cmd_error();
        virtual void empty();  // welcome message or home page
        void finish();
		transaction tx() const {
			return _transaction;
		}
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
		ctemplate::TemplateDictionary* _dict;
		ctemplate::ExpandEmitter* _output;
		bool _owner;  // this owns _dict and _output resources
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
		tagd_template(tagd_template *caca) :
			_dict{caca->_dict},
			_output{caca->_output},
			_owner{false} {}

		virtual ~tagd_template() {
			if (_owner) {
				delete _dict;
				delete _output;
			}

			for (auto s: _sub_templates)
				if (s) delete s;
		}

		// expand template filename into ouput
		tagd_code expand(const std::string&);

		// output of the last expansion
		const std::string& output_str() const {
			return _output_str;
		}

		// add dictionary subsection given id
		tagd_template* add_section(const std::string &);

		// include dictionary subsection given id and template file
		tagd_template* include(const std::string &, const std::string&);

		// show subsection given marker
		void show_section(const std::string &);

		// set key in template to value
		void set_value(const std::string &k, const std::string &v);

		// set key in template to int value
		void set_int_value(const std::string &k, long v );

		// set marker key in template to value if marker found and value non-empty
		void set_value_show_section(const std::string &k, const std::string &v, const std::string &id);

		void set_tag_link(
				const std::string&,
				const std::string&,
				const view&,
				const std::string *c = nullptr);

		void set_relator_link(
				const std::string&,
				const std::string&,
				const view&,
				const std::string *c = nullptr);

		void set_query_link(const std::string& k, const std::string& v);
		void set_query_related_link(const std::string& k, const std::string& v);

	protected:
		// ctemplate::ShowSection() and others return ctemplate::TemplateDictionary*
		// pointers owned by them.  Similary we will own sub-templates created by this
		tagd_template* new_sub_template(ctemplate::TemplateDictionary *t) {
			auto s = new tagd_template(t, _output);
			_sub_templates.push_back(s);
			return s;
		}
};


class html_callback : public tagl_callback, public tagd::errorable {
	protected:
		bool _trace_on;

	public:
		html_callback(
				tagspace::tagspace* ts,
				request* req,
				response *res,
				std::string ctx
			) : tagl_callback(ts, req, res, ctx),
					_trace_on(req->svr()->args()->opt_trace)
			{
				_res->content_type = "text/html; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		// void cmd_put(const tagd::abstract_tag&);
		// void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
        void cmd_error();
		void empty();
};


} // namespace httagd
