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

struct viewspace;

class server : public tagsh, public tagd::errorable {
	protected:
		viewspace *_VS;
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
		server(tagspace::sqlite *ts, viewspace *vs, httagd_args *args)
			: tagsh(ts), _VS{vs}, _args{args}
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

		// WTF, why sqlite? This should be a regular tagspace::tagspace
		tagspace::sqlite* TS() {
			return _TS;
		}

		viewspace* VS() {
			return _VS;
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

		static evhtp_res tagd_code_evhtp_res(tagd::code tc) {
			switch (tc) {
				case tagd::TAGD_OK:
					return EVHTP_RES_OK;
				case tagd::TS_NOT_FOUND:
					return EVHTP_RES_NOTFOUND;
				case tagd::TS_DUPLICATE:
					// using 409 - Conflict, 422 - Unprocessable Entity might also be exceptable
					return EVHTP_RES_CONFLICT;
				default:
					return EVHTP_RES_SERVERR;
			}
		}

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

		void send_reply(tagd::code tc) {
			this->send_reply(tagd_code_evhtp_res(tc));
		}

		void send_reply(evhtp_res res) {
			if (_reply_sent) {
				std::cerr << "reply already sent" << std::endl;
				return;
			}
			this->add_header("Content-Type", content_type);
			if (_server->args()->opt_trace)
				std::cerr << "send_reply(" << res << "): " << evhtp_res_str(res) << std::endl;
			evhtp_send_reply(_ev_req, res);
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

		std::string query_opt_search() const {
			return this->query_opt(QUERY_OPT_SEARCH);
		}

		// opt view if not empty, or the default
		std::string effective_opt_view() const {
			std::string view_opt = this->query_opt(QUERY_OPT_VIEW);
			if (view_opt.empty())
				return _server->args()->default_view;
			return view_opt;
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

class transaction;
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
|*| An action will likely be performed on a tagspace before the handler
|*| is called. The handler then populates a response from the results.
\*/
enum struct view_action {
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
		view_id() {}

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
	public:
		std::string tpl_fname;

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
					break;
			}
		}

	public:
		view() : view_id() {
			ctor_handler(); // uses default action
		}

		~view() {
			dtor_handler();
		}

		view(const view& v) : view_id(v), tpl_fname{v.tpl_fname} {
			ctor_handler();
			copy_handler(v);
		}

		view(view&& v) : view_id(std::move(v)), tpl_fname(std::move(v.tpl_fname)) {
			ctor_handler();
			move_handler(std::move(v));
		}

		view& operator=(const view& v) {
			if (_action == v._action) {
				_name = v._name;
				tpl_fname = v.tpl_fname;
			} else {
				dtor_handler(); // destruct current handler
				_name = v._name;
				_action = v._action;
				tpl_fname = v.tpl_fname;
				ctor_handler(); // construct handler of new action
			}
			copy_handler(v);

			return *this;
		}

		view& operator=(view&& v) {
			if (_action == v._action) {
				_name = std::move(v._name);
				tpl_fname = std::move(v.tpl_fname);
			} else {
				dtor_handler(); // destruct current handler
				_name = std::move(v._name);
				_action = std::move(v._action);
				tpl_fname = std::move(v.tpl_fname);
				ctor_handler(); // construct handler of new action
			}
			move_handler(std::move(v));

			return *this;
		}

		view(const std::string& n, const std::string& tn, const empty_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_empty_handler = h;
		}

		view(const std::string& n, const std::string& tn, const get_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_get_handler = h;
		}

		view(const std::string& n, const std::string& tn, const put_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_put_handler = h;
		}

		view(const std::string& n, const std::string& tn, const del_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_del_handler = h;
		}

		view(const std::string& n, const std::string& tn, const query_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_query_handler = h;
		}

		view(const std::string& n, const std::string& tn, const error_handler_t& h)
		: view_id{n, h.action}, tpl_fname{tn} {
				ctor_handler();
				_error_handler = h;
		}

		view(const empty_view_id& v, const std::string& tn, const empty_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_empty_handler = h;
		}

		view(const get_view_id& v, const std::string& tn, const get_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_get_handler = h;
		}

		view(const put_view_id& v, const std::string& tn, const put_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_put_handler = h;
		}

		view(const del_view_id& v, const std::string& tn, const del_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_del_handler = h;
		}

		view(const query_view_id& v, const std::string& tn, const query_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_query_handler = h;
		}

		view(const error_view_id& v, const std::string& tn, const error_handler_t& h)
		: view_id{v}, tpl_fname{tn} {
				assert(v.action() == h.action);
				ctor_handler();
				_error_handler = h;
		}

		static tagd::code error_mismatched_action(const std::string& member, view_action a) {
			std::cerr << "attempting to call " << member << " function with action set to "
				<< view_action_str(a) << std::endl;
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

// container of views(templates and handlers)
// TODO (maybe) extend it from tagspace::tagspace so views can be stored in a tagspace
class viewspace : public tagd::errorable {
		view_map_t _views;
		std::string _tpl_dir;

	public:
		viewspace(const std::string& tpl_dir) : _tpl_dir{tpl_dir}
		{
			if (_tpl_dir.empty()) {
				_tpl_dir = "./tpl/";
			} else {
				if ( _tpl_dir[_tpl_dir.size()-1] != '/' )
					_tpl_dir.push_back('/');
			}
		}

		// TODO insert errors, for get, put failures, etc.
		// if inserting errors is not always desirable,
		// add a register_errors(bool) method

		tagd::code put(const view& vw) {
			if ( !_views.emplace(std::make_pair(static_cast<view_id>(vw), vw)).second )
				return this->ferror(tagd::TS_DUPLICATE, "put view failed: %s %s", vw.action_cstr(), vw.name().c_str());

			return tagd::TAGD_OK;
		}

		tagd::code get(view& vw, const view_id& id) {
			auto it = _views.find(id);
			if (it == _views.end())
				return this->ferror(tagd::TS_NOT_FOUND, "get view failed: %s %s", id.action_cstr(), id.name().c_str());

			vw = it->second;
			return tagd::TAGD_OK;
		}

		tagd::code get(view&& vw, const view_id& id) {
			auto it = _views.find(id);
			if (it == _views.end())
				return this->ferror(tagd::TS_NOT_FOUND, "get view failed: %s %s", id.action_cstr(), id.name().c_str());

			vw = std::move(it->second);
			return tagd::TAGD_OK;
		}

		std::string fpath(const std::string& tpl_fname) {
			return std::string(_tpl_dir)  // has trailing '/'
					.append(tpl_fname);
		}

		std::string fpath(const view& vw) {
			return this->fpath(vw.tpl_fname);
		}
};

typedef std::function<tagd::code(const tagd::errorable&)> errorable_function_t;

// holds members passed to every callback
class transaction : public tagd::errorable {
	public:
		tagspace::tagspace *TS;
		request *req;
		response *res;
		httagd::viewspace *VS;
		std::string context;
		bool trace_on;

		transaction(
			tagspace::tagspace* ts,
			request* rq,
			response* rs,
			httagd::viewspace *vs,
			const std::string& ctx
		) : TS{ts}, req{rq}, res{rs}, VS{vs}, context{ctx},
			trace_on{rq->svr()->args()->opt_trace}
		{}

		bool has_error() const {
			return (
				errorable::has_error() ||  // this has error
				TS->has_error() ||
				VS->has_error()
			);
		}

		size_t size() const {
			return errorable::size()
				+ TS->size()
				+ VS->size();
		}

		void print_errors(std::ostream& os = std::cerr) const {
			errorable::print_errors(os);
			TS->print_errors(os);
			VS->print_errors(os);
		}

		// adds errors to response ouput buffer in plain text
		void add_errors() const {
			if (errorable::has_error()) res->add_error_str(*this);
			if (TS->has_error()) res->add_error_str(*TS);
			if (VS->has_error()) res->add_error_str(*VS);
		}

		// calls errorable functor on each erroable object
		// failure on one results in adding all in plain text
		void add_errors(const errorable_function_t &f) const {
			if (errorable::has_error()) {
				if (f(*this) != tagd::TAGD_OK)
					this->add_errors();
			}
			if (TS->has_error()) {
				if (f(*TS) != tagd::TAGD_OK)
					this->add_errors();
			}
			if (VS->has_error()) {
				if (f(*VS) != tagd::TAGD_OK)
					this->add_errors();
			}
		}
};

// TODO rename just httagd::callback
class tagl_callback : public TAGL::callback {
	protected:
		transaction* _tx;

	public:
		tagl_callback(transaction* tx) : _tx{tx}
			{
				_tx->res->content_type = "text/plain; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
        void cmd_error();
        virtual void empty();  // welcome message or home page
        void finish();

		transaction* tx() {
			return _tx;
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

		// expand template; add errors to viewspace if they occur
		tagd::code expand(viewspace& VS, const std::string& fname) {
			if (this->expand(VS.fpath(fname)) != tagd::TAGD_OK)
				return VS.errors(*this);  // add this errors to viewspace and return last code

			return tagd::TAGD_OK;
		}

		tagd::code expand(viewspace& VS, const view& vw) {
			return this->expand(VS, vw.tpl_fname);
		}

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
				transaction&,
				const std::string&,  // key
				const std::string&); // val

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


class html_callback : public tagl_callback, public tagd::errorable {
	public:
		html_callback(transaction *tx) : tagl_callback(tx)
			{
				_tx->res->content_type = "text/html; charset=utf-8";
			}

		void cmd_get(const tagd::abstract_tag&);
		// void cmd_put(const tagd::abstract_tag&);
		// void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
        void cmd_error();
		void empty();
};

} // namespace httagd

// defined by the user, called before server::start
void init_viewspace(httagd::viewspace &);
