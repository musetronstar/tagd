#pragma once

#include <cassert>
#include <bitset>
#include "tagd.h"

namespace tagspace {

typedef enum {
	NO_REFERENTS	= 1 << 0		// don't do referent lookups
	// ...
	//          	= 1 << 31,
} flags;

typedef std::bitset<sizeof(flags)> flags_t;

// pure virtual interface
class tagspace : public tagd::errorable {
	protected:
		// stack of tags ids as context
		tagd::id_vec _context;

	public:
		tagspace() : tagd::errorable(tagd::TS_INIT) {}
		virtual ~tagspace() {}

		tagd_code push_context(const tagd::id_type& id) {
			tagd::abstract_tag t;
			if (this->exists(id) == tagd::TAGD_OK) {
				_context.push_back(id);
				return _code;
			}

			return this->error(tagd::TS_INTERNAL_ERR, "push_context failed: %s");
		}

		const tagd::id_vec& context() const { return _context; }

		// though returning a tagd code is irrelevent here, it is useful
		// for derived classes to return a code
		tagd_code pop_context() { _context.pop_back(); return tagd::TAGD_OK; }
		tagd_code clear_context() { _context.clear(); return tagd::TAGD_OK; }

		virtual tagd_code get(tagd::abstract_tag&, const tagd::id_type&, const flags_t& = flags_t()) = 0; // get into tag, given id
		virtual tagd_code put(const tagd::abstract_tag&, const flags_t& = flags_t()) = 0;
		virtual tagd::part_of_speech pos(const tagd::id_type&) = 0; 
		virtual tagd_code exists(const tagd::id_type&) = 0;
		virtual tagd_code query(tagd::tag_set&, const tagd::interrogator&) = 0;
		virtual tagd_code dump(std::ostream& os = std::cout) = 0;
		virtual tagd_code dump_grid(std::ostream& os = std::cout) = 0;
};

struct util {
	// users default db
	static std::string user_db();
};

} // namespace tagspace
