#include "httagd.h"

namespace httagd {

const char* view_action_str(view_action a) {
	switch (a) {
		case view_action::EMPTY: return "EMPTY";
		case view_action::GET: return "GET";
		case view_action::PUT: return "PUT";
		case view_action::DELETE: return "DELETE";
		case view_action::QUERY: return "QUERY";
		case view_action::ERROR: return "ERROR";
		case view_action::UNDEF: return "UNDEF";
		default: assert(0);
	}

	return "";
}

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

const view* viewspace::find(const view_id& id) const {
	const auto found = _views.find(id);
	return found == _views.end() ? nullptr : &found->second;
}

bool viewspace::contains_name(const std::string& name) const {
	// Keys sort by name, then action; UNDEF precedes every registered handler action.
	const auto found = _views.lower_bound(view_id(name, view_action::UNDEF));
	return found != _views.end() && found->first.name() == name;
}

tagd::code viewspace::resolve(view& result, const view_id& id) {
	result = view{};
	if (id.name() == DEFAULT_VIEW)
		return tagd::TAGD_OK;

	if (!contains_name(id.name()))
		return ferror(tagd::TS_NOT_FOUND, "view not found: %s", id.name().c_str());

	if (const auto* registered = find(id))
		result = *registered;

	return tagd::TAGD_OK;
}

tagd::code viewspace::validate_config() {
	const auto selected = _config.default_view();
	if (!selected.empty() && selected != DEFAULT_VIEW && !contains_name(selected))
		return ferror(tagd::TS_NOT_FOUND, "configured default view not found: %s", selected.c_str());

	const auto fallback = _config.fallback_error_view();
	if (!fallback.empty() && fallback != DEFAULT_VIEW && !find(error_view_id(fallback)))
		return ferror(tagd::TS_NOT_FOUND, "configured fallback error view not found: %s",
			fallback.c_str());

	return tagd::TAGD_OK;
}

std::string viewspace::fpath(const std::string& tpl_fname) {
	return tagd::io::concat_dir(_config.tpl_dir(), tpl_fname);
}

} // namespace httagd
