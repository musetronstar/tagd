#include "tagspace.h"
#include "filepile-records.h"
#include <filesystem>
#include <charconv>
#include <sstream>
#include <array>

namespace tagd::filepile {

id_string canonical_id(id_view input) {
	const std::string text(input);
	if (url::looks_like_url(text) || url::looks_like_hduri(text)) {
		url value(input);
		if (value.ok())
			return value.hduri();
	}
	return id_string(input);
}

tagd::code link_id(errorable& errors, std::string& id, const url& source, unsigned revision) {
	if (revision < 1 || revision > last_revision)
		return errors.error(TS_ERR, "filepile content revision must be between 1 and 999: " + source.id());
	const auto digits = std::to_string(revision);
	id = "link:" + source.hduri() + '#' + std::string(3 - digits.size(), '0') + digits;
	if (id.size() > MAX_TAG_LEN)
		return errors.error(TS_ERR_MAX_TAG_LEN, "filepile link ID maximum of " + std::to_string(MAX_TAG_LEN) + " bytes exceeded");
	return TAGD_OK;
}

code read_association(errorable& errors, association& item, const abstract_tag& link) {
	item = {};
	item.link = link.id();
	unsigned from_count = 0, to_count = 0, revision_count = 0;
	for (const auto& p : link.relations) {
		if (p.relator == HARD_TAG_FROM || p.relator == HARD_TAG_TO) {
			if (!p.modifier.empty())
				return errors.error(TS_ERR, "filepile endpoint has a modifier: " + item.link);
			if (p.relator == HARD_TAG_FROM) {
				item.source = p.object;
				++from_count;
			} else {
				item.stored = p.object;
				++to_count;
			}
		}
		if (p.relator == HARD_TAG_HAS && p.object == HARD_TAG_REVISION) {
			++revision_count;
			const auto parsed = std::from_chars(p.modifier.data(),
				p.modifier.data() + p.modifier.size(), item.revision);
			if (parsed.ec != std::errc{} || parsed.ptr != p.modifier.data() + p.modifier.size() ||
				p.modifier_type != TYPE_INTEGER || p.opr8r != OP_EQ)
				return errors.error(TS_ERR, "invalid filepile revision: " + item.link);
		}
	}
	if (from_count != 1 || to_count != 1 || revision_count != 1 ||
		item.revision < 1 || item.revision > last_revision)
		return errors.error(TS_ERR, "incomplete filepile association: " + item.link);
	const url source(item.source), stored(item.stored);
	if (!source.ok() || !stored.ok() || source.hduri() != item.source || stored.hduri() != item.stored)
		return errors.error(TS_ERR, "filepile endpoints must be canonical URL identities: " + item.link);

	// STORE allocation and DELETE selection must interpret the same source/revision identity.
	std::string expected;
	const auto rc = link_id(errors, expected, source, item.revision);
	if (rc != TAGD_OK)
		return rc;
	if (item.link != expected || link.super_object() != HARD_TAG_LINK_FILEPILE)
		return errors.error(TS_ERR, "inconsistent filepile association identity: " + item.link);
	return TAGD_OK;
}

code classify(record_roles& result, tagdb::tagdb& db, id_view input) {
	result = {};
	const auto id = canonical_id(input);
	const std::array<id_view, 3> targets{HARD_TAG_FILE_STORED, HARD_TAG_LINK_FILEPILE,
		HARD_TAG_FILEPILE_EVENT};
	std::set<id_string> inherited;
	const auto rc = db.inherited_sources(inherited, id, targets);
	if (rc != TAGD_OK)
		return rc;
	result.stored = inherited.contains(id_string(HARD_TAG_FILE_STORED));
	result.link = inherited.contains(id_string(HARD_TAG_LINK_FILEPILE));
	result.event = inherited.contains(id_string(HARD_TAG_FILEPILE_EVENT));
	return TAGD_OK;
}

void log_event(event& ev, code result, const std::string& message) {
	const auto level = result == TAGD_OK ? log_level::NOTICE : log_level::ERROR;
	if (!TAGSPACE_LOG_ENABLED(level))
		return;

	const auto rc = ev.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, message);
	if (rc != TAGD_OK) {
		TAGSPACE_LOG(log_level::ERROR, "cannot describe filepile event: " + std::string(code_str(rc)));
		return;
	}

	std::ostringstream output;
	if (ev.time_code() != TAGD_OK) {
		TAGSPACE_LOG(level, "event timestamp unavailable: " + std::string(code_str(ev.time_code()))
			+ "; " + message);
		return;
	}
	// The logger's ordinary event overload emits only the identity; retain the predicates.
	output << ">> " << ev << ';';
	TAGSPACE_LOG(level, output.str());
}

void log_event(session* ssn, id_view type, code result, const std::string& message) {
	const auto level = result == TAGD_OK ? log_level::NOTICE : log_level::ERROR;
	if (!TAGSPACE_LOG_ENABLED(level))
		return;
	static session fallback;
	event ev(ssn ? *ssn : fallback, "filepile", id_string(type));
	log_event(ev, result, message);
}

}

namespace tagd {
namespace {

bool url_component(const predicate& p) {
	if (p.relator != HARD_TAG_HAS)
		return false;
	for (auto object : {HARD_TAG_SCHEME, HARD_TAG_USER, HARD_TAG_PASS, HARD_TAG_HOST,
		HARD_TAG_PORT, HARD_TAG_PATH, HARD_TAG_QUERY, HARD_TAG_FRAGMENT,
		HARD_TAG_PUBLIC, HARD_TAG_PRIV_LABEL, HARD_TAG_SUBDOMAIN}) {
		if (p.object == object)
			return true;
	}
	return false;
}

}

code stored_tagspace::managed_path(bool& result, const url& value, bool resolve) {
	result = false;
	auto scheme = value.scheme();
	auto host = value.host();
	for (auto* text : {&scheme, &host}) {
		for (char& c : *text) {
			if (c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
		}
	}
	if (scheme != "file" || (!host.empty() && host != "localhost"))
		return TAGD_OK;
	if (resolve && !_filepile_home_error.empty())
		return error(TS_NOT_AVAILABLE, _filepile_home_error);
	if (_filepile_canonical_home.empty() && _filepile_configured_root.empty())
		return TAGD_OK;

	std::string path;
	if (!uri_decode_checked(path, value.path()))
		return error(TS_MISUSE, "invalid encoded file path: " + value.id());

	std::filesystem::path candidate(path);
	if (resolve) {
		std::error_code ec;
		candidate = std::filesystem::weakly_canonical(candidate, ec);
		if (ec)
			return error(TS_NOT_AVAILABLE, "cannot resolve file URL: " + path + ": " + ec.message());
	} else {
		// Ordinary PUT declares an identifier; it must not inspect the resource or its aliases.
		candidate = candidate.lexically_normal();
	}
	const auto canonical_root = _filepile_canonical_home.empty() ? std::string{}
		: (std::filesystem::path(_filepile_canonical_home) / "filepile").string();
	for (const auto& root : {canonical_root, _filepile_configured_root}) {
		if (root.empty())
			continue;
		const auto relative = candidate.lexically_relative(root);
		result |= !relative.empty() && *relative.begin() != "..";
	}
	return TAGD_OK;
}

code stored_tagspace::guard_mutation(const abstract_tag& input, bool deleting,
	tagspace_session* ssn, flags_t flags) {
	if (!(flags & F_NO_RESET))
		reset(ssn);
	if (!prepare())
		return code();
	// Referent declarations edit name bindings, never the referred tag or its filepile facts.
	if (input.pos() == POS_REFERENT && !(flags & F_NO_POS_CAST))
		return code(TAGD_OK);

	auto& db = backend();
	abstract_tag resolved(input);
	if (input.pos() != POS_REFERENT && !(flags & F_NO_TRANSFORM_REFERENTS)) {
		const auto rc = db.resolve_referents(resolved, input, ssn);
		if (rc != TAGD_OK)
			return rc;
	}

	filepile::record_roles roles;
	auto rc = filepile::classify(roles, db, resolved.id());
	if (rc != TAGD_OK)
		return rc;
	url identity(resolved.id());
	const bool is_url = identity.ok();
	const bool event_id = resolved.id().starts_with(EVURI_SCHEME) || resolved.id().starts_with(ERRURI_SCHEME);
	const bool whole = resolved.relations.empty() && resolved.type_relations.empty();
	if (deleting && whole && (roles.stored || roles.link))
		return error(TS_DEPENDENCY, "managed filepile records require !!+ URL: " + resolved.id());

	if (!deleting) {
		bool managed = false;
		if (is_url) {
			rc = managed_path(managed, identity);
			if (rc != TAGD_OK)
				return rc;
		}
		if (managed || roles.stored)
			return error(TS_MISUSE, "cannot directly PUT a managed filepile URL: " + resolved.id());

		std::vector<id_string> objects{resolved.super_object()};
		for (const auto& p : resolved.type_relations)
			objects.push_back(p.object);
		for (const auto& object : objects) {
			filepile::record_roles target;
			rc = filepile::classify(target, db, object);
			if (rc != TAGD_OK)
				return rc;
			if (target.stored || target.link || (event_id && target.event))
				return error(TS_MISUSE, "filepile classification is system-maintained: " + object);
		}
		if ((roles.link || (event_id && roles.event)) && !resolved.super_object().empty())
			return error(TS_MISUSE, "filepile record identity is system-maintained");
	}

	if (is_url && !deleting && !resolved.super_object().empty()
		&& (resolved.super_object() != identity.super_object() || resolved.sub_relator() != identity.sub_relator()))
		return error(TS_MISUSE, "URL syntax determines its subordinate relation");

	if (deleting && (roles.stored || (event_id && roles.event)) && !resolved.type_relations.empty()) {
		for (const auto& p : resolved.type_relations) {
			filepile::record_roles target;
			rc = filepile::classify(target, db, p.object);
			if (rc != TAGD_OK)
				return rc;
			if (target.stored || (event_id && target.event))
				return error(TS_MISUSE, "filepile classification is system-maintained");
		}
	}

	for (const auto& p : resolved.relations) {
		const bool link_field = roles.link && (p.relator == HARD_TAG_FROM || p.relator == HARD_TAG_TO
			|| (p.relator == HARD_TAG_HAS && (p.object == HARD_TAG_REVISION
				|| p.object == HARD_TAG_MTIME || p.object == HARD_TAG_STORED_TIME)));
		const bool stored_field = roles.stored && p.relator == HARD_TAG_HAS
			&& (p.object == HARD_TAG_HASH_SHA256 || p.object == HARD_TAG_FILE_SIZE
				|| p.object == HARD_TAG_MEDIA_TYPE || p.object == HARD_TAG_MTIME);
		const bool event_field = roles.event && event_id && p.relator == HARD_TAG_HAS
			&& p.object == HARD_TAG_MESSAGE;
		if ((is_url && url_component(p)) || link_field || stored_field || event_field)
			return error(TS_MISUSE, "cannot change system-maintained predicate: " + resolved.id());
	}
	return code(TAGD_OK);
}

}
