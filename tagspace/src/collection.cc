#include "tagspace-config.h"

#include <filesystem>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

namespace tagd {
namespace fs = std::filesystem;
namespace {
constexpr flags_t raw = F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR;
bool location(const std::string& s) {
	return s.starts_with('/') || s.starts_with("./") || s.starts_with("../")
		|| s.starts_with("~/") || s.starts_with("file:");
}
}

tagd::code config::normalize_source(const std::string& input, const std::string& base,
	std::string& result) {
	std::string path = input;
	if (path.starts_with("file:") || path.starts_with("hd:")) {
		url u(path);
		if (!u.ok() || u.scheme() != "file" || !u.host().empty()
			|| !u.query().empty() || !u.fragment().empty())
			return error(TS_MISUSE, "source must be a local file URL");
		path = uri_decode(u.path());
	}
	if (path.starts_with("~/")) {
		const char* home = std::getenv("HOME");
		if (!home || !*home) {
			auto* pw = getpwuid(getuid());
			home = pw ? pw->pw_dir : nullptr;
		}
		if (!home || !*home)
			return error(TS_MISUSE, "cannot expand user home");
		path = (fs::path(home) / path.substr(2)).string();
	}
	if (path.empty() || path.find('\0') != std::string::npos)
		return error(TS_MISUSE, "invalid source path");
	fs::path file(path);
	if (file.extension() != ".db" && file.extension() != ".tagl")
		return error(TS_MISUSE, "source requires .db or .tagl extension");
	std::error_code ec;
	if (file.is_relative())
		file = (base.empty() ? fs::current_path(ec) : fs::path(base)) / file;
	if (!ec)
		file = fs::weakly_canonical(file, ec);
	if (ec)
		return error(TS_ERR, ec.message());
	result = file.string();
	return TAGD_OK;
}

tagd::code config::source_path(const abstract_tag& t, std::string& path) {
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
		if (home().empty())
			return error(TS_MISUSE, "home required for source: " + t.id());
		source = (fs::path(home()) / "tagspaces" / t.id() / (t.id() + ".db")).string();
	}
	return normalize_source(source, {}, path);
}

namespace {
// Child QUERY returns headers only; load local predicates and walk the hierarchy explicitly.
tagd::code subtree(tagspace& space, id_view root, tag_set& result) {
	std::vector<id_string> pending{std::string(root)};
	std::set<id_string> visited;
	while (!pending.empty()) {
		auto parent = std::move(pending.back());
		pending.pop_back();
		if (parent == HARD_TAG_REFERENT || !visited.insert(parent).second)
			continue;
		tag_set children;
		interrogator q(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, parent);
		auto rc = space.query(children, q, nullptr, raw);
		if (rc != TAGD_OK && rc != TS_NOT_FOUND)
			return rc;
		for (const auto& child : children) {
			if (child.id() == parent)
				continue;
			abstract_tag local;
			if ((rc = space.get(local, child.id(), nullptr, raw)) != TAGD_OK)
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
		encoded += c == '/' ? "/" : uri_encode(std::string(1, c));
	}
	return "file://" + encoded;
}
}

tagd::code config::descriptions(tag_set& result) {
	return subtree(_configspace, HARD_TAG_TAGSPACE, result);
}

tagd::code config::validate_alias(const referent& r, bool planned) {
	bool target = planned || r.refers_to() == HARD_TAG_DEFAULT || r.refers_to() == HARD_TAG_CONFIG
		|| _configspace.contains(HARD_TAG_TAGSPACE, r.refers_to());
	tag_set refs;
	interrogator q(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, HARD_TAG_REFERENT);
	(void)q.relation(HARD_TAG_REFERS, r.refers());
	auto rc = _configspace.query(refs, q, nullptr, raw);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	for (const auto& t : refs) {
		referent other(t);
		bool registered = _configspace.contains(HARD_TAG_TAGSPACE, other.refers_to());
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

tagd::code config::validate_collection(const abstract_tag* candidate, bool deleting) {
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
	std::map<std::string, id_string> sources;
	std::set<id_string> registered;
	for (const auto& [id, t] : tags) {
		if (id == HARD_TAG_TAGSPACE || !beneath(id, HARD_TAG_TAGSPACE))
			continue;
		if (!valid_tagspace_name(id))
			return error(TS_MISUSE, "invalid tagspace name: " + id);
		registered.insert(id);
		std::string path;
		if ((rc = source_path(t, path)) != TAGD_OK)
			return rc;
		auto [it, inserted] = sources.emplace(path, id);
		if (!inserted && it->second != id)
			return error(TS_MISUSE, "source collision: " + id + " and " + it->second);
		if (_opened.contains(id) && _sources[id] != path)
			return error(TS_MISUSE, "close before changing source: " + id);
	}
	for (const auto& [id, ptr] : _opened) {
		if (id.front() != '_' && !registered.contains(id))
			return error(TS_MISUSE, "close before removing description: " + id);
	}
	tag_set refs;
	interrogator rq(HARD_TAG_INTERROGATOR, HARD_TAG_SUB, HARD_TAG_REFERENT);
	rc = _configspace.query(refs, rq, nullptr, raw);
	if (rc != TAGD_OK && rc != TS_NOT_FOUND)
		return finish(rc);
	std::map<id_string, id_string> names;
	for (const auto& id : registered)
		names.emplace(id, id);
	for (const auto& t : refs) {
		referent r(t);
		if (!registered.contains(r.refers_to()))
			continue;
		if (!valid_tagspace_name(r.refers())
			|| (tags.contains(r.refers()) && r.refers() != r.refers_to()))
			return error(TS_MISUSE, "tagspace alias collides with tag: " + r.refers());
		auto [it, added] = names.emplace(r.refers(), r.refers_to());
		if (!added && it->second != r.refers_to())
			return error(TS_MISUSE, "conflicting alias: " + r.refers());
	}
	for (const auto& t : refs) {
		referent r(t);
		auto it = names.find(r.refers());
		if (it != names.end() && it->second != r.refers_to())
			return error(TS_MISUSE, "conflicting referent: " + r.refers());
	}
	return TAGD_OK;
}

tagd::code config::resolve_space(const std::string& name, id_string& id,
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
	if (!_configspace.contains(HARD_TAG_TAGSPACE, id))
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
	// Legacy callers and config itself retain external ownership; no self-owning cycle.
	return std::shared_ptr<tagspace>(tsp, [](tagspace*) {});
}

tagd::code config::open_space(const std::string& target, const std::string& alias,
	std::shared_ptr<tagspace>& result, tagspace_session* ssn, const std::string& base,
	bool http, source_loader loader) {
	clear_errors();
	id_string id;
	std::string path;
	bool declared = true;
	auto rc = TAGD_OK;
	if (location(target)) {
		if (http)
			return error(TS_MISUSE, "HTTP cannot open filesystem locations");
		if ((rc = normalize_source(target, base, path)) != TAGD_OK)
			return rc;
		tag_set tags;
		if ((rc = descriptions(tags)) != TAGD_OK)
			return finish(rc);
		for (const auto& t : tags) {
			std::string existing;
			if ((rc = source_path(t, existing)) != TAGD_OK)
				return rc;
			if (existing == path)
				id = t.id();
		}
		if (id.empty()) {
			id = fs::path(path).stem().string();
			declared = false;
		}
	} else {
		rc = resolve_space(target, id, ssn);
		if (rc == TS_NOT_FOUND) {
			if (http)
				return error(TS_NOT_FOUND, "unregistered tagspace: " + target);
			id = target;
			declared = false;
		} else if (rc != TAGD_OK) {
			return rc;
		}
	}
	if (!alias.empty() && (rc = validate_alias(referent(alias, id, HARD_TAG_CONFIG), true)) != TAGD_OK)
		return rc;
	if (id == HARD_TAG_CONFIG) {
		if ((rc = bind_alias(id, alias)) == TAGD_OK)
			result = borrow(this);
		return rc;
	}
	if (id == HARD_TAG_DEFAULT && !_opened.contains(id)) {
		auto mem = std::make_shared<tagspace::memory>();
		if ((rc = mem->init()) != TAGD_OK)
			return rc;
		_opened.emplace(id, std::move(mem));
	}
	if (_opened.contains(id)) {
		if ((rc = bind_alias(id, alias)) == TAGD_OK)
			result = _opened.at(id);
		return rc;
	}
	abstract_tag description(id, HARD_TAG_SUB, HARD_TAG_TAGSPACE, POS_TAG);
	if (declared) {
		if ((rc = _configspace.get(description, id, nullptr, raw)) != TAGD_OK)
			return finish(rc);
	} else if (!valid_tagspace_name(id) || _configspace.exists(id)) {
		return error(TS_MISUSE, "description ID collision: " + id);
	}
	if (path.empty() && (rc = source_path(description, path)) != TAGD_OK)
		return rc;
	if (http && fs::path(path).extension() == ".tagl")
		return error(TS_MISUSE, "HTTP cannot load a closed TAGL source");
	std::error_code ec;
	if (!fs::is_regular_file(path, ec))
		return error(TS_NOT_FOUND, "missing source: " + path);
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
	if ((rc = validate_collection(&description)) != TAGD_OK)
		return rc;
	std::shared_ptr<tagspace> opened;
	if (fs::path(path).extension() == ".db") {
		auto db = std::make_shared<tagspace::persistent>();
		rc = db->open_file(path);
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
	// A source script may have changed declarations; validate again before publication.
	if ((rc = validate_collection(&description)) != TAGD_OK)
		return rc;
	if (!declared) {
		rc = _configspace.put(source, nullptr, F_IGNORE_DUPLICATES);
		if (rc != TAGD_OK)
			return finish(rc);
		rc = put(description);
		if (rc != TAGD_OK)
			return rc;
	}
	if ((rc = bind_alias(id, alias)) != TAGD_OK)
		return rc;
	_sources[id] = path;
	_opened.emplace(id, opened);
	result = std::move(opened);
	return code(TAGD_OK);
}

tagd::code config::close_space(const std::string& target, const std::string& alias,
	tagspace_session* ssn, const std::string& base) {
	clear_errors();
	id_string id;
	if (location(target)) {
		std::string path;
		auto rc = normalize_source(target, base, path);
		if (rc != TAGD_OK)
			return rc;
		for (const auto& [name, source] : _sources) {
			if (path == source)
				id = name;
		}
		if (id.empty())
			return code(TAGD_OK);
	} else {
		auto rc = resolve_space(target, id, ssn);
		if (rc == TS_NOT_FOUND)
			return code(TAGD_OK);
		if (rc != TAGD_OK)
			return rc;
	}
	if (!alias.empty()) {
		auto rc = validate_alias(referent(alias, id, HARD_TAG_CONFIG), true);
		if (rc != TAGD_OK)
			return rc;
		rc = _configspace.del(referent(alias, id, HARD_TAG_CONFIG), nullptr, raw);
		return code(rc == TS_NOT_FOUND ? TAGD_OK : rc);
	}
	if (id == HARD_TAG_CONFIG || id == HARD_TAG_DEFAULT)
		return error(TS_MISUSE, "cannot close reserved tagspace");
	auto it = _opened.find(id);
	if (it == _opened.end())
		return code(TAGD_OK);
	if (it->second.use_count() != 1)
		return error(TS_MISUSE, "tagspace is borrowed: " + id);
	_opened.erase(it);
	return code(TAGD_OK);
}

} // namespace tagd
