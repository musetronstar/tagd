#include "tagspace-config.h"

#include <charconv>
#include <filesystem>
#include <set>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace tagd {

// Typed getters require validated configuration; broken internal state cannot supply a value.
[[noreturn]] static void invalid_configuration(const char* message) {
	std::fprintf(stderr, "tagd configuration invariant: %s\n", message);
	std::abort();
}

namespace {
/*
 * Built-in settings and their application defaults. Every selected profile must
 * have an effective value for each one. Fixed settings bind runtime resources
 * (home, listener) and cannot change once runtime begins. Home's empty initial
 * value is a placeholder: init() and select() supply the bootstrap home.
 */
struct setting {
	id_view id;
	const char* initial;
	bool fixed;
};

const setting settings[] = {
	{HARD_TAG_CONFIG_TAGDURL_AUTOLOAD, HARD_TAG_FALSE.data(), false},
	{HARD_TAG_CONFIG_HOME, "", true},
	{HARD_TAG_CONFIG_LOG_LEVEL, "error", false},
	{HARD_TAG_CONFIG_BIND_ADDR, "localhost", true},
	{HARD_TAG_CONFIG_BIND_PORT, "2112", true},
	{HARD_TAG_CONFIG_TPL_DIR, "./app/tpl/", false},
	{HARD_TAG_CONFIG_WWW_DIR, "./app/www/", false},
	{HARD_TAG_CONFIG_FAVICON, "", false},
	{HARD_TAG_CONFIG_DEFAULT_VIEW, "", false},
	{HARD_TAG_CONFIG_PROMPT, "tagd> ", false},
	{HARD_TAG_CONFIG_ECHO_RESULT_CODE, HARD_TAG_TRUE.data(), false}
};

const setting* find_setting(id_view id) {
	for (const auto& s : settings) {
		if (s.id == id)
			return &s;
	}
	return nullptr;
}

bool valid_port(const std::string& value, uint16_t& port) {
	unsigned number = 0;
	auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
	if (ec != std::errc{} || end != value.data() + value.size() || number == 0
		|| number > std::numeric_limits<uint16_t>::max())
		return false;
	port = static_cast<uint16_t>(number);
	return true;
}
}

void config::prepare() {
	// Copy only this operation's private diagnostics into the caller's current list.
	// Retaining shared backend lists would mix errors from separate HTTP requests.
	_configspace.clear_errors();
	_configspace.report_errors = report_errors;
}

tagd::code config::finish(tagd::code rc) {
	if (rc != TAGD_OK)
		copy_errors(_configspace);
	return code(rc);
}

tagd::code config::init(const std::string& home) {
	_bootstrap_home = home;
	auto rc = _configspace.init();
	if (rc != TAGD_OK)
		return finish(rc);
	prepare();
	_logger.level(log_level::ERROR);

	abstract_tag defaults(CONFIG_SUBJECT, HARD_TAG_SUB, HARD_TAG_CONFIG, POS_TAG);
	for (const auto& s : settings) {
		rc = defaults.relation(HARD_TAG_HAS, s.id,
			s.id == HARD_TAG_CONFIG_HOME ? home : s.initial);
		if (rc != TAGD_OK)
			return error(rc, "invalid configuration default");
	}
	return finish(_configspace.put(defaults));
}

tagd::code config::begin_config_load() {
	if (_runtime || _file_loading)
		return error(TS_MISUSE, "configuration file loading already started or completed");

	_file_loading = true;
	_file_default_touched = false;
	_file_profile.clear();
	_file_context.clear();

	// Retain the familiar subject for bare PUTs, but no local defaults may mask ancestors.
	auto rc = _configspace.del(abstract_tag(CONFIG_SUBJECT));
	if (rc != TAGD_OK)
		return finish(rc);

	return finish(_configspace.put(abstract_tag(CONFIG_SUBJECT, HARD_TAG_SUB,
		HARD_TAG_CONFIG, POS_TAG)));
}

tagd::code config::finish_config_load(const std::string& cli_id, const std::string& home) {
	id_string selected;
	if (!_file_profile.empty()) {
		// Restore the SET's context IDs even if their tags were removed later in the file.
		tagspace_session ssn([](id_view) { return true; });
		for (const auto& id : _file_context) {
			auto rc = ssn.push_context(id);
			if (rc != TAGD_OK)
				return rc;
		}
		abstract_tag resolved;
		auto rc = _configspace.resolve_referents(resolved,
			abstract_tag(_file_profile), &ssn);
		if (rc != TAGD_OK)
			return finish(rc);
		selected = resolved.id();
		if (!_configspace.exists(selected)
			|| (selected == CONFIG_SUBJECT && !_file_default_touched))
			return error(TS_NOT_FOUND, "config profile not found: " + selected);
		if (selected.empty() || selected.front() == '_'
			|| !_configspace.contains(HARD_TAG_CONFIG, selected))
			return error(TS_MISUSE, "invalid config profile: " + selected);
	}

	if (!cli_id.empty())
		selected = cli_id;

	if (selected == CONFIG_SUBJECT && !_file_default_touched)
		return error(TS_NOT_FOUND, "config profile not defined by file: " + selected);

	if (selected.empty()) {
		std::vector<id_string> pending{std::string(HARD_TAG_CONFIG)};
		std::set<id_string> profiles;
		while (!pending.empty()) {
			auto parent = pending.back();
			pending.pop_back();
			tag_set children;
			auto rc = _configspace.query(children,
				interrogator(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, parent), nullptr,
				F_NO_TRANSFORM_REFERENTS);
			if (rc != TAGD_OK && rc != TS_NOT_FOUND)
				return finish(rc);
			for (const auto& child : children) {
				if (child.id().front() == '_' || child.id() == parent)
					continue;
				if (profiles.insert(child.id()).second)
					pending.push_back(child.id());
			}
		}
		if (!_file_default_touched)
			profiles.erase(std::string(CONFIG_SUBJECT));
		if (profiles.size() != 1) {
			std::string message = "select config profile with %% _config_id ID; or --config-id ID; profiles:";
			for (const auto& id : profiles)
				message += " " + id;
			return error(TS_MISUSE, message);
		}
		selected = *profiles.begin();
	}

	auto rc = select(selected, home);
	if (rc == TAGD_OK)
		_file_loading = false;
	return rc;
}

// After loading, validation keeps every built-in setting present; bootstrap reads use defaults.
std::string config::value(id_view id) const {
	abstract_tag current;
	if (_configspace.get(current, _subject_id, nullptr,
		F_NO_TRANSFORM_REFERENTS | F_INCLUDE_INHERITED_PREDICATES | F_NO_NOT_FOUND_ERROR) != TAGD_OK
		&& !_file_loading)
		invalid_configuration("configuration accessed before successful initialization");

	for (const auto& p : current.relations) {
		if (p.relator == HARD_TAG_HAS && p.object == id)
			return p.modifier;
	}
	if (_file_loading) {
		const auto* setting = find_setting(id);
		if (setting)
			return id == HARD_TAG_CONFIG_HOME ? _bootstrap_home : setting->initial;
	}
	invalid_configuration("required configuration value missing");
}

uint16_t config::bind_port() const {
	uint16_t port = 0;
	if (!valid_port(value(HARD_TAG_CONFIG_BIND_PORT), port))
		invalid_configuration("invalid stored configuration port");
	return port;
}

bool config::echo_result_code() const {
	return value(HARD_TAG_CONFIG_ECHO_RESULT_CODE) == HARD_TAG_TRUE;
}

tagd::code config::set(id_view id, const std::string& value) {
	abstract_tag change(_subject_id);
	auto rc = change.relation(HARD_TAG_HAS, id, value);
	if (rc != TAGD_OK)
		return error(rc, "invalid configuration override");
	return put(change, nullptr, F_IGNORE_DUPLICATES);
}

tagd::code config::select(id_view subject, const std::string& default_home) {
	if (_runtime)
		return error(TS_MISUSE, "configuration subject cannot change at runtime");
	if (!_configspace.exists(subject))
		return error(TS_NOT_FOUND, "config profile not found: " + std::string(subject));
	if (subject.empty() || subject.front() == '_'
		|| !_configspace.contains(HARD_TAG_CONFIG, subject))
		return error(TS_MISUSE, "unknown or invalid config subject: " + std::string(subject));

	abstract_tag inherited;
	auto rc = _configspace.get(inherited, subject, nullptr,
		F_NO_TRANSFORM_REFERENTS | F_INCLUDE_INHERITED_PREDICATES);
	if (rc != TAGD_OK)
		return finish(rc);

	// Fill only absent settings: local copies of inherited values would shadow future edits.
	abstract_tag missing(subject);
	for (const auto& s : settings) {
		if (!inherited.related(HARD_TAG_HAS, s.id)) {
			rc = missing.relation(HARD_TAG_HAS, s.id,
				s.id == HARD_TAG_CONFIG_HOME ? default_home : s.initial);
			if (rc != TAGD_OK)
				return error(rc, "invalid configuration default");
		}
	}
	if (!missing.relations.empty()) {
		rc = put(missing);
		if (rc != TAGD_OK)
			return rc;
	}

	// The selected profile's home and _use change collection invariants; restore on failure.
	auto previous = _subject_id;
	_subject_id = subject;
	rc = validate_collection(nullptr);
	if (rc != TAGD_OK) {
		_subject_id = std::move(previous);
		return rc;
	}

	refresh_sources();
	update_logger();
	return code(TAGD_OK);
}

namespace {
const std::string* modifier(const abstract_tag& t, id_view setting) {
	for (const auto& p : t.relations) {
		if (p.relator == HARD_TAG_HAS && p.object == setting)
			return &p.modifier;
	}
	return nullptr;
}

void overlay(abstract_tag& target, const predicate_set& relations, bool assertions = false) {
	// Stored ancestry overrides whole predicates; PUT preserves existing values on bare assertions.
	for (const auto& p : relations) {
		if (assertions && p.modifier.empty() && target.related(p.relator, p.object))
			continue;
		erase_if(target.relations, [&](const auto& existing) {
			return existing.relator == p.relator && existing.object == p.object;
		});
		target.relations.insert(p);
	}
}
}

/*
 * assertions: t is PUT input, where a bare predicate keeps any stored value, so
 * value checks apply only to supplied modifiers. Otherwise t is a complete
 * candidate tag and every setting value is checked.
 */
tagd::code config::validate(const abstract_tag& t, bool assertions) {
	/*
	 * TODO: when predicate IDs carry ranks, use containment matching for setting families
	 * (for example, objects contained by HARD_TAG_CONFIG_BIND_PORT). Resolving an
	 * alias to the same tag is separate from matching a descendant setting tag.
	 */
	if (t.pos() == POS_URL)
		return TAGD_OK;
	if (t.pos() != POS_TAG && t.pos() != POS_UNKNOWN && t.pos() != POS_RELATOR)
		return error(TS_MISUSE, "unsupported configuration subject");
	if (t.id().empty() || t.id().front() == '_')
		return error(TS_MISUSE, "configuration requires a mutable subject");

	// Setting rules apply only to profiles; descriptions and supporting tags are ordinary
	// TAGL here, and validate_collection() checks their tagspace invariants.
	bool profile = _configspace.contains(HARD_TAG_CONFIG, t.id())
		|| (!t.super_object().empty() && _configspace.contains(HARD_TAG_CONFIG, t.super_object()));
	if (!profile)
		return TAGD_OK;

	if ((!t.sub_relator().empty() && t.sub_relator() != HARD_TAG_SUB)
		|| (!t.super_object().empty()
			&& (!_configspace.contains(HARD_TAG_CONFIG, t.super_object())
				|| t.id() == t.super_object()
				|| _configspace.contains(t.id(), t.super_object()))))
		return error(TS_MISUSE, "configuration subjects must remain beneath _config");
	if (t.super_object().empty() && !_configspace.contains(HARD_TAG_CONFIG, t.id()))
		return error(TS_MISUSE, "unknown configuration subject");

	std::set<id_string> seen;
	for (const auto& p : t.relations) {
		// _use names a startup target, not a setting value; validate_collection() checks it.
		if (p.relator == HARD_TAG_USE) {
			if (p.opr8r != OP_EQ)
				return error(TS_MISUSE, "_use requires equality");
			continue;
		}

		const auto* s = find_setting(p.object);
		if (p.relator != HARD_TAG_HAS || !seen.insert(p.object).second || p.opr8r != OP_EQ
			|| (!s && (p.object == HARD_TAG_CONFIG
				|| !_configspace.contains(HARD_TAG_CONFIG, p.object))))
			return error(TS_MISUSE, "unknown or conflicting configuration setting");
		if (assertions && p.modifier.empty())
			continue;

		if (p.object == HARD_TAG_CONFIG_BIND_PORT) {
			uint16_t port;
			if (!valid_port(p.modifier, port))
				return error(TS_MISUSE, "bind port must be an integer from 1 to 65535");
		} else if (p.object == HARD_TAG_CONFIG_LOG_LEVEL) {
			logger candidate;
			if (p.modifier.empty() || !parse_log_level_spec(p.modifier, candidate))
				return error(TS_MISUSE, "invalid log level");
		} else if (p.object == HARD_TAG_CONFIG_ECHO_RESULT_CODE
			|| p.object == HARD_TAG_CONFIG_TAGDURL_AUTOLOAD) {
			if (p.modifier != HARD_TAG_TRUE && p.modifier != HARD_TAG_FALSE)
				return error(TS_MISUSE, "boolean setting requires _true or _false");
		} else if (p.object == HARD_TAG_CONFIG_HOME || p.object == HARD_TAG_CONFIG_BIND_ADDR) {
			if ((p.object == HARD_TAG_CONFIG_BIND_ADDR && p.modifier.empty())
				|| p.modifier.find('\0') != std::string::npos)
				return error(TS_MISUSE, "configuration value cannot be empty or contain NUL");
		} else if (p.object == HARD_TAG_CONFIG_TPL_DIR || p.object == HARD_TAG_CONFIG_WWW_DIR) {
			std::error_code ec;
			if (p.modifier != s->initial && !std::filesystem::is_directory(p.modifier, ec))
				return error(TS_MISUSE, "configuration directory does not exist");
		}
	}
	return TAGD_OK;
}

tagd::code config::validate_effective(const abstract_tag& candidate) {
	if (_file_loading || !_configspace.contains(candidate.id(), _subject_id))
		return TAGD_OK;

	/*
	 * Validate before storing: obtain the proposed parent's inherited values,
	 * then apply the candidate and the selected subject's nearer local overrides.
	 * This also checks reparenting without temporarily changing the live tagspace.
	 */
	std::vector<abstract_tag> descendants;
	id_string id = _subject_id;
	while (id != candidate.id()) {
		abstract_tag local;
		auto rc = _configspace.get(local, id, nullptr, F_NO_TRANSFORM_REFERENTS);
		if (rc != TAGD_OK)
			return finish(rc);
		id = local.super_object();
		descendants.push_back(std::move(local));
	}

	abstract_tag effective;
	auto rc = _configspace.get(effective, candidate.super_object(), nullptr,
		F_NO_TRANSFORM_REFERENTS | F_INCLUDE_INHERITED_PREDICATES);
	if (rc != TAGD_OK)
		return finish(rc);
	overlay(effective, candidate.relations);
	for (auto it = descendants.rbegin(); it != descendants.rend(); ++it) {
		overlay(effective, it->relations);
	}

	for (const auto& s : settings) {
		const auto* next = modifier(effective, s.id);
		if (!next)
			return error(TS_MISUSE, "required configuration values cannot be deleted");
		if (_runtime && s.fixed && *next != value(s.id))
			return error(TS_MISUSE, "setting cannot change at runtime: " + std::string(s.id));
	}
	return TAGD_OK;
}

void config::update_logger() {
	logger candidate;
	candidate.level(log_level::ERROR);
	if (!parse_log_level_spec(value(HARD_TAG_CONFIG_LOG_LEVEL), candidate))
		invalid_configuration("invalid validated logger configuration");
	_logger.levels(candidate);
}

tagd::code config::put(const abstract_tag& input, tagspace_session* ssn, flags_t flags) {
	reset(ssn);
	prepare();

	// Referent definitions change vocabulary, not the canonical configuration values.
	if (input.pos() == POS_REFERENT && !(flags & F_NO_POS_CAST)) {
		auto rc = validate_alias(referent(input));
		return rc == TAGD_OK ? finish(_configspace.put(referent(input), ssn, flags)) : rc;
	}

	abstract_tag t;
	auto resolved = _configspace.resolve_referents(t, input,
		(flags & F_NO_TRANSFORM_REFERENTS) ? nullptr : ssn);
	if (resolved != TAGD_OK)
		return finish(resolved);

	// Validate the input, then the stored tag as this PUT would leave it.
	auto rc = validate(t, true);
	if (rc == TAGD_OK) {
		abstract_tag existing;
		rc = _configspace.get(existing, t.id(), nullptr,
			F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR);
		if (rc == TAGD_OK || rc == TS_NOT_FOUND) {
			abstract_tag candidate(t.id(), HARD_TAG_SUB,
				t.super_object().empty() ? existing.super_object() : t.super_object(), POS_TAG);
			candidate.relations = std::move(existing.relations);
			overlay(candidate, t.relations, true);
			rc = validate(candidate);
			if (rc == TAGD_OK)
				rc = validate_effective(candidate);
			if (rc == TAGD_OK)
				rc = validate_collection(&candidate);
		}
	}
	if (rc != TAGD_OK) {
		if (ssn)
			ssn->code(rc);
		return code(rc);
	}

	prepare();
	// Apply exactly the canonical operation validated above; do not resolve twice.
	if (t.pos() == POS_URL) {
		// Storage's POS dispatch requires an actual URL object, not a sliced abstract_tag.
		url u(t.id());
		u.relations = t.relations;
		rc = _configspace.put(u, ssn, flags | F_NO_TRANSFORM_REFERENTS);
	} else {
		rc = _configspace.put(t, ssn, flags | F_NO_TRANSFORM_REFERENTS);
	}
	if (rc == TAGD_OK) {
		if (_file_loading && t.id() == CONFIG_SUBJECT)
			_file_default_touched = true;
		refresh_sources();
		update_logger();
	}
	return finish(rc);
}

tagd::code config::del(const abstract_tag& input, tagspace_session* ssn, flags_t flags) {
	reset(ssn);
	prepare();

	// Referent definitions change vocabulary, not the canonical configuration values.
	if (input.pos() == POS_REFERENT && !(flags & F_NO_POS_CAST))
		return finish(_configspace.del(referent(input), ssn, flags));

	abstract_tag t;
	auto resolved = _configspace.resolve_referents(t, input,
		(flags & F_NO_TRANSFORM_REFERENTS) ? nullptr : ssn);
	if (resolved != TAGD_OK)
		return finish(resolved);

	/*
	 * Hard tags, and whole-tag deletion of the selected profile's ancestry, are never
	 * deletable. DELETE names no new parent; URL tags carry their own subordinate
	 * relation, so theirs is permitted.
	 */
	auto rc = TAGD_OK;
	bool allowed = (t.pos() == POS_TAG || t.pos() == POS_UNKNOWN || t.pos() == POS_URL || t.pos() == POS_RELATOR)
		&& !t.id().empty() && t.id().front() != '_'
		&& (t.super_object().empty() || t.pos() == POS_URL);
	if (allowed && t.relations.empty() && !_file_loading)
		allowed = !_configspace.contains(t.id(), _subject_id);

	if (!allowed) {
		rc = error(TS_MISUSE, "required configuration values cannot be deleted");
	} else if (!t.relations.empty()) {
		// Preview storage semantics: a matching modifier clears only the value; bare DELETE
		// removes the whole predicate.
		abstract_tag candidate;
		rc = _configspace.get(candidate, t.id(), nullptr, F_NO_TRANSFORM_REFERENTS);
		if (rc == TAGD_OK) {
			for (const auto& p : t.relations) {
				// Match storage's deletion precondition before previewing its effect.
				if (!(p.modifier.empty() ? candidate.related(p.relator, p.object) : candidate.related(p)))
					continue;
				erase_if(candidate.relations, [&](const auto& existing) {
					return existing.relator == p.relator && existing.object == p.object;
				});
				if (!p.modifier.empty())
					candidate.relations.insert(predicate(p.relator, p.object));
			}
			rc = validate(candidate);
			if (rc == TAGD_OK)
				rc = validate_effective(candidate);
			if (rc == TAGD_OK)
				rc = validate_collection(&candidate);
		}
	}
	if (rc != TAGD_OK) {
		if (ssn)
			ssn->code(rc);
		return code(rc);
	}

	if (t.relations.empty()) {
		rc = validate_collection(&t, true);
		if (rc != TAGD_OK)
			return rc;
	}

	prepare();
	if (t.pos() == POS_URL) {
		url u(t.id());
		u.relations = t.relations;
		rc = _configspace.del(u, ssn, flags | F_NO_TRANSFORM_REFERENTS);
	} else {
		rc = _configspace.del(t, ssn, flags | F_NO_TRANSFORM_REFERENTS);
	}
	if (rc == TAGD_OK) {
		if (_file_loading && t.id() == CONFIG_SUBJECT)
			_file_default_touched = true;
		refresh_sources();
		update_logger();
	}
	return finish(rc);
}

tagd::code config::get(abstract_tag& t, id_view id, tagspace_session* ssn, flags_t flags) {
	prepare();
	return finish(_configspace.get(t, id, ssn, flags));
}

tagd::code config::query(tag_set& result, const interrogator& q, tagspace_session* ssn, flags_t flags) {
	prepare();
	return finish(_configspace.query(result, q, ssn, flags));
}

part_of_speech config::pos(id_view id, tagspace_session* ssn, flags_t flags) {
	prepare();
	auto result = _configspace.pos(id, ssn, flags);
	finish(_configspace.code());
	return result;
}

bool config::exists(id_view id, flags_t flags) const {
	return _configspace.exists(id, flags);
}

// The const tagspace API preserves stored data; errorable diagnostics are non-const.
// Keep that API contract rather than making all error state mutable for these reads.
tagd::code config::dump(std::ostream& os) const {
	auto& self = *const_cast<config*>(this);
	self.prepare();
	return self.finish(_configspace.dump(os));
}

tagd::code config::dump_grid(std::ostream& os) const {
	auto& self = *const_cast<config*>(this);
	self.prepare();
	return self.finish(_configspace.dump_grid(os));
}

tagd::code config::dump_terms(std::ostream& os) const {
	auto& self = *const_cast<config*>(this);
	self.prepare();
	return self.finish(_configspace.dump_terms(os));
}

tagd::code config::dump_search(std::ostream& os) const {
	auto& self = *const_cast<config*>(this);
	self.prepare();
	return self.finish(_configspace.dump_search(os));
}

} // namespace tagd
