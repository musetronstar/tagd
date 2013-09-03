#pragma once

#include <cassert>
#include "tagd.h"

namespace tagspace {

// pure virtual interface
class tagspace : public tagd::errorable {
	public:
		tagspace() : tagd::errorable(tagd::TS_INIT) {}
		virtual ~tagspace() {}

		virtual tagd_code get(tagd::abstract_tag&, const tagd::id_type&) = 0; // get into tag, given id
		virtual tagd_code put(const tagd::abstract_tag&) = 0;
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
