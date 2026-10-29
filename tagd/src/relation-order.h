#pragma once
#include "tagd.h"
#include <tuple>

namespace tagd::detail {

// Public label edits invalidate snapshot keys; immutable hard tags provide the only fallback.
template<class Keys>
const rank* component_rank(id_view id, const Keys* keys, id_string Keys::* identity,
	rank Keys::* position, rank& hard) {
	if (keys && id == keys->*identity) {
		return &(keys->*position);
	}
	if (const auto* axiom = hard_tag_axiom_for(id)) {
		hard = rank(axiom->packed_rank);
		return &hard;
	}
	return nullptr;
}

template<class Keys>
auto component_order(id_view id, const Keys* keys, id_string Keys::* identity,
	rank Keys::* position, rank& hard) {
	const auto* resolved = component_rank(id, keys, identity, position, hard);
	// A resolved empty root sorts before unresolved labels; IDs break cross-snapshot rank ties.
	return std::tuple(!resolved,
		resolved ? id_view(resolved->c_str(), resolved->size()) : id, id);
}

} // namespace tagd::detail
