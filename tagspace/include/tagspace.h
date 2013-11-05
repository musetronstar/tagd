#pragma once

#include <cassert>
#include <stdint.h>
#include "tagd.h"

namespace tagspace {

typedef enum {
	NO_POS_CAST	           = 1 << 0, // don't cast a tag according to its pos (i.e. url, referent...)
	NO_TRANSFORM_REFERENTS = 1 << 1  // don't transform to/from referent to tag when putting/getting
	// ...
	//          	= 1 << 31,
} ts_flags;

typedef uint32_t flags_t;

// pure virtual interface
class tagspace : public tagd::errorable {
	protected:
		// stack of tags ids as context
		tagd::id_vec _context;

	public:
		tagspace() : tagd::errorable(tagd::TS_INIT) {}
		virtual ~tagspace() {}

		virtual tagd_code push_context(const tagd::id_type&);
		virtual const tagd::id_vec& context() const;
		virtual tagd_code pop_context();
		virtual tagd_code clear_context();

		virtual tagd_code get(tagd::abstract_tag&, const tagd::id_type&, flags_t = flags_t()) = 0; // get into tag, given id
		virtual tagd_code put(const tagd::abstract_tag&, flags_t = flags_t()) = 0;
		virtual tagd::part_of_speech pos(const tagd::id_type&, flags_t = flags_t()) = 0; 
		virtual tagd_code exists(const tagd::id_type&) = 0;
		virtual tagd_code query(tagd::tag_set&, const tagd::interrogator&, flags_t = flags_t()) = 0;
		virtual tagd_code dump(std::ostream& os = std::cout) = 0;
		virtual tagd_code dump_grid(std::ostream& os = std::cout) = 0;
		virtual tagd_code dump_terms(std::ostream& os = std::cout) = 0;
};

struct util {
	// users default db
	static std::string user_db();
};

} // namespace tagspace
