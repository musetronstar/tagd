#include "tagd.h"
#include "relation-order.h"
#include <algorithm>
#include <tuple>

namespace tagd {

bool type_predicate::operator<(const type_predicate& rhs) const {
	const auto& a = *this;
	const auto& b = rhs;
	auto component = [&](auto field, auto identity, auto position) {
		rank hard_a, hard_b;
		return detail::component_order(a.*field, a._order.get(), identity, position, hard_a)
			<=> detail::component_order(b.*field, b._order.get(), identity, position, hard_b);
	};

	const auto relator = component(&type_predicate::relator, &order_keys::relator_id,
		&order_keys::relator_rank);
	if (relator != 0) {
		return relator < 0;
	}
	return component(&type_predicate::object, &order_keys::object_id, &order_keys::object_rank) < 0;
}

type_predicate_set::iterator type_predicate_set::find(const type_predicate& value) const {
	// Membership uses labels independently of the caller's rank snapshot.
	return std::find(begin(), end(), value);
}

std::pair<type_predicate_set::iterator, bool>
type_predicate_set::insert(const type_predicate& value) {
	const auto found = find(value);
	if (found != end()) {
		return {found, false};
	}

	return _values.insert(value);
}

size_t type_predicate_set::erase(const type_predicate& value) {
	const auto found = find(value);
	if (found == end()) {
		return 0;
	}

	erase(found);
	return 1;
}

bool type_predicate_set::operator==(const type_predicate_set& rhs) const {
	return size() == rhs.size() && std::all_of(begin(), end(), [&](const auto& value) {
		return rhs.contains(value);
	});
}

code type_predicate_set::order(const std::function<code(rank&, id_view)>& lookup) {
	storage ordered;
	for (auto value : _values) {
		type_predicate::order_keys keys{value.relator, value.object, {}, {}};
		auto rc = lookup(keys.relator_rank, value.relator);
		if (rc != TAGD_OK) {
			return rc;
		}

		rc = lookup(keys.object_rank, value.object);
		if (rc != TAGD_OK) {
			return rc;
		}

		value._order = std::make_shared<const type_predicate::order_keys>(std::move(keys));
		ordered.insert(std::move(value));
	}

	_values.swap(ordered);
	return TAGD_OK;
}

void type_predicate_set::transform(const std::function<id_string(const id_string&)>& encode) {
	type_predicate_set encoded;
	for (auto value : _values) {
		const bool ranked = value._order && value.relator == value._order->relator_id
			&& value.object == value._order->object_id;
		value.relator = encode(value.relator);
		value.object = encode(value.object);
		if (ranked) {
			// Context changes labels without changing the ranks retained by this read.
			auto keys = *value._order;
			keys.relator_id = value.relator;
			keys.object_id = value.object;
			value._order = std::make_shared<const type_predicate::order_keys>(std::move(keys));
		} else {
			value._order.reset();
		}

		encoded.insert(std::move(value));
	}

	swap(encoded);
}

code abstract_tag::type_relation(id_view relator, id_view object) {
	if (relator.empty() || object.empty()) {
		return TAG_ILLEGAL;
	}

	return type_relations.emplace(relator, object).second ? TAGD_OK : TAG_DUPLICATE;
}

} // namespace tagd
