#pragma once

#include "tagspace.h"

namespace tagspace {

class bootstrap {
		// TS, id, super, pos
		static ts_res_code put_hard_tag(
			tagspace&, const tagd::id_type&, const tagd::id_type&, const tagd::part_of_speech&);

	public:
		static ts_res_code init_hard_tags(tagspace&);
};

} // namespace tagspace
