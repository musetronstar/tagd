#include "tagspace-config.h"

#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <pwd.h>
#include <unistd.h>

namespace tagd {
namespace fs = std::filesystem;
namespace {
// Collection reads compare canonical IDs and treat absence as data, not as an error object.
constexpr flags_t canonical_lookup = F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR;

// Only SET path prefixes introduce locations; HDURI is accepted as a stored _from object.
bool location(const std::string& s) {
	return s.starts_with('/') || s.starts_with("./") || s.starts_with("../")
		|| s.starts_with("~/") || s.starts_with("file:");
}
}

/*
 * Source identity is an absolute, lexically normal path. With canonical set,
 * existing symlink prefixes are also resolved (weakly canonical), so equivalent
 * spellings of one file share an identity even before the file exists.
 */
tagd::code config::normalize_source(const std::string& input, const std::string& base,
	std::string& result, bool canonical) {
	std::string path = input;
	if (path.starts_with("file:") || path.starts_with("hd:")) {
		url u(path);
		if (!u.ok() || u.scheme() != "file" || !u.host().empty()
			|| !u.query().empty() || !u.fragment().empty())
			return error(TS_MISUSE, "source must be a local file URL");

		if (!uri_decode_checked(path, u.path()))
			return error(TS_MISUSE, "invalid file URL escape");
	}

	// ~/ means the operating-system user's home, independent of --home.
	if (path.starts_with("~/")) {
		const char* home = std::getenv("HOME");
		if (!home || !*home)
			return error(TS_MISUSE, "cannot expand user home");
		path = (fs::path(home) / path.substr(2)).string();
	}

	if (path.empty() || path.find('\0') != std::string::npos)
		return error(TS_MISUSE, "invalid source path");
	fs::path file(path);
	if (file.extension() != TAGSPACE_DB_SUFFIX && file.extension() != TAGSPACE_TAGL_SUFFIX)
		return error(TS_MISUSE, "source requires .db or .tagl extension");

	std::error_code ec;
	if (file.is_relative())
		file = (base.empty() ? fs::current_path(ec) : fs::path(base)) / file;
	if (!ec && canonical)
		file = fs::weakly_canonical(file, ec);
	if (ec)
		return error(TS_ERR, ec.message());

	result = file.lexically_normal().string();
	return TAGD_OK;
}

tagd::code config::source_path(const abstract_tag& t, std::string& path,
	const std::string* configured_home, bool canonical) {
	// Only the description's local _from counts; an inherited source is never used.
	std::string source;
	for (const auto& p : t.relations) {
		if (p.relator != HARD_TAG_FROM)
			continue;
		if (!source.empty() || !p.modifier.empty() || p.opr8r != OP_EQ)
			return error(TS_MISUSE, "one unmodified _from source required: " + t.id());
		url u(p.object);
		if (!u.ok() || u.scheme() != "file")
			return error(TS_MISUSE, "_from requires a file URL tag");
		source = p.object;
	}

	if (source.empty()) {
		const auto directory = configured_home ? *configured_home : home();
		if (directory.empty())
			return error(TS_MISUSE, "invalid tagd home: required for source: " + t.id());
		source = tagspace_database_path(directory, t.id());
	}

	return normalize_source(source, {}, path, canonical);
}

namespace {
// Child QUERY returns headers only; load local predicates and walk the hierarchy explicitly.
tagd::code subtree(tagspace& ts, id_view root, tag_set& result) {
	std::vector<id_string> pending{std::string(root)};
	std::set<id_string> visited;
	while (!pending.empty()) {
		auto parent = std::move(pending.back());
		pending.pop_back();
		if (parent == HARD_TAG_REFERENT || !visited.insert(parent).second)
			continue;

		tag_set children;
		interrogator q(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, parent);
		auto rc = ts.query(children, q, nullptr, canonical_lookup);
		if (rc != TAGD_OK && rc != TS_NOT_FOUND)
			return rc;

		for (const auto& child : children) {
			if (child.id() == parent)
				continue;
			abstract_tag local;
			if ((rc = ts.get(local, child.id(), nullptr, canonical_lookup)) != TAGD_OK)
				return rc;
			result.insert(local);
			pending.push_back(child.id());
		}
	}
	return TAGD_OK;
}

std::string file_url(const std::string& path) {
	std::string encoded;
	for (unsigned char c : path) {
		// Preserve URI unreserved bytes so ordinary file URLs share canonical spelling.
		bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
		encoded += (unreserved || c == '/') ? std::string(1, c) : uri_encode(std::string(1, c));
	}
	return "file://" + encoded;
}
}

tagd::code config::descriptions(tag_set& result) {
	return subtree(_configspace, HARD_TAG_TAGSPACE, result);
}

tagd::code config::validate_alias(const referent& r, bool planned) {
	if (_configspace.contains(HARD_TAG_TAGSPACE, r.refers()) && r.refers() != r.refers_to())
		return error(TS_MISUSE, "alias collides with tagspace: " + r.refers());

	// planned: the target description is about to be published, so treat it as registered.
	bool target = planned || r.refers_to() == HARD_TAG_DEFAULT || r.refers_to() == HARD_TAG_CONFIG
		|| _configspace.contains(HARD_TAG_TAGSPACE, r.refers_to());

	// Tagspace names are one namespace across contexts: no spelling may name two targets.
	tag_set refs;
	interrogator q(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, HARD_TAG_REFERENT);
	(void)q.relation(HARD_TAG_REFERS, r.refers());
	auto rc = _configspace.query(refs, q, nullptr, canonical_lookup);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	for (const auto& t : refs) {
		referent other(t);
		bool registered = other.refers_to() == HARD_TAG_DEFAULT || other.refers_to() == HARD_TAG_CONFIG
			|| _configspace.contains(HARD_TAG_TAGSPACE, other.refers_to());
		if ((target || registered) && other.refers_to() != r.refers_to())
			return error(TS_MISUSE, "conflicting alias: " + r.refers());
	}

	if (target) {
		if (!valid_tagspace_name(r.refers()))
			return error(TS_MISUSE, "invalid or reserved alias: " + r.refers());
		if (_configspace.exists(r.refers()) && r.refers() != r.refers_to())
			return error(TS_MISUSE, "alias collides with tag: " + r.refers());
	}
	return TAGD_OK;
}

/*
 * Validate the whole proposed _config state: stored tags with the candidate
 * inserted, replaced, or deleted. Invariants span subjects (ancestry, sources,
 * names), so checking the candidate alone could miss a conflict it creates.
 */
tagd::code config::validate_collection(const abstract_tag* candidate, bool deleting, bool opening) {
	tag_set all;
	auto rc = subtree(_configspace, HARD_TAG_ENTITY, all);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	std::map<id_string, abstract_tag> tags;
	for (const auto& t : all)
		tags.emplace(t.id(), t);
	if (candidate) {
		if (deleting)
			tags.erase(candidate->id());
		else
			tags.insert_or_assign(candidate->id(), *candidate);
	}

	// Evaluate the proposed hierarchy, including descendants affected by reparenting.
	auto beneath = [&](id_string id, id_view parent) {
		std::set<id_string> visited;
		while (id != parent && visited.insert(id).second) {
			auto it = tags.find(id);
			if (it == tags.end())
				return false;
			id = it->second.super_object();
		}
		return id == parent;
	};

	// Effective home belongs to the proposed profile, not the pre-mutation storage.
	std::string proposed_home = home();
	std::set<id_string> ancestry;
	for (id_string id = _subject_id; !_home_disabled && ancestry.insert(id).second;) {
		auto it = tags.find(id);
		if (it == tags.end())
			break;
		bool found = false;
		for (const auto& p : it->second.relations) {
			if (p.relator == HARD_TAG_HAS && p.object == HARD_TAG_CONFIG_HOME) {
				proposed_home = p.modifier;
				found = true;
				break;
			}
		}
		if (found)
			break;
		id = it->second.super_object();
	}

	// Each source has one description, and an open instance keeps its source.
	std::map<std::string, id_string> sources;
	std::set<id_string> registered;
	for (const auto& [id, t] : tags) {
		if (id == HARD_TAG_TAGSPACE || !beneath(id, HARD_TAG_TAGSPACE))
			continue;
		if (!valid_tagspace_name(id))
			return error(TS_MISUSE, "invalid tagspace name: " + id);
		registered.insert(id);

		std::string path;
		std::string lexical;
		if ((rc = source_path(t, lexical, &proposed_home, false)) != TAGD_OK)
			return rc;
		// Unchanged descriptions retain identity even if their symlink disappears.
		// Opening, unlike a descriptive edit, rechecks today's filesystem mapping.
		if (!opening && _locations.contains(id) && _locations.at(id) == lexical
			&& _sources.contains(id))
			path = _sources.at(id);
		else if ((rc = normalize_source(lexical, {}, path)) != TAGD_OK)
			return rc;

		auto [it, inserted] = sources.emplace(path, id);
		if (!inserted && it->second != id)
			return error(TS_MISUSE, "source collision: " + id + " and " + it->second);
		if (_opened.contains(id) && (!_sources.contains(id) || _sources.at(id) != path))
			return error(TS_MISUSE, "close before changing source: " + id);
	}

	for (const auto& [id, t] : tags) {
		if (id.front() == '_' || !beneath(id, HARD_TAG_CONFIG))
			continue;
		// An ancestry chain may declare _use once; no declaration means the reserved _default.
		size_t count = 0;
		id_string current = id;
		id_string target(HARD_TAG_DEFAULT);
		std::set<id_string> visited;
		while (visited.insert(current).second) {
			auto it = tags.find(current);
			if (it == tags.end())
				break;
			for (const auto& pred : it->second.relations) {
				if (pred.relator != HARD_TAG_USE)
					continue;
				if (++count > 1)
					return error(TS_MISUSE, "one _use declaration permitted per config ancestry: " + id);
				target = pred.object;
				if (target != HARD_TAG_DEFAULT && target != HARD_TAG_CONFIG
					&& !registered.contains(target))
					return error(TS_MISUSE, "_use requires a tagspace description: " + target);
				if (!pred.modifier.empty()) {
					rc = validate_alias(referent(pred.modifier, target, HARD_TAG_CONFIG), true);
					if (rc != TAGD_OK)
						return rc;
				}
			}
			current = it->second.super_object();
		}
		if (id == _subject_id && _frozen_target && target != *_frozen_target)
			return error(TS_MISUSE, "server startup target cannot change at runtime");
	}

	// An open instance must keep its description until it is closed.
	for (const auto& [id, ptr] : _opened) {
		if (id.front() != '_' && !registered.contains(id))
			return error(TS_MISUSE, "close before removing description: " + id);
	}

	// Tagspace IDs and aliases share one namespace with every other _config tag.
	tag_set refs;
	interrogator rq(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, HARD_TAG_REFERENT);
	rc = _configspace.query(refs, rq, nullptr, canonical_lookup);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	std::map<id_string, id_string> names;
	for (const auto& id : registered)
		names.emplace(id, id);
	for (const auto& t : refs) {
		referent r(t);
		if (r.refers_to() != HARD_TAG_DEFAULT && r.refers_to() != HARD_TAG_CONFIG
			&& !registered.contains(r.refers_to()))
			continue;
		if (!valid_tagspace_name(r.refers())
			|| (tags.contains(r.refers()) && r.refers() != r.refers_to()))
			return error(TS_MISUSE, "tagspace alias collides with tag: " + r.refers());
		auto [it, added] = names.emplace(r.refers(), r.refers_to());
		if (!added && it->second != r.refers_to())
			return error(TS_MISUSE, "conflicting alias: " + r.refers());
	}

	// Check existing ordinary referents too, so insertion order cannot conceal a name conflict.
	for (const auto& t : refs) {
		referent r(t);
		auto it = names.find(r.refers());
		if (it != names.end() && it->second != r.refers_to())
			return error(TS_MISUSE, "conflicting referent: " + r.refers());
	}
	return TAGD_OK;
}

tagd::code config::resolve_tagspace(const std::string& name, id_string& id,
	tagspace_session* ssn) {
	if (name == HARD_TAG_DEFAULT || name == HARD_TAG_CONFIG) {
		id = name;
		return TAGD_OK;
	}
	if (!valid_tagspace_name(name))
		return error(TS_MISUSE, "invalid tagspace name: " + name);

	// The fallback is local to this lookup; never alter the caller's context stack.
	auto lookup = _configspace.get_session();
	(void)lookup.push_context(HARD_TAG_CONFIG);
	if (ssn) {
		for (const auto& c : ssn->context()) {
			if (_configspace.exists(c))
				(void)lookup.push_context(c);
		}
	}

	abstract_tag resolved;
	auto rc = _configspace.resolve_referents(resolved, abstract_tag(name), &lookup);
	if (rc != TAGD_OK)
		return finish(rc);
	id = resolved.id();
	if (id != HARD_TAG_DEFAULT && id != HARD_TAG_CONFIG
		&& !_configspace.contains(HARD_TAG_TAGSPACE, id))
		return code(TS_NOT_FOUND);
	return code(TAGD_OK);
}

tagd::code config::bind_alias(id_view id, const std::string& alias) {
	if (alias.empty())
		return TAGD_OK;

	referent r(alias, id, HARD_TAG_CONFIG);
	auto rc = validate_alias(r);
	if (rc != TAGD_OK)
		return rc;
	return finish(_configspace.put(r, nullptr, F_IGNORE_DUPLICATES));
}

std::shared_ptr<tagspace> config::borrow(tagspace* tsp) {
	for (const auto& [id, ptr] : _opened) {
		if (ptr.get() == tsp)
			return ptr;
	}

	// Config and stack-owned tagspaces remain owned by their application or test scope.
	// Aliasing an empty owner preserves a borrowed address without installing weak_from_this.
	return std::shared_ptr<tagspace>(std::shared_ptr<tagspace>{}, tsp);
}

id_string config::tagspace_id(const tagspace* ts) const {
	if (ts == this)
		return std::string(HARD_TAG_CONFIG);
	for (const auto& [id, opened] : _opened) {
		if (opened.get() == ts)
			return id;
	}
	return {};
}

tagd::code config::open_tagspace(const std::string& target, const std::string& alias,
	std::shared_ptr<tagspace>& result, tagspace_session* ssn, const std::string& base,
	tagspace_access access, source_loader loader, bool* denied,
	source_action action, const std::string& expected_id) {
	clear_errors();
	if (denied)
		*denied = false;
	// Policy denials are reported separately so callers can distinguish them from bad input.
	auto prohibit = [&](const char* message) {
		if (denied)
			*denied = true;
		return error(TS_MISUSE, message);
	};

	// Resolve the target: a location reuses the description owning that source;
	// a name resolves through referents, or names an undeclared home-layout target.
	id_string id;
	std::string path;
	bool declared = true;
	auto rc = TAGD_OK;
	std::string spelling;
	if (location(target)) {
		if (access != tagspace_access::local)
			return prohibit("execution cannot open filesystem locations");
		if ((rc = normalize_source(target, base, spelling, false)) != TAGD_OK)
			return rc;
		if ((rc = normalize_source(target, base, path)) != TAGD_OK)
			return rc;

		tag_set tags;
		if ((rc = descriptions(tags)) != TAGD_OK)
			return finish(rc);
		for (const auto& t : tags) {
			std::string existing;
			if ((rc = source_path(t, existing)) != TAGD_OK)
				return rc;
			if (existing == path) {
				if (!id.empty() && id != t.id())
					return error(TS_MISUSE, "source collision: " + id + " and " + t.id());
				id = t.id();
			}
		}

		if (id.empty()) {
			id = fs::path(path).stem().string();
			declared = false;
		}
	} else {
		rc = resolve_tagspace(target, id, ssn);
		if (rc == TS_NOT_FOUND) {
			if (access == tagspace_access::registered_only
				|| access == tagspace_access::registered_with_config)
				return error(TS_NOT_FOUND, "unregistered tagspace: " + target);
			id = target;
			declared = false;
		} else if (rc != TAGD_OK) {
			return rc;
		}
	}

	// A filename cannot impersonate an application-owned reserved tagspace.
	if (!declared && !valid_tagspace_name(id))
		return error(TS_MISUSE, "invalid or reserved tagspace name: " + id);

	if (!expected_id.empty() && expected_id != id)
		return error(TS_MISUSE, "--tagspace must match database description: " + id);
	if (action == source_action::create && (id.front() == '_' || _opened.contains(id)))
		return error(TS_DUPLICATE, "tagspace exists: " + id);

	// Check the resolved target before binding: aliases cannot grant access to config.
	if (id == HARD_TAG_CONFIG && (access == tagspace_access::registered_only
		|| access == tagspace_access::named_only))
		return prohibit("config is not the server default");
	if (!alias.empty() && (rc = validate_alias(referent(alias, id, HARD_TAG_CONFIG), true)) != TAGD_OK)
		return rc;

	// Reserved targets are source-free: config is never owned here; _default opens on first use.
	if (id == HARD_TAG_CONFIG) {
		if ((rc = bind_alias(id, alias)) == TAGD_OK)
			result = borrow(this);
		return rc;
	}
	if (id == HARD_TAG_DEFAULT && action == source_action::register_only)
		return bind_alias(id, alias);
	if (id == HARD_TAG_DEFAULT && !_opened.contains(id)) {
		auto mem = std::make_shared<tagspace::memory>();
		if ((rc = mem->init()) != TAGD_OK)
			return rc;
		_opened.emplace(id, std::move(mem));
	}

	if (!declared && _configspace.exists(id))
		return error(TS_MISUSE, "description ID collision: " + id);
	if (_opened.contains(id)) {
		// An existing instance does not need its source again; a supplied path must still agree.
		if (!path.empty() && _sources.at(id) != path)
			return error(TS_MISUSE, "source changed while open: " + id);
		if ((rc = bind_alias(id, alias)) == TAGD_OK)
			result = _opened.at(id);
		if (rc == TAGD_OK && !spelling.empty())
			_spellings[spelling] = id;
		return rc;
	}

	abstract_tag description(id, HARD_TAG_SUB, HARD_TAG_TAGSPACE, POS_TAG);
	if (declared) {
		if ((rc = _configspace.get(description, id, nullptr, canonical_lookup)) != TAGD_OK)
			return finish(rc);
	} else if (!valid_tagspace_name(id) || _configspace.exists(id)) {
		return error(TS_MISUSE, "description ID collision: " + id);
	}
	if (declared && action == source_action::register_only)
		return bind_alias(id, alias);
	if (path.empty() && (rc = source_path(description, path)) != TAGD_OK)
		return rc;
	if (access != tagspace_access::local && fs::path(path).extension() == TAGSPACE_TAGL_SUFFIX)
		return prohibit("execution cannot load a closed TAGL source");

	std::error_code ec;
	if (action != source_action::create && !fs::is_regular_file(path, ec))
		return error(TS_NOT_FOUND, "no such tagspace source: " + path);
	if (!_loading.insert(path).second)
		return error(TS_MISUSE, "recursive source load: " + path);
	struct loading_guard {
		std::set<std::string>& loading;
		std::string path;
		~loading_guard() { loading.erase(path); }
	} guard{_loading, path};

	url source(file_url(path));
	if (!declared)
		(void)description.relation(HARD_TAG_FROM, source.hduri());
	if ((rc = validate_collection(&description, false, true)) != TAGD_OK)
		return rc;

	// A .tagl source executes into a private memory instance that is published only on success.
	std::shared_ptr<tagspace> opened;
	if (action == source_action::register_only) {
		// Registering the tagspace tag does not require opening its database or executing TAGL.
		rc = TAGD_OK;
	} else if (fs::path(path).extension() == TAGSPACE_DB_SUFFIX) {
		auto db = std::make_shared<tagspace::persistent>();
		rc = action == source_action::create ? db->create_file(path) : db->open_file(path);
		opened = db;
	} else {
		auto mem = std::make_shared<tagspace::memory>();
		rc = mem->init();
		opened = mem;
		if (rc == TAGD_OK)
			rc = loader ? loader(path, mem) : error(TS_MISUSE, "TAGL source loader required");
	}
	if (rc != TAGD_OK) {
		copy_errors(*opened);
		return code(rc);
	}

	// A source script may change names or sources; never overwrite those earlier effects.
	if (!alias.empty() && (rc = validate_alias(referent(alias, id, HARD_TAG_CONFIG), true)) != TAGD_OK)
		return rc;
	if (!declared && _configspace.exists(id))
		return error(TS_MISUSE, "description created during source load: " + id);
	if (declared) {
		abstract_tag current;
		std::string current_path;
		if (_configspace.get(current, id, nullptr, canonical_lookup) != TAGD_OK
			|| source_path(current, current_path) != TAGD_OK || current_path != path)
			return error(TS_MISUSE, "description changed during source load: " + id);
		description = current;
	}
	if ((rc = validate_collection(&description, false, true)) != TAGD_OK)
		return rc;

	bool source_existed = _configspace.exists(source.hduri());
	if (!declared) {
		rc = _configspace.put(source, nullptr, F_IGNORE_DUPLICATES);
		if (rc != TAGD_OK)
			return finish(rc);
		rc = put(description);
		if (rc != TAGD_OK) {
			if (!source_existed)
				(void)_configspace.del(source);
			return code(rc);
		}
	}
	if ((rc = bind_alias(id, alias)) != TAGD_OK) {
		// Undo only this publication; successful statements inside the script survive.
		if (!declared) {
			(void)_configspace.del(abstract_tag(id));
			if (!source_existed)
				(void)_configspace.del(source);
			refresh_sources();
		}
		return code(rc);
	}

	_sources[id] = path;
	if (!spelling.empty())
		_spellings[spelling] = id;
	if (opened)
		_opened.emplace(id, opened);
	result = std::move(opened);
	return code(TAGD_OK);
}

tagd::code config::open_database(const std::string& input, bool create,
	const std::string& expected_id, std::string& resolved) {
	auto rc = normalize_source(input, {}, resolved);
	if (rc != TAGD_OK)
		return rc;
	if (fs::path(resolved).extension() != TAGSPACE_DB_SUFFIX)
		return error(TS_MISUSE, "--db requires a .db source");

	std::shared_ptr<tagspace> opened;
	return open_tagspace(resolved, "", opened, nullptr, {}, tagspace_access::local,
		{}, nullptr, create ? source_action::create : source_action::open, expected_id);
}

tagd::code config::close_tagspace(const std::string& target, const std::string& alias,
	tagspace_session* ssn, const std::string& base) {
	clear_errors();

	// UNSET never opens storage: an unknown target is already closed.
	id_string id;
	if (location(target)) {
		std::string path;
		auto rc = normalize_source(target, base, path, false);
		if (rc != TAGD_OK)
			return rc;

		auto retained = _spellings.find(path);
		if (retained != _spellings.end())
			id = retained->second;
		for (const auto& [name, source] : _sources) {
			if (path == source)
				id = name;
		}

		if (id.empty()) {
			// Cached spellings survive disappearance; a new equivalent spelling needs filesystem lookup.
			std::error_code ec;
			auto canonical = fs::weakly_canonical(path, ec).string();
			if (!ec) {
				for (const auto& [name, source] : _sources) {
					if (canonical == source)
						id = name;
				}
			}
		}
		if (id.empty())
			return code(TAGD_OK);
	} else {
		auto rc = resolve_tagspace(target, id, ssn);
		if (rc == TS_NOT_FOUND)
			return code(TAGD_OK);
		if (rc != TAGD_OK)
			return rc;
	}

	// A qualified UNSET removes only the _config-context alias, never the instance.
	if (!alias.empty()) {
		auto rc = validate_alias(referent(alias, id, HARD_TAG_CONFIG), true);
		if (rc != TAGD_OK)
			return rc;
		rc = _configspace.del(referent(alias, id, HARD_TAG_CONFIG), nullptr, canonical_lookup);
		return code(rc == TS_NOT_FOUND ? TAGD_OK : rc);
	}

	if (id == HARD_TAG_CONFIG || id == HARD_TAG_DEFAULT)
		return error(TS_MISUSE, "cannot close reserved tagspace");
	auto it = _opened.find(id);
	if (it == _opened.end())
		return code(TAGD_OK);
	// One owner means only the collection remains; every additional shared owner is a borrower.
	if (it->second.use_count() != 1)
		return error(TS_MISUSE, "tagspace is borrowed: " + id);
	_opened.erase(it);
	return code(TAGD_OK);
}

// Call after description or effective-home mutations. Alias-only writes cannot change source identity.
void config::refresh_sources() {
	tag_set tags;
	if (descriptions(tags) != TAGD_OK)
		return;

	std::map<id_string, std::string> locations;
	for (const auto& t : tags) {
		std::string path;
		if (source_path(t, path, nullptr, false) != TAGD_OK)
			continue;
		locations.emplace(t.id(), path);
		// Re-resolve only changed locations; unchanged ones keep identity if their file vanished.
		if (!_locations.contains(t.id()) || _locations.at(t.id()) != path) {
			_sources.erase(t.id());
			std::erase_if(_spellings, [&](const auto& entry) { return entry.second == t.id(); });
			std::string resolved;
			if (normalize_source(path, {}, resolved) == TAGD_OK)
				_sources[t.id()] = resolved;
		}
		_spellings[path] = t.id();
	}

	// Drop index entries for descriptions that no longer exist.
	std::erase_if(_sources, [&](const auto& entry) { return !locations.contains(entry.first); });
	std::erase_if(_spellings, [&](const auto& entry) { return !locations.contains(entry.second); });
	_locations = std::move(locations);
}

tagd::code config::list_tagspaces(std::ostream& os, const tagspace* active) {
	tag_set tags;
	auto rc = descriptions(tags);
	if (rc != TAGD_OK)
		return finish(rc);

	std::map<id_string, std::string> rows;
	rows[std::string(HARD_TAG_CONFIG)] = "reserved/open";
	rows[std::string(HARD_TAG_DEFAULT)] = _opened.contains(std::string(HARD_TAG_DEFAULT))
		? "reserved/open" : "reserved/closed";
	for (const auto& t : tags)
		rows[t.id()] = _opened.contains(t.id()) ? "registered/open" : "registered/closed";

	// Discovery is deliberately shallow: directory names are unverified, not registrations.
	std::error_code ec;
	if (!home().empty()) {
		fs::directory_iterator entries(tagspaces_directory(home()), ec), end;
		for (; !ec && entries != end; entries.increment(ec)) {
			auto name = entries->path().filename().string();
			if (valid_tagspace_name(name) && entries->is_directory(ec))
				rows.try_emplace(name, "unregistered/unverified");
		}
	}

	tag_set refs;
	interrogator q(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, HARD_TAG_REFERENT);
	rc = _configspace.query(refs, q, nullptr, canonical_lookup);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	std::map<id_string, std::set<id_string>> aliases;
	for (const auto& t : refs) {
		referent r(t);
		if (rows.contains(r.refers_to()))
			aliases[r.refers_to()].insert(r.refers());
	}

	for (const auto& [id, state] : rows) {
		bool selected = (id == HARD_TAG_CONFIG && active == this)
			|| (_opened.contains(id) && _opened.at(id).get() == active);
		os << (selected ? "* " : "  ") << id << '\t' << state;
		for (const auto& alias : aliases[id])
			os << '\t' << alias;
		os << '\n';
	}
	return code(TAGD_OK);
}

tagd::code config::startup_target(id_string& target, bool aliases) {
	abstract_tag profile;
	auto rc = get(profile, _subject_id, nullptr, canonical_lookup | F_INCLUDE_INHERITED_PREDICATES);
	if (rc != TAGD_OK)
		return rc;

	// The ancestry invariant permits at most one effective _use; none selects _default.
	target = HARD_TAG_DEFAULT;
	for (const auto& pred : profile.relations) {
		if (pred.relator == HARD_TAG_USE) {
			target = pred.object;
			if (aliases)
				return bind_alias(target, pred.modifier);
		}
	}
	return code(TAGD_OK);
}

tagd::code config::freeze_target() {
	id_string target;
	auto rc = startup_target(target, false);
	if (rc == TAGD_OK)
		_frozen_target = target;
	return rc;
}

tagd::code config::create_tagspace(const std::string& name, std::shared_ptr<tagspace>& result) {
	id_string id;
	auto rc = resolve_tagspace(name, id);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return rc;
	if (rc == TS_NOT_FOUND)
		id = name;
	if (id == HARD_TAG_DEFAULT || id == HARD_TAG_CONFIG)
		return error(TS_MISUSE, "reserved tagspace name");

	// --create makes only home-layout databases; a registered description must agree.
	abstract_tag description(id, HARD_TAG_SUB, HARD_TAG_TAGSPACE, POS_TAG);
	if (_configspace.exists(id) && !_configspace.contains(HARD_TAG_TAGSPACE, id))
		return error(TS_MISUSE, "description ID collision: " + id);
	if (_configspace.get(description, id, nullptr, canonical_lookup) == TAGD_OK) {
		std::string source, expected;
		if ((rc = source_path(description, source)) != TAGD_OK)
			return rc;
		abstract_tag derived(id, HARD_TAG_SUB, HARD_TAG_TAGSPACE, POS_TAG);
		if ((rc = source_path(derived, expected)) != TAGD_OK)
			return rc;
		if (source != expected)
			return error(TS_MISUSE, "--create requires a home-layout database");
	}
	if (description.id().empty())
		description = abstract_tag(id, HARD_TAG_SUB, HARD_TAG_TAGSPACE, POS_TAG);
	if ((rc = validate_collection(&description)) != TAGD_OK)
		return rc;

	auto db = std::make_shared<tagspace::persistent>();
	rc = db->create(id, home());
	if (rc != TAGD_OK) {
		copy_errors(*db);
		return code(rc);
	}

	// Reopen through the collection so creation and SET share description/source identity.
	db.reset();
	return open_tagspace(id, "", result);
}

} // namespace tagd
