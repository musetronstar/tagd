#include "tagspace.h"
#include "tagdb.h"
#include "filepile-records.h"
#include "filepile-files.h"
#include <set>
#include <sstream>

namespace tagd {
namespace {

using filepile::association;

code associations(errorable& errors, std::vector<association>& output,
	tagdb::tagdb& db, id_view relator, id_view object) {
	interrogator query(HARD_TAG_WHAT, HARD_TAG_SUB, HARD_TAG_LINK_FILEPILE);
	auto rc = query.relation(relator, object);
	if (rc != TAGD_OK)
		return rc;
	tag_set records;
	rc = db.query(records, query, nullptr, filepile::canonical_flags);
	if (rc == TS_NOT_FOUND)
		return TAGD_OK;
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot read filepile associations");
	std::set<std::pair<id_string, unsigned>> revisions;
	for (const auto& record : records) {
		if (record.id() == HARD_TAG_LINK_FILEPILE)
			continue;
		abstract_tag link;
		rc = db.get(link, record.id(), nullptr, filepile::canonical_flags);
		if (rc != TAGD_OK)
			return errors.error(rc, "cannot read filepile association: " + record.id());
		// Predicate queries include inheritance; only explicit endpoints authorize cleanup.
		if (!link.related(relator, object))
			continue;
		association item;
		rc = filepile::read_association(errors, item, link);
		if (rc != TAGD_OK)
			return rc;
		if (!revisions.emplace(item.source, item.revision).second)
			return errors.error(TS_ERR, "duplicate retained revision: " + item.link);
		url source, stored;
		rc = db.get(source, item.source, nullptr, filepile::canonical_flags);
		if (rc != TAGD_OK)
			return errors.error(rc, "cannot read filepile source: " + item.source);
		rc = db.get(stored, item.stored, nullptr, filepile::canonical_flags);
		if (rc != TAGD_OK)
			return errors.error(rc, "cannot read stored filepile record: " + item.stored);
		if (!stored.type_relations.contains({HARD_TAG_TYPE_OF, HARD_TAG_FILE_STORED}))
			return errors.error(TS_ERR, "filepile target lacks managed classification: " + item.stored);

		std::string name;
		rc = filepile::stored_address(errors, name, stored);
		if (rc != TAGD_OK)
			return rc;
		output.push_back(std::move(item));
	}
	return TAGD_OK;
}

const char* cleanup_name(cleanup_outcome value) {
	switch (value) {
		case cleanup_outcome::retained: return "retained";
		case cleanup_outcome::removed: return "removed";
		case cleanup_outcome::already_missing: return "already missing";
		case cleanup_outcome::failed: return "failed";
		case cleanup_outcome::not_attempted: return "not attempted";
	}
	return "unknown";
}

}

code stored_tagspace::delete_storage(filepile_delete_result& result, const abstract_tag& input,
	tagspace_session* ssn, flags_t flags) {
	result = {};
	if (!(flags & F_NO_RESET))
		reset(ssn);
	if (!prepare())
		return code();
	static session fallback;
	// One occurrence identity describes initiation and its final cleanup results.
	event initiated(ssn ? *ssn : fallback, "filepile", id_string(HARD_TAG_FILEPILE_DELETE_EVENT));
	auto& db = backend();
	std::vector<association> selected;
	const auto rc = [&]() -> tagd::code {
		if (input.pos() == POS_REFERENT || !input.relations.empty() || !input.type_relations.empty()) {
			const auto status = del(input, ssn, flags);
			result.committed = status == TAGD_OK;
			return status;
		}
		if (input.pos() != POS_URL)
			return error(TS_MISUSE, "whole-subject managed DELETE requires a URL");
		url requested(input.id());
		if (!requested.ok())
			return error(requested.code(), "invalid managed DELETE URL: " + input.id());
		// Match STORE's local spelling without imposing resource rules on ordinary DELETE.
		errorable parse_errors;
		parse_errors.report_errors = false;
		url normalized;
		std::string path;
		if (filepile::canonical_source(parse_errors, normalized, path, requested) == TAGD_OK)
			requested = std::move(normalized);
		filepile::record_roles roles;
		auto status = filepile::classify(roles, db, requested.hduri());
		if (status != TAGD_OK)
			return error(status, "cannot check managed DELETE classification: " + requested.id());
		bool source_role = false;
		interrogator sources(HARD_TAG_WHAT, HARD_TAG_SUB, HARD_TAG_LINK_FILEPILE);
		status = sources.relation(HARD_TAG_FROM, requested.hduri());
		if (status != TAGD_OK)
			return status;
		tag_set matches;
		status = db.query(matches, sources, nullptr, filepile::canonical_flags);
		if (status != TAGD_OK && status != TS_NOT_FOUND)
			return error(status, "cannot check filepile source references: " + requested.id());
		for (const auto& match : matches) {
			abstract_tag explicit_link;
			status = db.get(explicit_link, match.id(), nullptr, filepile::canonical_flags);
			if (status != TAGD_OK)
				return status;
			if (explicit_link.related(HARD_TAG_FROM, requested.hduri())) {
				association item;
				status = filepile::read_association(*this, item, explicit_link);
				if (status != TAGD_OK)
					return status;
				source_role = true;
			}
		}
		if (!source_role && !roles.stored) {
			// Bare !!+ is permitted for B; absence is not an ordinary-write refusal.
			if (has_filepile_scheme(requested.id()))
				return error(TS_NOT_FOUND, "no such stored file: " + requested.id());

			status = del(input, ssn, flags);
			result.committed = status == TAGD_OK;
			return status;
		}
		if (!_persistent || _filepile_home.empty())
			return error(TS_MISUSE, "managed DELETE requires a persistent tagspace and filepile home");

		status = db.prepare_delete();
		if (status != TAGD_OK)
			return status;

		if (!_filepile_home_error.empty())
			return error(TS_NOT_AVAILABLE, _filepile_home_error);

		// TODO(filepile-foreign-tagspaces): Local references suffice only for one participating
		// tagspace. See TASKS.d/2026-10-28-foreign-tagspaces.md before sharing a filepile.
		status = associations(*this, selected, db,
			source_role ? HARD_TAG_FROM : HARD_TAG_TO, requested.hduri());
		if (status != TAGD_OK)
			return status;
		if (selected.empty())
			return error(TS_ERR, "stored file has no managed association: " + requested.id());

		std::set<id_string> candidates;
		std::vector<id_string> ids;
		if (source_role)
			ids.push_back(requested.hduri());
		for (const auto& link : selected) {
			ids.push_back(link.link);
			candidates.insert(link.stored);
		}
		if (!source_role) {
			// Reverse !!+ A: remove a source only when no retained link survives !!+ B.
			std::set<id_string> sources;
			for (const auto& link : selected) {
				sources.insert(link.source);
			}

			for (const auto& source : sources) {
				std::vector<association> history;
				status = associations(*this, history, db, HARD_TAG_FROM, source);
				if (status != TAGD_OK)
					return status;

				bool retained = false;
				for (const auto& link : history) {
					retained |= link.stored != requested.hduri();
				}
				if (!retained)
					ids.push_back(source);
			}
		}

		for (const auto& id : candidates) {
			bool shared = false;
			if (source_role) {
				std::vector<association> references;
				status = associations(*this, references, db, HARD_TAG_TO, id);
				if (status != TAGD_OK)
					return status;
				for (const auto& link : references)
					shared |= link.source != requested.hduri();
			}
			cleanup_result content;
			content.stored = url(id);
			if (shared)
				content.outcome = cleanup_outcome::retained;
			else
				ids.push_back(id);
			result.content.push_back(std::move(content));
		}
		status = db.del_batch(ids, ssn);
		if (status != TAGD_OK) {
			result.commit_unknown = db.commit_unknown();
			return status;
		}
		result.committed = true;
		/*
		 * TODO(tagd-io): Coordinate STORE from commit through unlink before allowing concurrent writers.
		 * TODO(filepile-foreign-tagspaces): Cleanup relies on all users being in this tagspace.
		 * Check cross-tagspace references first; TASKS.d/2026-10-28-foreign-tagspaces.md.
		 */
		for (auto& content : result.content) {
			if (content.outcome == cleanup_outcome::retained)
				continue;
			content.code = filepile::remove_content(*this, content, _filepile_canonical_home, content.stored);
			if (content.code != TAGD_OK) {
				if (content.outcome != cleanup_outcome::removed)
					content.outcome = cleanup_outcome::failed;
				if (status == TAGD_OK)
					status = content.code;
			} else if (content.outcome == cleanup_outcome::already_missing) {
				result.warnings.emplace_back(TS_NOT_FOUND, "stored content already missing: " + content.stored.id());
				std::ostringstream warning;
				warning << result.warnings.back();
				TAGSPACE_LOG(log_level::WARNING, warning.str());
			}
		}
		return status;
	}();

	// Frontends must report the committed state even when event logging is disabled.
	if (rc != TAGD_OK && result.committed)
		error(rc, "filepile deletion committed; physical cleanup incomplete: " + input.id());

	std::ostringstream message;
	message << "DELETE " << input.id() << "; result=" << code_str(rc) << "; changed="
		<< (result.commit_unknown ? "unknown" : result.committed ? "true" : "false");
	for (const auto& link : selected)
		message << "; A=" << url(link.source).id() << " L=" << link.link << " B=" << url(link.stored).id();
	for (const auto& content : result.content)
		message << "; " << content.stored.id() << ": " << cleanup_name(content.outcome)
			<< " (" << code_str(content.code) << ')';
	filepile::log_event(initiated, rc, message.str());
	if (ssn)
		ssn->code(rc);
	return code(rc);
}

}
