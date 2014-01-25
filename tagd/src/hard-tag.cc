#include <cstring>  // tolower, strcmp (for gperf)
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>  // find_last_of

#include "tagd.h"
#include "hard-tags.gperf.h"

namespace tagd {

// looks up hard tag and returns part_of_speech
part_of_speech hard_tag::pos(const std::string &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    // std::cout << "hard_tag_hash::lookup( " << id << " ) => "
      //         << (val == NULL ? "POS_UKNOWN" : pos_str(val->pos)) << std::endl;

    if (val == NULL)
        return POS_UNKNOWN;
    else
        return val->pos;
}

part_of_speech hard_tag::term_pos(const tagd::id_type& id, rowid_t* row_id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    // std::cout << "hard_tag_hash::lookup( " << id << " ) => "
      //         << (val == NULL ? "POS_UKNOWN" : pos_str(val->pos)) << std::endl;

    if (val == NULL) {
        return POS_UNKNOWN;
	} else {
		*row_id = (val->rank + 1);
        return val->pos;
	}
}

tagd_code hard_tag::get(abstract_tag& t, const std::string &id) {
    hard_tag_hash_value *val = hard_tag_hash::lookup(id.c_str(), id.size());

    if (val == NULL) {
        return TS_NOT_FOUND;
	} else {
		t.id(id);
		t.super_relator(HARD_TAG_SUPER);
		t.super_object(val->super);
		t.pos(val->pos);
		rank r;
		r.push_back(val->rank);
		t.rank(r);
    	// std::cout << "hard_tag_hash::get( " << pos_str(val->pos) << ", " << val->rank << ", " << t.rank().dotted_str() << " ) => "
          //    << t  << std::endl;
	
        return t.code();
	}
}

} // namespace tagd
