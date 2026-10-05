#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>  // find_last_of

#include "tagdb.h"
#include "tagd/hard-tags.h"
#include "tagd/logger.h"

namespace tagdb {

namespace {

std::vector<const char *>& hard_tag_rows_storage() {
	static std::vector<const char *> rows = []() {
		std::vector<const char *> values;
		values.reserve(tagd::HARD_TAG_AXIOMS.size() + 1);
		/*
		 * Row 0 stays empty so hard-tag row ids match the historical sqlite ids
		 * starting at 1 without rewriting the bootstrap callers.
		 */
		values.push_back("");
		for (const tagd::hard_tag_axiom& axiom : tagd::HARD_TAG_AXIOMS)
			values.push_back(axiom.id.data());
		return values;
	}();

	return rows;
}

rowid_t hard_tag_row_id(const tagd::hard_tag_axiom& axiom) {
	// Hard-tag row IDs follow declaration order in hard-tags.h.
	return static_cast<rowid_t>((&axiom - tagd::HARD_TAG_AXIOMS.data()) + 1);
}

}

static bool valid_log_role_hard_tag(const std::string& role) {
	tagd::abstract_tag tag;
	if (hard_tag::get(tag, role) != tagd::TAGD_OK)
		return false;

	return tag.super_object() == HARD_TAG_ROLE
		|| tag.super_object() == HARD_TAG_ROLE_SYSTEM;
}

// looks up hard tag and returns part_of_speech
tagd::part_of_speech hard_tag::pos(tagd::id_view id) {
	const tagd::hard_tag_axiom *axiom = tagd::hard_tag_axiom_for(id);
	return axiom == nullptr ? tagd::POS_UNKNOWN : axiom->pos;
}

// returns pos, given term - if non-null row_id passed in, it will be set if found
tagd::part_of_speech hard_tag::term_pos(tagd::id_view id, rowid_t* row_id) {
	const tagd::hard_tag_axiom *axiom = tagd::hard_tag_axiom_for(id);
	if (axiom == nullptr)
		return tagd::POS_UNKNOWN;

	if (row_id != nullptr)
		*row_id = hard_tag_row_id(*axiom);

	return axiom->pos;
}

// returns pos, given term row_id - if non-null term passed in, it will be set if found
tagd::part_of_speech hard_tag::term_id_pos(rowid_t row_id, tagd::id_string *term) {
	const auto& rows = hard_tag_rows_storage();
	// row == 0 unused
	if (row_id <= 0 || static_cast<std::size_t>(row_id) >= rows.size())
		return tagd::POS_UNKNOWN;

	const char *id = rows[row_id];
	const tagd::hard_tag_axiom *axiom = tagd::hard_tag_axiom_for(id);
	if (axiom == nullptr)
		return tagd::POS_UNKNOWN;

	if (term != nullptr)
		*term = id;

	return axiom->pos;
}

tagd::code hard_tag::get(tagd::abstract_tag& t, tagd::id_view id) {
	const tagd::hard_tag_axiom *axiom = tagd::hard_tag_axiom_for(id);
	if (axiom == nullptr)
		return tagd::TS_NOT_FOUND;

	// TODO: remove bridge when hard_tag::get() returns by value
	tagd::abstract_tag sem(id, axiom->sub_relator, axiom->super_object, axiom->pos);
	tagd::rank r;
	(void)r.init(axiom->packed_rank);
	t = r.empty() ? std::move(sem) : tagd::abstract_tag(sem, r);

	return t.code();
}

void hard_tag::install_logger_validator() {
	// Validate logging roles against the generated hard tags used by the backend.
	tagd::set_log_role_validator(valid_log_role_hard_tag);
}

const char ** hard_tag::rows() {
	return hard_tag_rows_storage().data();
}

size_t hard_tag::rows_end() {
	return hard_tag_rows_storage().size();
}

} // namespace tagdb
