#include "tagd/hard-tagspace.h"

#include <algorithm>
#include <string>
#include <vector>

namespace tagd {

namespace {

[[nodiscard]] rank rank_from_packed(uint64_t packed) {
	rank r;
	// Generated definitions pack rank path bytes into a uint64_t.
	(void)r.init(packed);
	return r;
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

std::function<bool(const predicate&, const predicate&)>
	const_tagspace::rank_comparator() const {
	// Resolved predicates carry their ranks; comparison never accesses live storage.
	return predicate::canonical_less;
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
	rank result;
	(void)lookup_rank(result, id);
	return result;
}

code const_tagspace::lookup_rank(rank& result, id_view id) const {
	if (const auto* axiom = hard_tag_axiom_for(id)) {
		result = rank_from_packed(axiom->packed_rank);
		return TAGD_OK;
	}
	abstract_tag t;
	auto* self = const_cast<const_tagspace*>(this);
	auto rc = self->get(t, id, nullptr,
		F_NO_NOT_FOUND_ERROR | F_NO_TRANSFORM_REFERENTS);
	if (rc == TAGD_OK)
		result = t.rank();
	return rc;
}

code const_tagspace::order_predicates(predicate_set& values) const {
	return values.order([this](rank& r, id_view id) {
		return lookup_rank(r, id);
	});
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
		tagd::TS_NOT_FOUND,
		"unknown context: %.*s",
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
