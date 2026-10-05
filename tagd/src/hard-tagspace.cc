#include "tagd/hard-tagspace.h"

#include <algorithm>
#include <string>
#include <vector>

namespace tagd {

namespace {

/* class_order sorts the root (0), ranked tags (1), then unknown IDs (2). */
struct rank_key {
	int class_order;
	std::string bytes;
};

[[nodiscard]] bool hard_axiom_for_id(id_view id, const hard_tag_axiom*& axiom) {
	axiom = hard_tag_axiom_for(id);
	return axiom != nullptr;
}

[[nodiscard]] rank rank_from_packed(uint64_t packed) {
	rank r;
	// Generated hard-tag ranks pack their path bytes into a uint64_t.
	(void)r.init(packed);
	return r;
}

[[nodiscard]] std::string packed_rank_bytes(const hard_tag_axiom& axiom) {
	std::string bytes;
	bytes.reserve(axiom.rank_size);

	// Eight-bit path components are packed from the highest byte downward.
	for (uint8_t index = 0; index < axiom.rank_size; ++index) {
		bytes.push_back(
			static_cast<char>((axiom.packed_rank >> (56 - (index * 8))) & 0xff)
		);
	}

	return bytes;
}

[[nodiscard]] std::string rank_bytes(const rank& r) {
	return std::string(r.c_str(), r.size());
}

[[nodiscard]] rank_key classify_id_rank(
	id_view id,
	const std::function<rank(id_view)>& lookup_rank_fn
) {
	/* Sort the root first, then tags by rank, then unknown IDs by their text. */
	const hard_tag_axiom* axiom = nullptr;
	if (hard_axiom_for_id(id, axiom)) {
		if (axiom->id == HARD_TAG_ENTITY) {
			return {0, std::string()};
		}

		return {1, packed_rank_bytes(*axiom)};
	}

	const rank resolved = lookup_rank_fn(id);
	if (resolved.empty()) {
		return {2, std::string()};
	}

	return {1, rank_bytes(resolved)};
}

[[nodiscard]] int compare_ranked_ids(
	id_view lhs,
	id_view rhs,
	const std::function<rank(id_view)>& lookup_rank_fn
) {
	// Compare ranks first; use ID text to break ties or order unknown IDs.
	const rank_key lhs_key = classify_id_rank(lhs, lookup_rank_fn);
	const rank_key rhs_key = classify_id_rank(rhs, lookup_rank_fn);

	if (lhs_key.class_order != rhs_key.class_order) {
		return lhs_key.class_order < rhs_key.class_order ? -1 : 1;
	}

	if (lhs_key.class_order == 1 && lhs_key.bytes != rhs_key.bytes) {
		return lhs_key.bytes < rhs_key.bytes ? -1 : 1;
	}

	if (lhs != rhs) {
		return lhs < rhs ? -1 : 1;
	}

	return 0;
}

[[nodiscard]] tagd::abstract_tag populated_tag(
	id_view id,
	id_view sub_relator,
	id_view super_object,
	part_of_speech pos,
	const rank& tag_rank,
	const predicate_set& relations = predicate_set{}
) {
	abstract_tag semantic(id, sub_relator, super_object, pos);
	semantic.relations = relations;

	return tag_rank.empty() ? semantic : abstract_tag(semantic, tag_rank);
}

} // namespace

rank hard_tagspace::lookup_rank(id_view id) const {
	const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
	return axiom == nullptr ? rank{} : rank_from_packed(axiom->packed_rank);
}

part_of_speech hard_tagspace::lookup_pos(id_view id) const {
	const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
	return axiom == nullptr ? POS_UNKNOWN : axiom->pos;
}

bool hard_tagspace::contains(id_view ancestor, id_view descendant) const {
	const hard_tag_axiom* ancestor_axiom = hard_tag_axiom_for(ancestor);
	const hard_tag_axiom* descendant_axiom = hard_tag_axiom_for(descendant);

	if (ancestor == descendant) {
		// Unknown ids must not self-contain: string equality is not membership.
		return ancestor_axiom != nullptr;
	}

	if (ancestor_axiom == nullptr || descendant_axiom == nullptr) {
		return false;
	}

	return rank_prefix_of(
		ancestor_axiom->packed_rank,
		ancestor_axiom->rank_size,
		descendant_axiom->packed_rank,
		descendant_axiom->rank_size
	);
}

std::function<bool(const predicate&, const predicate&)> const_tagspace::
	rank_comparator() const {
	/*
	 * Keep this tagspace alive and its ranks unchanged while a container uses
	 * this comparator. Otherwise the container's stored order could be wrong.
	 */
	return [this](const predicate& lhs, const predicate& rhs) {
		const auto lookup = [this](id_view id) {
			return this->lookup_rank(id);
		};
		const int relator_cmp =
			compare_ranked_ids(lhs.relator, rhs.relator, lookup);
		if (relator_cmp != 0) {
			return relator_cmp < 0;
		}

		const int object_cmp =
			compare_ranked_ids(lhs.object, rhs.object, lookup);
		if (object_cmp != 0) {
			return object_cmp < 0;
		}

		if (lhs.modifier != rhs.modifier) {
			return lhs.modifier < rhs.modifier;
		}

		if (lhs.opr8r != rhs.opr8r) {
			return lhs.opr8r < rhs.opr8r;
		}

		return lhs.modifier_type < rhs.modifier_type;
	};
}

code hard_tagspace::query(
	tag_set&,
	const interrogator&,
	tagspace_session*,
	flags_t
) {
	return TS_NOT_IMPLEMENTED;
}

bool hard_tagspace::exists(id_view id, flags_t) const {
	return hard_tag_axiom_for(id) != nullptr;
}

code hard_tagspace::dump(std::ostream&) const {
	return TS_NOT_IMPLEMENTED;
}

code hard_tagspace::get(abstract_tag& t, id_view id, tagspace_session*, flags_t) {
	const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
	if (axiom == nullptr) {
		t.clear();
		return TS_NOT_FOUND;
	}

	t = populated_tag(
		axiom->id,
		axiom->sub_relator,
		axiom->super_object,
		axiom->pos,
		rank_from_packed(axiom->packed_rank)
	);
	return TAGD_OK;
}

rank const_tagspace::lookup_rank(id_view id) const {
	abstract_tag t;
	/*
	 * get() may record an error but does not change stored tags.
	 * Look up the exact ID; rank lookup must not resolve it as a referent.
	 */
	auto* self = const_cast<const_tagspace*>(this);
	return self->get(t, id, nullptr,
			   F_NO_NOT_FOUND_ERROR | F_NO_TRANSFORM_REFERENTS
		   ) == TAGD_OK ? t.rank() : rank{};
}

part_of_speech const_tagspace::lookup_pos(id_view id) const {
	// Like rank lookup, read the exact ID without resolving referents.
	abstract_tag t;
	auto* self = const_cast<const_tagspace*>(this);
	return self->get(t, id, nullptr,
			   F_NO_NOT_FOUND_ERROR | F_NO_TRANSFORM_REFERENTS
		   ) == TAGD_OK ? t.pos() : POS_UNKNOWN;
}

bool const_tagspace::contains(id_view ancestor, id_view descendant) const {
	if (!this->exists(ancestor) || !this->exists(descendant))
		return false;
	// The root contains all existing tags; a missing ID is not another root.
	if (ancestor == HARD_TAG_ENTITY || ancestor == descendant)
		return true;
	return this->lookup_rank(ancestor).contains(this->lookup_rank(descendant));
}

tagd::code tagspace_session::push_context(tagd::id_view id) {
	if (id.empty())
		return this->error(tagd::TS_MISUSE,
			tagd::predicate(
				HARD_TAG_CAUSED_BY,
				HARD_TAG_CONTEXT,
				HARD_TAG_EMPTY
			)
		);
	else if (id == HARD_TAG_ENTITY)
		return this->error(tagd::TS_MISUSE,
			tagd::predicate(
				HARD_TAG_CAUSED_BY,
				HARD_TAG_CONTEXT,
				HARD_TAG_ENTITY
			)
		);

	tagd::abstract_tag t;
	if (_exists(id)) {
		_context.emplace_back(id);
		return tagd::TAGD_OK;
	}

	return this->ferror(
		tagd::TS_INTERNAL_ERR,
		"push_context failed: %.*s",
		static_cast<int>(id.size()),
		id.data()
	);
}

/*
 * though returning a tagd code is irrelevent here, it is useful
 * for derived classes to return a code
 */
tagd::code tagspace_session::pop_context() {
	if (!_context.empty())
		_context.pop_back();
	return tagd::TAGD_OK;
}

tagd::code tagspace_session::clear_context() {
	_context.clear();
	return tagd::TAGD_OK;
}

void tagspace_session::print_context() {
	size_t i = 0, sz = this->context().size();
	for (auto id : this->context()) {
		TAGD_COUT << id;
		if (++i != sz)
			TAGD_COUT << ", ";
		else
			TAGD_COUT << std::endl;
	}
}

} // namespace tagd
