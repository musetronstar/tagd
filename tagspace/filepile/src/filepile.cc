#include "tagspace.h"
#include "tagdb.h"
#include "filepile-files.h"
#include "filepile-records.h"
#include "tagd/datetime.h"
#include "tagd/magic.h"
#include <filesystem>
#include <set>

namespace tagd {
namespace {

constexpr flags_t canonical_flags = F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR;

struct history {
	unsigned revision = 0;
	id_string link;
	url stored;
	std::string name, hash;
};

tagd::code latest(errorable& errors, history& result, tagdb::tagdb& db,
	const url& source, const std::string& home, tagspace_session* ssn) {
	tagd::code rc;
	if (!db.exists(source.hduri()))
		return TAGD_OK;
	// TODO(tagd-io): Reading and allocating per-source revisions is not concurrency safe.
	// TODO(filepile-integrity): History and content lookups depend on backend predicate-query performance.
	interrogator query(HARD_TAG_WHAT, HARD_TAG_SUB, HARD_TAG_LINK_FILEPILE);
	rc = query.relation(HARD_TAG_FROM, source.hduri());
	if (rc != TAGD_OK)
		return rc;
	tag_set links;
	rc = db.query(links, query, ssn, canonical_flags);
	if (rc == TS_NOT_FOUND)
		return TAGD_OK;
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot query filepile history");
	for (const auto& entry : links) {
		abstract_tag link;
		rc = db.get(link, entry.id(), ssn, canonical_flags);
		if (rc != TAGD_OK)
			return errors.error(rc, "cannot read filepile link: " + entry.id());
		// Retained associations are explicit system facts, not inherited descriptions.
		if (!link.related(HARD_TAG_FROM, source.hduri()))
			continue;
		filepile::association association;
		rc = filepile::read_association(errors, association, link);
		if (rc != TAGD_OK)
			return rc;
		if (association.revision > result.revision) {
			result.revision = association.revision;
			result.link = association.link;
			result.stored = url(association.stored);
		}
	}
	if (result.revision) {
		rc = filepile::stored_address(errors, result.name, home, result.stored);
		if (rc != TAGD_OK)
			return rc;
		result.hash = result.name.substr(0, filepile::digest_length);
		rc = db.get(result.stored, result.stored.id(), ssn, canonical_flags);
		if (rc != TAGD_OK)
			return errors.error(rc, "missing stored filepile record");
		if (result.stored.super_object() != HARD_TAG_FILE ||
			!result.stored.type_relations.contains({HARD_TAG_TYPE_OF, HARD_TAG_FILE_STORED}))
			return errors.error(TS_ERR, "incompatible stored-file classification: " + result.stored.id());
	}
	return TAGD_OK;
}

tagd::code verify_recorded_content(errorable& errors, std::set<std::string>& verified,
	tagdb::tagdb& db, filepile::files& content, tagspace_session* ssn) {
	tagd::code rc;
	interrogator query(HARD_TAG_WHAT);
	rc = query.type_relation(HARD_TAG_TYPE_OF, HARD_TAG_FILE_STORED);
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot construct stored-file classification query");
	rc = query.relation(HARD_TAG_HAS, HARD_TAG_HASH_SHA256, content.digest);
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot construct filepile content query");

	tag_set records;
	rc = db.query(records, query, ssn, canonical_flags);
	if (rc == TS_NOT_FOUND)
		return TAGD_OK;
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot query recorded filepile content");

	for (const auto& record : records) {
		url canonical;
		std::string path;
		const auto parsed = filepile::canonical_source(errors, canonical, path, url(record.id()));
		if (parsed != TAGD_OK)
			return parsed;
		const std::string name = std::filesystem::path(path).filename();

		// Metadata narrows lookup; the URL-derived address remains the integrity authority.
		if (!name.starts_with(content.digest + '.'))
			continue;
		if (path != content.destination(name))
			return errors.error(TS_ERR, "stored filepile reference is outside the selected home: " + canonical.id());

		// This operation-local result includes durability; reuse must not hash the same object twice.
		const auto rc = content.verify(name, content.digest, true);
		if (rc != TAGD_OK)
			return rc;
		verified.insert(name);
	}

	return TAGD_OK;
}

tagd::code metadata(errorable& errors, abstract_tag& tag, id_view object,
	const std::string& value, data_t type = TYPE_STRING) {
	const auto rc = tag.relation(HARD_TAG_HAS, object, value, OP_EQ, type);
	if (rc != TAGD_OK)
		return errors.error(rc, "cannot construct filepile metadata");
	return TAGD_OK;
}

} // namespace

tagd::code stored_tagspace::store(store_result& result, const abstract_tag& input,
	tagspace_session* ssn, flags_t flags) {
	result = {};
	if (!(flags & F_NO_RESET))
		reset(ssn);
	filepile::files content;
	bool published = false;
	bool semantic_attempted = false;
	std::string attempted_association;
	// All operation exits pass through explicit cleanup; destructors only release resources.
	const auto operation = [&]() -> tagd::code {
		tagd::code rc;
		if (!_persistent)
			return tagspace::store(result, input, ssn, flags);
		if (input.pos() != POS_URL)
			return error(TS_MISUSE, "STORE requires a POS_URL subject");
		rc = guard_mutation(input, false, ssn, flags);
		if (rc != TAGD_OK)
			return rc;
		if (_filepile_home.empty())
			return error(TS_MISUSE, "a tagd home directory is required for filepile storage. " + _home_reason);
		if (!prepare())
			return error(code(), "cannot prepare tagspace for filepile storage");
		auto& db = backend();
		if (!db.supports_atomic_put())
			return error(TS_NOT_IMPLEMENTED, "backend does not support atomic filepile storage");
		rc = db.prepare_store();
		if (rc != TAGD_OK)
			return error(rc, "backend cannot establish durable filepile storage");
		url canonical;
		std::string source_path;
		rc = filepile::canonical_source(*this, canonical, source_path, url(input.id()));
		if (rc != TAGD_OK)
			return rc;
		bool managed = false;
		rc = managed_path(managed, canonical, true);
		if (rc != TAGD_OK)
			return rc;
		if (managed)
			return error(TS_MISUSE, "cannot STORE a managed filepile URL: " + canonical.id());

		abstract_tag resolved(input);
		if (!(flags & F_NO_TRANSFORM_REFERENTS)) {
			rc = db.resolve_referents(resolved, input, ssn);
			if (rc != TAGD_OK)
				return rc;
		}
		canonical.relations = resolved.relations;
		canonical.type_relations = resolved.type_relations;
		// Validate the caller's PUT against pre-command state before any file acquisition.
		semantic_attempted = true;
		const auto put_rc = db.check_put(canonical,
			(flags & ~F_NO_POS_CAST) | F_NO_TRANSFORM_REFERENTS);
		if (put_rc != TAGD_OK && put_rc != TS_DUPLICATE)
			return put_rc;

		std::string first_link;
		rc = filepile::link_id(*this, first_link, canonical, 1);
		if (rc != TAGD_OK)
			return rc;
		history previous;
		rc = latest(*this, previous, db, canonical,
			_filepile_canonical_home, ssn);
		if (rc != TAGD_OK)
			return rc;

		rc = content.init(_filepile_canonical_home, canonical);
		if (rc != TAGD_OK)
			return rc;
		rc = content.capture();
		if (rc != TAGD_OK)
			return rc;

		magic detector;
		magic_result detection;
		if (detector.detect(detection, content.sample) != TAGD_OK) {
			copy_errors(detector);
			return error(detector.code(), "filepile content detection failed");
		}
		store_result output;
		output.source = canonical;
		if (content.initial.st_size == 0) {
			tagd::error warning(TS_ERR);
			(void)warning.relation(HARD_TAG_CAUSED_BY, content.source.hduri());
			(void)warning.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, "warning: source file is empty (0 bytes)");
			output.warnings.push_back(std::move(warning));
		}

		std::string name;
		if (previous.revision) {
			// A needed historical address must exist; another suffix cannot silently repair it.
			if (filepile::path_url(content.destination(previous.name)) != previous.stored.id())
				return error(TS_ERR, "stored filepile reference is outside the selected home: " + previous.stored.id());
			rc = content.verify(previous.name, previous.hash);
			if (rc != TAGD_OK)
				return rc;
			rc = content.discover(name);
			if (rc != TAGD_OK)
				return rc;
			if (previous.hash == content.digest) {
				rc = content.validate_source();
				if (rc != TAGD_OK)
					return rc;
				rc = content.cleanup();
				if (rc != TAGD_OK)
					return rc;
				output.stored = previous.stored;
				output.link = previous.link;
				output.revision = previous.revision;
				attempted_association = "; attempted A=" + output.source.id()
					+ "; L=" + output.link + "; B=" + output.stored.id();
				output.outcome = store_outcome::unchanged;
				if (put_rc == TAGD_OK) {
					const std::vector<const abstract_tag*> batch{&canonical};
					rc = db.put_batch(batch, ssn,
						(flags & ~F_NO_POS_CAST) | F_NO_TRANSFORM_REFERENTS | F_IGNORE_DUPLICATES);
					if (rc != TAGD_OK)
						return error(rc, "filepile source predicate update failed");
					output.changed = true;
				}
				result = std::move(output);
				if (result.changed || (flags & F_IGNORE_DUPLICATES)) {
					if (ssn)
						ssn->code(TAGD_OK);
					return code(TAGD_OK);
				}
				const auto rc = error(TS_DUPLICATE, "filepile source content is unchanged: " + result.source.id());
				if (ssn)
					ssn->code(rc);
				return rc;
			}
		}

		std::set<std::string> verified;
		rc = verify_recorded_content(*this, verified, db, content, ssn);
		if (rc != TAGD_OK)
			return rc;
		output.revision = previous.revision + 1;
		rc = filepile::link_id(*this, output.link, content.source, output.revision);
		if (rc != TAGD_OK)
			return rc;
		// TODO(filepile-extensions): Reconciliation may refresh metadata; store preserves existing suffixes.
		if (name.empty()) {
			rc = content.discover(name);
			if (rc != TAGD_OK)
				return rc;
		}
		const bool reuse = !name.empty();
		if (!reuse) {
			const auto extension = canonical_extension(detection.media_type);
			name = content.digest + '.' + (extension.empty() ? "bin" : extension);
		}
		output.stored = url(filepile::path_url(content.destination(name)));
		attempted_association = "; attempted A=" + output.source.id()
			+ "; L=" + output.link + "; B=" + output.stored.id();
		rc = output.stored.code();
		if (rc != TAGD_OK)
			return error(rc, "filepile destination exceeds URL limits");
		rc = output.stored.type_relation(HARD_TAG_TYPE_OF, HARD_TAG_FILE_STORED);
		if (rc != TAGD_OK)
			return error(rc, "cannot classify stored filepile URL");
		rc = content.validate_source();
		if (rc != TAGD_OK)
			return rc;
		if (reuse) {
			if (!verified.contains(name)) {
				rc = content.verify(name, content.digest, true);
				if (rc != TAGD_OK)
					return rc;
			}
			rc = content.validate_source();
			if (rc != TAGD_OK)
				return rc;
			rc = content.cleanup();
			if (rc != TAGD_OK)
				return rc;
		} else {
			rc = content.publish(name);
			if (rc != TAGD_OK)
				return rc;
			published = true;
		}

		std::vector<const abstract_tag*> batch;
		if (put_rc == TAGD_OK)
			batch.push_back(&output.source);
		if (!db.exists(output.stored.hduri())) {
			rc = metadata(*this, output.stored, HARD_TAG_HASH_SHA256, content.digest);
			if (rc != TAGD_OK)
				return rc;
			rc = metadata(*this, output.stored, HARD_TAG_FILE_SIZE, std::to_string(content.stored.st_size), TYPE_INTEGER);
			if (rc != TAGD_OK)
				return rc;
			rc = metadata(*this, output.stored, HARD_TAG_MEDIA_TYPE, detection.media_type);
			if (rc != TAGD_OK)
				return rc;

			std::string modified;
			rc = format_datetime(modified, content.stored.st_mtim.tv_sec, content.stored.st_mtim.tv_nsec);
			if (rc != TAGD_OK)
				return error(rc, "filepile timestamp is out of range");
			rc = metadata(*this, output.stored, HARD_TAG_MTIME, modified);
			if (rc != TAGD_OK)
				return rc;
			batch.push_back(&output.stored);
		}
		abstract_tag link(output.link, HARD_TAG_SUB, HARD_TAG_LINK_FILEPILE, POS_TAG);
		rc = link.relation(HARD_TAG_FROM, output.source.hduri());
		if (rc != TAGD_OK)
			return error(rc, "cannot construct filepile source association");
		rc = link.relation(HARD_TAG_TO, output.stored.hduri());
		if (rc != TAGD_OK)
			return error(rc, "cannot construct filepile destination association");
		rc = metadata(*this, link, HARD_TAG_REVISION, std::to_string(output.revision), TYPE_INTEGER);
		if (rc != TAGD_OK)
			return rc;

		std::string modified;
		rc = format_datetime(modified, content.initial.st_mtim.tv_sec, content.initial.st_mtim.tv_nsec);
		if (rc != TAGD_OK)
			return error(rc, "filepile timestamp is out of range");
		rc = metadata(*this, link, HARD_TAG_MTIME, modified);
		if (rc != TAGD_OK)
			return rc;

		std::string now;
		rc = current_datetime(now);
		if (rc != TAGD_OK)
			return error(rc, "cannot obtain filepile association time");
		rc = metadata(*this, link, HARD_TAG_STORED_TIME, now);
		if (rc != TAGD_OK)
			return rc;
		batch.push_back(&link);
		// TODO(tagd-io): Atomic PUT covers semantic failure, not shared-filesystem concurrency.
		rc = db.put_batch(batch, ssn, (flags & ~F_NO_POS_CAST) | F_NO_TRANSFORM_REFERENTS | F_IGNORE_DUPLICATES);
		if (rc != TAGD_OK)
			return error(rc, "filepile semantic update failed; published content is retained");
		output.outcome = store_outcome::new_revision;
		output.changed = true;
		result = std::move(output);
		return code(TAGD_OK);
	};

	const auto rc = operation();
	// Preserve the initiating status even when removing staging also fails.
	const auto cleanup_rc = content.cleanup();
	copy_errors(content);
	const auto status = rc == TAGD_OK ? cleanup_rc : rc;
	result.commit_unknown = semantic_attempted && backend().commit_unknown();
	if (ssn)
		ssn->code(status);
	std::string message = "STORE " + input.id() + "; result=" + code_str(status)
		+ "; changed=" + (result.commit_unknown ? "unknown" : result.changed || published ? "true" : "false");
	if (!result.link.empty())
		message += "; A=" + result.source.id() + "; L=" + result.link + "; B=" + result.stored.id();
	else
		message += attempted_association;
	if (published && !result.changed)
		message += "; complete content published; semantic update did not confirm success";
	filepile::log_event(ssn, HARD_TAG_FILEPILE_STORE_EVENT, status, message);
	return code(status);
}

} // namespace tagd
