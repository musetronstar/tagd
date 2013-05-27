#pragma once

#include <cassert>
#include "tagd.h"

// outside tagspace namespace so typenames aren't so freaking long
// IMPORTANT: changes to this enum must be reflected in code_str below
typedef enum {
    TS_OK,          /* 200 */
    TS_NOT_FOUND,   /* 404 */
    TS_DUPLICATE,
    TS_SUPER_UNK,  /* 400 */
    TS_SUBJECT_UNK,  /* 400 */
    TS_RELATOR_UNK,  /* 400 */
    TS_OBJECT_UNK,  /* 400 */
    TS_ERR_MAX_TAG_LEN, /* 400 */
    TS_ERR,         /* general error due to caller input */
    TS_MISUSE,      /* caller misusing class interface */
    TS_INTERNAL_ERR /* error not due to caller input  */ /* 500 */
} ts_res_code;

// wrap functions in a struct so we can implement them here
// and users can just include this header without introducing
// another implementation file and linker dependency
struct ts_res {
    static const char *code_str(ts_res_code c) {
        switch (c) {
            case TS_OK:              return "TS_OK";
            case TS_NOT_FOUND:       return "TS_NOT_FOUND";
            case TS_DUPLICATE:       return "TS_DUPLICATE";
            case TS_SUPER_UNK:       return "TS_SUPER_UNK";
            case TS_SUBJECT_UNK:     return "TS_SUBJECT_UNK";
            case TS_RELATOR_UNK:     return "TS_RELATOR_UNK";
            case TS_OBJECT_UNK:      return "TS_OBJECT_UNK";
            case TS_ERR_MAX_TAG_LEN: return "TS_ERR_MAX_TAG_LEN";
            case TS_ERR:             return "TS_ERR";
            case TS_INTERNAL_ERR:    return "TS_INTERNAL_ERR";
            default:                 return "TS_STR_UNK";
        }
    }
};

#define ts_res_str(c) ts_res::code_str(c)

namespace tagspace {

/*
struct statement {
    tagd::id_type subject;
    tagd::id_type relation;
    tagd::id_type object;
    tagd::id_type modifier;
    bool operator<(const statement& s) const {
        return (
            subject < s.subject &&
            relation <= s.relation &&
            object < s.object
        );
    }

    bool operator==(const statement& s) const {
        return subject == s.subject &&
               relation == s.relation &&
               object == s.object;
    }
};

typedef std::set<statement> statement_set;
*/

// pure virtual interface
class tagspace {
	public:
		tagspace() {}
		virtual ~tagspace() {}

		virtual ts_res_code get(tagd::abstract_tag&, const tagd::id_type&) = 0; // get into tag, given id
        virtual ts_res_code get(tagd::url&, const tagd::id_type&) = 0;
		virtual ts_res_code put(tagd::abstract_tag&) = 0;
		virtual ts_res_code exists(const tagd::id_type&) = 0;
		//virtual ts_res_code statement(const statement&) = 0;
		virtual ts_res_code sub(tagd::tag_set&, const tagd::id_type&) = 0; // get tags subordinate to id

        // get tags related to object
		//virtual ts_res_code related(tagd::tag_set&, const tagd::predicate&, const tagd::id_type&="_entity") = 0;

		virtual ts_res_code query(tagd::tag_set&, const tagd::interrogator&) = 0;
		// virtual ts_res_code merge(const tagspace&) = 0;
		virtual ts_res_code dump(std::ostream& os = std::cout) = 0;
};

} // namespace tagspace
