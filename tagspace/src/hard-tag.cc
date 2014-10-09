#include <cstring>  // tolower, strcmp (for gperf)
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>  // find_last_of

#include "tagd.h"
#include "hard-tags.gperf.h"

namespace tagspace {

// looks up hard tag and returns part_of_speech
	tagd::part_of_speech hard_tag::pos(const tagd::id_type &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == NULL)
        return tagd::POS_UNKNOWN;
    else
        return val->pos;
}

// returns pos, given term - if non-null row_id passed in, it will be set if found
tagd::part_of_speech hard_tag::term_pos(const tagd::id_type& id, rowid_t* row_id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == NULL) {
        return tagd::POS_UNKNOWN;
	} else {
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
		*term = id;
        return val->pos;
	}
}

tagd::code hard_tag::get(tagd::abstract_tag& t, const tagd::id_type &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == NULL) {
        return tagd::TS_NOT_FOUND;
	} else {
		t.id(id);
		t.super_relator(HARD_TAG_SUPER);
		t.super_object(val->super);
		t.pos(val->pos);
		t.rank(tagd::rank(val->rank));
    	std::cout << "hard_tag_hash::get( " << pos_str(val->pos) << ", " << val->rank << ", " << t.rank().dotted_str() << " ) => "
        			<< t  << std::endl;
	
        return t.code();
	}
}

} // namespace tagspace