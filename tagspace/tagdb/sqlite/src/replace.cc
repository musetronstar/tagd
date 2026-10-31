#include "tagdb/sqlite.h"

namespace tagdb {

tagd::code sqlite::replace(tagd::replace_outcome& outcome, const tagd::abstract_tag& input,
	session* ssn, flags_t flags, const prepare_replace& retain) {
	mutation_scope mutation(*this);
	if (!(flags & F_NO_RESET))
		reset(ssn);
	if (_write_poisoned)
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");

	// Replacement compares explicit rows under the same write lock as the eventual update.
	write_transaction transaction(*this, write_transaction::kind::immediate);
	auto rc = transaction.begin();
	if (rc != tagd::TAGD_OK)
		return rc;
	tagd::replace_outcome result = tagd::replace_outcome::unchanged;
	rc = transaction.finish(replace_impl(result, input, ssn, flags, retain));
	if (rc == tagd::TAGD_OK || rc == tagd::TS_DUPLICATE)
		outcome = result;
	if (ssn)
		ssn->code(rc);
	return code(rc);
}

tagd::code sqlite::replace_impl(tagd::replace_outcome& result, const tagd::abstract_tag& input,
	session* ssn, flags_t flags, const prepare_replace& retain) {
	if (input.pos() == tagd::POS_REFERENT)
		return error(tagd::TS_MISUSE, "replace does not accept referent declarations");

	tagd::abstract_tag candidate;
	auto status = normalize_declaration(candidate, input, ssn, flags);
	if (status != tagd::TAGD_OK)
		return status;

	status = validate_put_identity(candidate, ssn);
	if (status != tagd::TAGD_OK)
		return status;
	if (candidate.super_object().empty())
		return error(tagd::TS_MISUSE, "replace requires a subordinate relation");

	tagd::abstract_tag existing;
	const auto read = get(existing, candidate.id(), nullptr,
		F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR);
	if (read != tagd::TAGD_OK && read != tagd::TS_NOT_FOUND)
		return read;
	// GET displays URL subjects as URLs; writes and row comparisons use their HDURI.
	if (read == tagd::TAGD_OK && existing.id() != candidate.id()) {
		tagd::abstract_tag stored(candidate.id(), existing.sub_relator(),
			existing.super_object(), existing.pos());
		stored.relations = std::move(existing.relations);
		stored.type_relations = std::move(existing.type_relations);
		// Canonicalizing the ID must preserve the subtree prefix used by moves.
		existing = tagd::abstract_tag(stored, existing.rank());
	}

	// Retained filepile facts are not assertions supplied by the caller.
	const bool axioms_only = only_type_axioms(candidate);
	if (retain) {
		const auto kept = retain(candidate, existing);
		if (kept != tagd::TAGD_OK)
			return kept;
	}

	status = validate_declaration(candidate);
	if (status != tagd::TAGD_OK)
		return status;

	// Axiomatic type edges are validated above but never constitute stored state.
	for (auto it = candidate.type_relations.begin(); it != candidate.type_relations.end();) {
		if (it->object == candidate.id() || it->object == tagd::HARD_TAG_ENTITY)
			it = candidate.type_relations.erase(it);
		else
			++it;
	}
	if (read == tagd::TAGD_OK
		&& candidate.sub_relator() == existing.sub_relator()
		&& candidate.super_object() == existing.super_object()
		&& candidate.relations == existing.relations
		&& candidate.type_relations == existing.type_relations) {
		return axioms_only || (flags & F_IGNORE_DUPLICATES) ? tagd::TAGD_OK
			: error(tagd::TS_DUPLICATE, "unchanged replacement: " + input.id());
	}

	// Keep the tag row and incoming references; only its own description is replaced.
	if (read == tagd::TAGD_OK) {
		status = delete_relations(candidate.id());
		if (status != tagd::TAGD_OK)
			return status;
		status = delete_type_relations(existing);
		if (status != tagd::TAGD_OK)
			return status;
	}
	status = put_validated(candidate, existing, ssn, F_NO_TRANSFORM_REFERENTS | F_NO_POS_CAST
		| F_IGNORE_DUPLICATES);
	if (status != tagd::TAGD_OK)
		return status;

	// PUT's bare identity no-op skips FTS, but REPLACE may have removed searchable facts.
	if (read == tagd::TAGD_OK && candidate.relations.empty() && candidate.type_relations.empty()
		&& candidate.sub_relator() == existing.sub_relator()
		&& candidate.super_object() == existing.super_object()) {
		status = update_fts_tag(candidate.id(), F_NO_TRANSFORM_REFERENTS);
		if (status != tagd::TAGD_OK)
			return status;
	}

	status = validate_effective({candidate.id()});
	if (status != tagd::TAGD_OK)
		return status;

	std::set<tagd::id_string> affected;
	tag_affected(affected, existing);
	tag_affected(affected, candidate);
	for (const auto& id : affected) {
		status = update_pos_occurence(id);
		if (status != tagd::TAGD_OK)
			return status;
	}
	result = read == tagd::TS_NOT_FOUND ? tagd::replace_outcome::created
		: tagd::replace_outcome::replaced;
	return tagd::TAGD_OK;
}

} // namespace tagdb
