#pragma once

#include "tagd.h"
#include "tagl.h"
#include "tagdb.h"
#include "tagsh.h"

#include <cstring>
#include <map>
#include <vector>
#include <evhtp.h>
#include <ctemplate/template.h>

const char* evhtp_res_str(int);

extern bool HTTAGD_TRACE_ON;

namespace httagd {

inline void HTTAGD_SET_TRACE_ON() {
	HTTAGD_TRACE_ON = true;
	/*
	static bool trace_init = false;
	if (!trace_init) { // init once ...
		trace_init = true;
		// init ...
	}
	*/
}

inline void HTTAGD_SET_TRACE_OFF() {
	HTTAGD_TRACE_ON = false;
}

#define HTTAGD_LOG_TRACE(MSG) if(HTTAGD_TRACE_ON) \
	{ std::cerr <<  __FILE__  << ':' << __LINE__ << '\t' << MSG ; }

class httagd_args : public cmd_args {
	public:
		std::string tpl_dir;
		std::string www_dir;
		std::string favicon;
		std::string default_view;
		std::string bind_addr;
		uint16_t bind_port;

		httagd_args () : bind_port{0} {
			_cmds["--tpl-dir"] = {
				[this](char *val) {
						if (!tagd::io::dir_exists(val)) {
							this->ferror(tagd::TAGD_ERR,
								"invalid value for argument --tpl-dir: no such directory: %s", val);
							return;
						}
						this->tpl_dir = val;
				},
				true
			};

			_cmds["--www-dir"] = {
				[this](char *val) {
						if (!tagd::io::dir_exists(val)) {
							this->ferror(tagd::TAGD_ERR,
								"invalid value for argument --www-dir: no such directory: %s", val);
							return;
						}
						this->www_dir = val;
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

struct viewspace;

class server : public tagsh, public tagd::errorable {
	protected:
		viewspace *_vws;
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
		server(tagdb::sqlite *tdb, viewspace *vs, httagd_args *args)
			: tagsh(tdb), _vws{vs}, _args{args}
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

		// WTF, why sqlite? This should be a regular tagdb::tagdb
		tagdb::sqlite* tdb() {
			return _tdb;
		}

		viewspace* vws() {
			return _vws;
		}

		httagd_args * args() {
			return _args;
		}

		tagd::code start();
};

class base_transaction;
class transaction;

static const std::string DEFAULT_CONTENT_TYPE{"text/plain; charset=utf-8"};

class response {
	protected:
		evhtp_request_t *_ev_req;
		// res_code sent
		// when set to >= 0 before sending,
		// send this code instead of translated tagd::code to EVHTP_RES_*
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

// request url query options
const std::string QUERY_OPT_SEARCH{"q"};    // full text search
const std::string QUERY_OPT_VIEW{"v"};		// view name
const std::string QUERY_OPT_CONTEXT{"c"};   // tagspace context

const std::string DEFAULT_VIEW{"tagl"};     // plain text tagl

// supported HTTP methods
typedef enum {
	HTTP_UNKNOWN = 0,
	HTTP_GET,
	HTTP_HEAD,
	HTTP_POST,
	HTTP_PUT,
	HTTP_DELETE
} http_method;

class request {
	public:
		http_method method = HTTP_UNKNOWN;

	protected:
		evhtp_request_t *_ev_req = nullptr;
		std::string _path;
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

		request(http_method meth, const std::string path)
			: method{meth}, _path{path} {}  // for Tester

		std::string query_opt(const std::string &opt) const {
			std::string val;
			tagd::url::query_find(_query_map, val, opt);
			return val;
		}

		std::string query_opt_search() const {
			return this->query_opt(QUERY_OPT_SEARCH);
		}

		std::string query_opt_view() const {
			return this->query_opt(QUERY_OPT_VIEW);
		}

		std::string query_opt_context() const {
			return this->query_opt(QUERY_OPT_CONTEXT);
		}

		const url_query_map_t &query_map() const {
			return _query_map;
		}

		const std::string& path() const {
			return _path;
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
		void scan_tagdurl_path(int cmd, const request&);
};

class httagl : public TAGL::driver {
	public:
		httagl(tagdb::tagdb *tdb, tagdb::session *ssn)
			: TAGL::driver(tdb, new htscanner(this), ssn) {
				_own_scanner = true;
			}
		httagl(tagdb::tagdb *tdb, TAGL::callback *cb, tagdb::session *ssn)
			: TAGL::driver(tdb, new htscanner(this), cb, ssn) {
				_own_scanner = true;
			}

		// undo name hiding so we can use overridden methods across scope
		using TAGL::driver::execute;
		tagd::code execute(transaction&);

		tagd::code tagdurl_get(const request&);
		tagd::code tagdurl_put(const request&);
		tagd::code tagdurl_del(const request&);
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
	public:
		tagdb::tagdb *tdb;
		httagl *drvr;
		httagd::viewspace *vws;

		transaction(
			server *sv,
			request *r,
			response *s,
			tagdb::tagdb *td,
			httagl *dr,
			httagd::viewspace *vs
		) : base_transaction(sv, r, s),
			tdb{td}, drvr{dr}, vws{vs}
		{}

		// opt view if not empty, or the default if not empty, or "tagl"
		std::string effective_opt_view() const;

		// adds errors to response ouput buffer in plain text
		void add_errors() const {
			if (this->has_errors())
				res->add_error_str(*this);
		}
};

class view;

typedef std::function<tagd::code(transaction&, const view&)> empty_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> get_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> put_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> del_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::interrogator&, const tagd::tag_set&)> query_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::errorable& E)> error_function_t;

/*\
|*| Each action specifies the selected handler member in the view's
|*| anonymous union. Its called an action because it corresponds to
|*| the command method (cmd_*) that was called in the httagd callback.
|*| An action will likely be performed on a tagdb before the handler
|*| is called. The handler then populates a response from the results.
\*/
enum struct view_action {
	UNDEF,  // view unitialized
	EMPTY,
	GET,
	PUT,
	DEL,
	QUERY,
	ERROR
};

static const char* view_action_str(view_action a) {
	switch (a) {
		case view_action::EMPTY: return "EMPTY";
		case view_action::GET: return "GET";
		case view_action::PUT: return "PUT";
		case view_action::DEL: return "DEL";
		case view_action::QUERY: return "QUERY";
		case view_action::ERROR: return "ERROR";
		case view_action::UNDEF: return "UNDEF";
		default: assert(0);
	}

	return "";
}

/*\
|*| we have to wrap the function typedefs to give them proper names
|*| otherwise, functions with identical signatures will not be able
|*| to be used in overloaded methods
\*/
struct empty_handler_t {
	const static auto action = view_action::EMPTY;
	empty_function_t function;
	empty_handler_t() = default;
	empty_handler_t(const empty_function_t& f) : function{f} {}
	empty_handler_t(empty_function_t&& f) : function{std::move(f)} {}
};
struct get_handler_t {
	const static auto action = view_action::GET;
	get_function_t function;
	get_handler_t() = default;
	get_handler_t(const get_function_t& f) : function{f} {}
	get_handler_t(get_function_t&& f) : function{std::move(f)} {}
};
struct put_handler_t {
	const static auto action = view_action::PUT;
	put_function_t function;
	put_handler_t() = default;
	put_handler_t(const put_function_t& f) : function{f} {}
	put_handler_t(put_function_t&& f) : function{std::move(f)} {}
};
struct del_handler_t {
	const static auto action = view_action::DEL;
	del_function_t function;
	del_handler_t() = default;
	del_handler_t(const del_function_t& f) : function{f} {}
	del_handler_t(del_function_t&& f) : function{std::move(f)} {}
};
struct query_handler_t {
	const static auto action = view_action::QUERY;
	query_function_t function;
	query_handler_t() = default;
	query_handler_t(const query_function_t& f) : function{f} {}
	query_handler_t(query_function_t&& f) : function{std::move(f)} {}
};
struct error_handler_t {
	const static auto action = view_action::ERROR;
	error_function_t function;
	error_handler_t() = default;
	error_handler_t(const error_function_t& f) : function{f} {}
	error_handler_t(error_function_t&& f) : function{std::move(f)} {}
};


// A view_id is a name+action used as a view_map_t key and also the base class for a view
class view_id {
	protected:
		std::string _name;  // view query option: v=name
		view_action _action;

	public:
		view_id() : _action{view_action::UNDEF} {}

		view_id(const std::string& n, view_action a)
			: _name{n}, _action{a} {}

		const std::string& name() const {
			return _name;
		}

		view_action action() const {
			return _action;
		}

		const char* action_cstr() const {
			return view_action_str(_action);
		}

		bool operator<(const view_id& r) const {
			return (
				_name < r._name ||
				(_name == r._name && _action < r._action)
			);
		}
};

class empty_view_id : public view_id {
	public:
		empty_view_id(const std::string& n)
			: view_id{n, view_action::EMPTY} {}
};
class get_view_id : public view_id {
	public:
		get_view_id(const std::string& n)
			: view_id{n, view_action::GET} {}
};
class put_view_id : public view_id {
	public:
		put_view_id(const std::string& n)
			: view_id{n, view_action::PUT} {}
};
class del_view_id : public view_id {
	public:
		del_view_id(const std::string& n)
			: view_id{n, view_action::DEL} {}
};
class query_view_id : public view_id {
	public:
		query_view_id(const std::string& n)
			: view_id{n, view_action::QUERY} {}
};
class error_view_id : public view_id {
	public:
		error_view_id(const std::string& n)
			: view_id{n, view_action::ERROR} {}
};

class view : public view_id {
	private:
		/*\
		|*| enum discriminated anonymous union of non-trivial types
		|*| inspired by the man himself:
		|*| http://www.stroustrup.com/C++11FAQ.html#unions
		\*/
		union {
			empty_handler_t _empty_handler;  // EMPTY
			get_handler_t _get_handler;      // GET
			put_handler_t _put_handler;      // PUT
			del_handler_t _del_handler;      // DEL
			query_handler_t _query_handler;  // QUERY
			error_handler_t _error_handler;  // ERROR
		};

		void ctor_handler() {
			switch (_action) {
				case view_action::EMPTY:
					new(&_empty_handler) empty_handler_t();
					break;
				case view_action::GET:
					new(&_get_handler) get_handler_t();
					break;
				case view_action::PUT:
					new(&_put_handler) put_handler_t();
					break;
				case view_action::DEL:
					new(&_del_handler) del_handler_t();
					break;
				case view_action::QUERY:
					new(&_query_handler) query_handler_t();
					break;
				case view_action::ERROR:
					new(&_error_handler) error_handler_t();
					break;
				case view_action::UNDEF:
					break;  // NOOP
				default: assert(0);
			}
		}

		void dtor_handler() {
			switch (_action) {
				case view_action::EMPTY:
					_empty_handler.~empty_handler_t();
					break;
				case view_action::GET:
					_get_handler.~get_handler_t();
					break;
				case view_action::PUT:
					_put_handler.~put_handler_t();
					break;
				case view_action::DEL:
					_del_handler.~del_handler_t();
					break;
				case view_action::QUERY:
					_query_handler.~query_handler_t();
					break;
				case view_action::ERROR:
					_error_handler.~error_handler_t();
					break;
				case view_action::UNDEF:
					break; // NOOP
				default: assert(0);
			}
		}

		void copy_handler(const view& v) {
			assert(_action == v._action);

			switch (_action) {
				case view_action::EMPTY:
					_empty_handler = v._empty_handler;
					break;
				case view_action::GET:
					_get_handler = v._get_handler;
					break;
				case view_action::PUT:
					_put_handler = v._put_handler;
					break;
				case view_action::DEL:
					_del_handler = v._del_handler;
					break;
				case view_action::QUERY:
					_query_handler = v._query_handler;
					break;
				case view_action::ERROR:
					_error_handler = v._error_handler;
					break;
				case view_action::UNDEF:
					break; // NOOP
				default: assert(0);
			}
		}

		void move_handler(view&& v) {
			assert(_action == v._action);

			switch (_action) {
				case view_action::EMPTY:
					_empty_handler = std::move(v._empty_handler);
					break;
				case view_action::GET:
					_get_handler = std::move(v._get_handler);
					break;
				case view_action::PUT:
					_put_handler = std::move(v._put_handler);
					break;
				case view_action::DEL:
					_del_handler = std::move(v._del_handler);
					break;
				case view_action::QUERY:
					_query_handler = std::move(v._query_handler);
					break;
				case view_action::ERROR:
					_error_handler = std::move(v._error_handler);
				case view_action::UNDEF:
					break; // NOOP
				default: assert(0);
			}
		}

	public:
		view() : view_id() {
			ctor_handler(); // uses default action
		}

		~view() {
			dtor_handler();
		}

		view(const view& v) : view_id(v) {
			ctor_handler();
			copy_handler(v);
		}

		view(view&& v) : view_id(std::move(v))  {
			ctor_handler();
			move_handler(std::move(v));
		}

		view& operator=(const view& v) {
			if (_action == v._action) {
				_name = v._name;
			} else {
				dtor_handler(); // destruct current handler
				_name = v._name;
				_action = v._action;
				ctor_handler(); // construct handler of new action
			}
			copy_handler(v);

			return *this;
		}

		view& operator=(view&& v) {
			if (_action == v._action) {
				_name = std::move(v._name);
			} else {
				dtor_handler(); // destruct current handler
				_name = std::move(v._name);
				_action = std::move(v._action);
				ctor_handler(); // construct handler of new action
			}
			move_handler(std::move(v));

			return *this;
		}

		view(const std::string& n, const empty_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_empty_handler = h;
		}

		view(const std::string& n, const get_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_get_handler = h;
		}

		view(const std::string& n, const put_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_put_handler = h;
		}

		view(const std::string& n, const del_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_del_handler = h;
		}

		view(const std::string& n, const query_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_query_handler = h;
		}

		view(const std::string& n, const error_handler_t& h)
		: view_id{n, h.action} {
				ctor_handler();
				_error_handler = h;
		}

		view(const empty_view_id& v, const empty_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_empty_handler = h;
		}

		view(const get_view_id& v, const get_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_get_handler = h;
		}

		view(const put_view_id& v, const put_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_put_handler = h;
		}

		view(const del_view_id& v, const del_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_del_handler = h;
		}

		view(const query_view_id& v, const query_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_query_handler = h;
		}

		view(const error_view_id& v, const error_handler_t& h)
		: view_id{v} {
				assert(v.action() == h.action);
				ctor_handler();
				_error_handler = h;
		}

		static tagd::code error_mismatched_action(const std::string& member, view_action a) {
			LOG_ERROR( "attempting to call " << member << " function with action set to "
				<< view_action_str(a) << std::endl )
			return tagd::TAGD_ERR;
		}

		// passthrough methods - signatures must match function typedefs
		tagd::code empty_function(transaction& tx, const view& v) {
			if (_action != view_action::EMPTY)
				return error_mismatched_action("empty_handler", _action);

			return _empty_handler.function(tx, v);
		}

		tagd::code get_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (_action != view_action::GET)
				return error_mismatched_action("get_handler", _action);

			return _get_handler.function(tx, v, T);
		}

		tagd::code put_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (_action != view_action::PUT)
				return error_mismatched_action("put_handler", _action);

			return _put_handler.function(tx, v, T);
		}

		tagd::code del_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (_action != view_action::DEL)
				return error_mismatched_action("del_handler", _action);

			return _del_handler.function(tx, v, T);
		}

		tagd::code query_function(transaction& tx, const view& v, const tagd::interrogator& q, const tagd::tag_set& S) {
			if (_action != view_action::QUERY)
				return error_mismatched_action("query_handler", _action);

			return _query_handler.function(tx, v, q, S);
		}

		tagd::code error_function(transaction& tx, const view& v, const tagd::errorable& e) {
			if (_action != view_action::ERROR)
				return error_mismatched_action("error_handler", _action);

			return _error_handler.function(tx, v, e);
		}

		// don't allow users to construct objects with mismatched action and handlers
		view(const std::string&, view_action, const std::string&, const empty_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const get_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const put_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const del_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const query_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const error_handler_t&) = delete;
};

/*\
|*| The key of a view map is a composite of {name + action}
|*| A view name represents a family of handlers each
|*| with seperate actions.  For example:
|*| name            action          handler
|*| ----            ----            -------
|*| browse.html     EMPTY           empty_handler
|*| browse.html     GET             get_handler
|*| browse.html     QUERY           query_handler
|*| browse.html     ERROR           error_handler
\*/
typedef std::map< view_id, view > view_map_t;


const error_function_t default_error_function(
	[](transaction& tx, const view& /*vw*/, const tagd::errorable& E)
	-> tagd::code {
		tx.res->add_error_str(E);
		return tagd::TAGD_OK;
	}
);

const view default_error_view(
	error_view_id(std::string()),
	default_error_function
);

// container of views(templates and handlers)
// TODO (maybe) extend it from tagdb::tagdb so views can be stored in a tagdb
class viewspace : public tagd::errorable {
		view_map_t _views;
		std::string _tpl_dir;

	public:
		// called if no error view has been inserted for a given name
		view fallback_error_view;

		viewspace(const std::string& tpl_dir) :
			_tpl_dir{tpl_dir}, fallback_error_view(default_error_view) {} 
		// TODO add a flags variable to get(), put(), etc.
		// such as F_DISABLE_ERROR_REPORTING

		tagd::code put(const view& vw) {
			if ( !_views.emplace(std::make_pair(static_cast<view_id>(vw), vw)).second )
				return this->ferror(tagd::TS_DUPLICATE, "put view failed: %s, %s", vw.action_cstr(), vw.name().c_str());

			return tagd::TAGD_OK;
		}

		tagd::code get(view& vw, const view_id& id) {
			auto it = _views.find(id);
			if (it == _views.end())
				return this->ferror(tagd::TS_NOT_FOUND, "get view failed: %s, %s", id.action_cstr(), id.name().c_str());

			vw = it->second;
			return tagd::TAGD_OK;
		}

		tagd::code get(view&& vw, const view_id& id) {
			auto it = _views.find(id);
			if (it == _views.end())
				return this->ferror(tagd::TS_NOT_FOUND, "get view failed: %s, %s", id.action_cstr(), id.name().c_str());

			vw = std::move(it->second);
			return tagd::TAGD_OK;
		}

		std::string fpath(const std::string& tpl_fname) {
			return tagd::io::concat_dir(_tpl_dir, tpl_fname); 
		}
};

class callback : public TAGL::callback {
	protected:
		transaction* _tx;

	public:
		callback(transaction* tx) : _tx{tx} {}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
        void cmd_error();
        void finish();
		// welcome message or home page
		// virtual because it get late binded
        virtual void empty();

		// methods that handle DEFAULT_VIEW which are not in the tagdb
		void default_cmd_get(const tagd::abstract_tag&);
		void default_cmd_put(const tagd::abstract_tag&);
		void default_cmd_del(const tagd::abstract_tag&);
		void default_cmd_query(const tagd::interrogator&);
        void default_cmd_error();
        void default_empty();  // welcome message or home page

		transaction* tx() {
			return _tx;
		}

		// output transaction errors, calling a the error handler if given a view name
		// or a fallback error handler, or to plain text the other handlers fail
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

		// load template filename
		// static so files can be pre-loaded without instanciating
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
				const std::string&); // val

		void set_tag_link(
				transaction& tx,
				const std::string& key,
				const std::string& val) {
			set_tag_link(tx.req->query_map(), key, val);
		}

		void set_relator_link (
				transaction& tx,
				const std::string& key,
				const std::string& val) {
			                                   // * = wildcard relator
			set_tag_link(tx, key, (val.empty() ? "*" : val));
		}


	protected:
		// ctemplate::ShowSection() and others return ctemplate::TemplateDictionary*
		// pointers owned by them.  Similary we will own sub-templates created by this
		tagd_template* new_sub_template(ctemplate::TemplateDictionary *t) {
			auto s = new tagd_template(t, _output);
			_sub_templates.push_back(s);
			return s;
		}
};

} // namespace httagd
