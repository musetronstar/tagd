#pragma once

#include "tagd.h"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <variant>

namespace tagd {
class config;
}

namespace httagd {

class transaction;

const std::string DEFAULT_VIEW{"tagl"};     // plain text tagl

class view;

typedef std::function<tagd::code(transaction&, const view&)> empty_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> get_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> put_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::abstract_tag&)> delete_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::interrogator&, const tagd::tag_set&)> query_function_t;
typedef std::function<tagd::code(transaction&, const view&, const tagd::errorable& E)> error_function_t;

/*\
|*| Each action identifies the view's handler type. It is called an
|*| action because it corresponds to
|*| the command method (cmd_*) that was called in the httagd callback.
|*| GET, QUERY, PUT, and DELETE handlers run after successful tagspace
|*| operations and populate the response. Rendering errors cannot undo
|*| completed writes; failed operations use the ERROR representation.
|*| QUERY also represents empty sets; their TS_NOT_FOUND status is not an error.
\*/
enum struct view_action {
	UNDEF,  // view unitialized
	EMPTY,
	GET,
	PUT,
	DELETE,
	QUERY,
	ERROR
};

const char* view_action_str(view_action);

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
struct delete_handler_t {
	const static auto action = view_action::DELETE;
	delete_function_t function;
	delete_handler_t() = default;
	delete_handler_t(const delete_function_t& f) : function{f} {}
	delete_handler_t(delete_function_t&& f) : function{std::move(f)} {}
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
class delete_view_id : public view_id {
	public:
		delete_view_id(const std::string& n)
			: view_id{n, view_action::DELETE} {}
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

using view_handler = std::variant<std::monostate, empty_handler_t, get_handler_t,
	put_handler_t, delete_handler_t, query_handler_t, error_handler_t>;

class view : public view_id {
	private:
		/*
		 * The variant owns handler lifetime. Constructors derive both the action and
		 * active alternative from the handler type, keeping dispatch and storage aligned.
		 */
		view_handler _handler;

	public:
		view() = default;

		view(const std::string& n, const empty_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const std::string& n, const get_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const std::string& n, const put_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const std::string& n, const delete_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const std::string& n, const query_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const std::string& n, const error_handler_t& h)
			: view_id{n, h.action}, _handler{h} {
		}

		view(const empty_view_id& v, const empty_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		view(const get_view_id& v, const get_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		view(const put_view_id& v, const put_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		view(const delete_view_id& v, const delete_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		view(const query_view_id& v, const query_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		view(const error_view_id& v, const error_handler_t& h)
			: view(v.name(), h) {
			assert(v.action() == h.action);
		}

		static tagd::code error_mismatched_action(const std::string& member, view_action a) {
			LOG_ERROR( "attempting to call " << member << " function with action set to "
				<< view_action_str(a) << std::endl )
			return tagd::TAGD_ERR;
		}

		// passthrough methods - signatures must match function typedefs
		tagd::code empty_function(transaction& tx, const view& v) {
			if (const auto* handler = std::get_if<empty_handler_t>(&_handler))
				return handler->function(tx, v);

			return error_mismatched_action("empty_handler", _action);
		}

		tagd::code get_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (const auto* handler = std::get_if<get_handler_t>(&_handler))
				return handler->function(tx, v, T);

			return error_mismatched_action("get_handler", _action);
		}

		tagd::code put_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (const auto* handler = std::get_if<put_handler_t>(&_handler))
				return handler->function(tx, v, T);

			return error_mismatched_action("put_handler", _action);
		}

		tagd::code delete_function(transaction& tx, const view& v, const tagd::abstract_tag& T) {
			if (const auto* handler = std::get_if<delete_handler_t>(&_handler))
				return handler->function(tx, v, T);

			return error_mismatched_action("delete_handler", _action);
		}

		tagd::code query_function(transaction& tx, const view& v, const tagd::interrogator& q, const tagd::tag_set& S) {
			if (const auto* handler = std::get_if<query_handler_t>(&_handler))
				return handler->function(tx, v, q, S);

			return error_mismatched_action("query_handler", _action);
		}

		tagd::code error_function(transaction& tx, const view& v, const tagd::errorable& e) {
			if (const auto* handler = std::get_if<error_handler_t>(&_handler))
				return handler->function(tx, v, e);

			return error_mismatched_action("error_handler", _action);
		}

		// don't allow users to construct objects with mismatched action and handlers
		view(const std::string&, view_action, const std::string&, const empty_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const get_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const put_handler_t&) = delete;
		view(const std::string&, view_action, const std::string&, const delete_handler_t&) = delete;
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


extern const error_function_t default_error_function;
extern const view default_error_view;

// container of views(templates and handlers)
// TODO (maybe) extend it from tagd::tagspace so views can be stored in a tagspace
class viewspace : public tagd::errorable {
		view_map_t _views;
		tagd::config& _config; // borrowed

	public:
		viewspace(tagd::config& cfg) : _config{cfg} {
		}
		/*
		 * TODO add a flags variable to get(), put(), etc.
		 * such as F_DISABLE_ERROR_REPORTING
		 */

		tagd::code put(const view& vw) {
			if (vw.name() == DEFAULT_VIEW)
				return ferror(tagd::TS_MISUSE, "view name is reserved: %s", DEFAULT_VIEW.c_str());

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

		// Borrowed handlers remain owned by this viewspace; lookup never consumes them.
		const view* find(const view_id&) const;
		bool contains_name(const std::string&) const;
		// UNDEF in the result selects built-in TAGL, never an app handler.
		tagd::code resolve(view&, const view_id&);
		tagd::code validate_config();
		std::string fpath(const std::string& tpl_fname);
};

} // namespace httagd
