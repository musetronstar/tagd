#include <cstring>  // tolower, strcmp (for gperf)
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>  // find_last_of

#include "tagd.h"
#include "hard-tags.gperf.h"

namespace tagdb {

// looks up hard tag and returns part_of_speech
tagd::part_of_speech hard_tag::pos(const tagd::id_type &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == nullptr)
        return tagd::POS_UNKNOWN;
    else
        return val->pos;
}

// returns pos, given term - if non-null row_id passed in, it will be set if found
tagd::part_of_speech hard_tag::term_pos(const tagd::id_type& id, rowid_t* row_id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == nullptr) {
        return tagd::POS_UNKNOWN;
	} else {
		if (row_id != nullptr)
			*row_id = val->row_id;
        return val->pos;
	}
}

// returns pos, given term row_id - if non-null term passed in, it will be set if found
tagd::part_of_speech hard_tag::term_id_pos(rowid_t row_id, tagd::id_type *term) {
	// row == 0 unused
	if (row_id <= 0 || row_id >= hard_tag_rows_end)
		return tagd::POS_UNKNOWN;

	tagd::id_type id{ hard_tag_rows[row_id] };
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == nullptr) {
        return tagd::POS_UNKNOWN;
	} else {
		if (term != nullptr)
			*term = id;
        return val->pos;
	}
}

tagd::code hard_tag::get(tagd::abstract_tag& t, const tagd::id_type &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == nullptr) {
        return tagd::TS_NOT_FOUND;
	} else {
		t.id(id);
		t.sub_relator(HARD_TAG_SUB);
		t.super_object(val->sub);
		t.pos(val->pos);
		t.rank(tagd::rank(val->rank));
	
        return t.code();
	}
}

const char ** hard_tag::rows() {
	return hard_tag_rows;
}

size_t hard_tag::rows_end() {
	return hard_tag_rows_end;
}

} // namespace tagdb
