#include "tagd.h"
#include "relation-order.h"
#include <map>
#include <tuple>

namespace tagd {

/*
 * TODO(TAGL-types): Use shared TAGL value ordering for modifiers,
 * retaining rank precedence and distinguishing numeric equality from
 * typed identity. See workspace TASKS.d/2026-10-22-TAGL-types-and-modifiers.md.
 */
bool predicate::canonical_less(const predicate& a, const predicate& b) {
	auto component = [&](auto field, auto identity, auto position) {
		rank hard_a, hard_b;
		return detail::component_order(a.*field, a._order.get(), identity, position, hard_a)
			<=> detail::component_order(b.*field, b._order.get(), identity, position, hard_b);
	};

	const auto relator = component(&predicate::relator, &order_keys::relator_id,
		&order_keys::relator_rank);
	if (relator != 0) {
		return relator < 0;
	}

	const auto object = component(&predicate::object, &order_keys::object_id,
		&order_keys::object_rank);
	if (object != 0) {
		return object < 0;
	}

	return std::tie(a.modifier, a.opr8r, a.modifier_type)
		< std::tie(b.modifier, b.opr8r, b.modifier_type);
}

bool predicate_set::group_compare::operator()(const predicate& a, const predicate& b) const {
	auto key = [](const predicate& p) {
		// Empty modifiers are equivalent regardless of metadata in the legacy comparator.
		const bool empty = p.modifier.empty();
		const bool text = !empty && p.modifier_type == TYPE_STRING;
		return std::tuple(id_view(p.relator), id_view(p.object), !empty, text,
			empty ? OP_EQ : p.opr8r, text ? id_view(p.modifier) : id_view{});
	};

	return key(a) < key(b);
}

predicate_set::predicate_set(const predicate_set& other) : _values(other._values) {
	rebuild_indexes();
}

predicate_set::predicate_set(const predicate_set& other, const allocator_type& allocator)
	: _values(other._values, allocator) {
	rebuild_indexes();
}

predicate_set::predicate_set(predicate_set&& other) noexcept {
	swap(other);
}

predicate_set::predicate_set(predicate_set&& other, const allocator_type&) noexcept {
	// std::allocator is always equal, so node ownership and index iterators can transfer together.
	swap(other);
}

predicate_set& predicate_set::operator=(const predicate_set& other) {
	if (this != &other) {
		predicate_set copy(other);
		swap(copy);
	}

	return *this;
}

predicate_set& predicate_set::operator=(predicate_set&& other) noexcept {
	if (this != &other) {
		clear();
		swap(other);
	}

	return *this;
}

void predicate_set::index(storage::const_iterator it) {
	_members[*it].insert(it);
	if (it->_order && it->relator == it->_order->relator_id
		&& it->object == it->_order->object_id) {
		_relators[it->relator].insert(it);
		_objects[it->object].insert(it);
	}
}

void predicate_set::unindex(storage::const_iterator it) {
	auto remove = [it](auto& index, const auto& key) {
		auto found = index.find(key);
		if (found == index.end())
			return;

		found->second.erase(it);
		if (found->second.empty())
			index.erase(found);
	};

	remove(_members, *it);
	remove(_relators, it->relator);
	remove(_objects, it->object);
}

void predicate_set::rebuild_indexes() {
	for (auto it = begin(); it != end(); ++it) {
		index(it);
	}
}

predicate_set::iterator predicate_set::find(const predicate& p) const {
	const auto group = _members.find(p);
	if (group == _members.end())
		return end();

	// Numeric spellings need not be adjacent in literal order; search only their group.
	for (auto it : group->second) {
		if ((p.modifier.empty() || it->modifier_type == p.modifier_type)
			&& !(*it < p) && !(p < *it))
			return it;
	}

	return end();
}

predicate predicate_set::lookup_key(const predicate& p) const {
	predicate result = p;

	// Public IDs can be edited on detached predicates; their old ranks then expire.
	if (result._order && (p.relator != result._order->relator_id
		|| p.object != result._order->object_id))
		result._order.reset();

	const auto relator = _relators.find(p.relator);
	const auto object = _objects.find(p.object);
	if (relator != _relators.end() && object != _objects.end()) {
		/*
		 * Match the destination snapshot, choosing its last canonical occurrence
		 * when callers combine snapshots. Partial knowledge leaves the key unchanged.
		 */
		const auto& r = *(*relator->second.rbegin())->_order;
		const auto& o = *(*object->second.rbegin())->_order;
		predicate::order_keys keys{p.relator, p.object, r.relator_rank, o.object_rank};
		result._order = std::make_shared<const predicate::order_keys>(std::move(keys));
	}

	return result;
}

std::pair<predicate_set::iterator, bool> predicate_set::insert(const predicate& p) {
	auto found = find(p);
	if (found != end())
		return {found, false};

	auto result = _values.insert(lookup_key(p));
	if (result.second) {
		index(result.first);
	}

	return result;
}

predicate_set::insert_return_type predicate_set::insert(node_type&& node) {
	if (node.empty())
		return {end(), false, {}};
	auto found = find(node.value());
	if (found != end())
		return {found, false, std::move(node)};

	node.value() = lookup_key(node.value());
	auto result = _values.insert(std::move(node));
	if (result.inserted) {
		index(result.position);
	}

	return {result.position, result.inserted, std::move(result.node)};
}

predicate_set::iterator predicate_set::insert(iterator, node_type&& node) {
	auto result = insert(std::move(node));

	// Hinted insertion leaves a rejected node in the caller's handle, as std::set does.
	if (!result.inserted)
		node = std::move(result.node);

	return result.position;
}

void predicate_set::merge(predicate_set& other) {
	if (this == &other)
		return;

	for (auto it = other.begin(); it != other.end();) {
		auto current = it++;
		if (contains(*current))
			continue;

		// Transfer the owning node and rebuild indexes for the destination snapshot.
		const predicate original = *current;
		auto node = other.extract(current);
		auto result = insert(std::move(node));
		if (!result.inserted) {
			// A rejected transfer retains the source snapshot's ranks.
			result.node.value() = original;
			other.insert(std::move(result.node));
		}
	}
}

predicate_set::iterator predicate_set::erase(iterator it) {
	unindex(it);
	return _values.erase(it);
}

predicate_set::iterator predicate_set::erase(iterator first, iterator last) {
	while (first != last) {
		first = erase(first);
	}

	return last;
}

predicate_set::node_type predicate_set::extract(iterator it) {
	unindex(it);
	return _values.extract(it);
}

size_t predicate_set::erase(const predicate& p) {
	auto it = find(p);
	if (it == end())
		return 0;

	erase(it);
	return 1;
}

bool predicate_set::operator==(const predicate_set& other) const {
	if (size() != other.size() || _members.size() != other._members.size())
		return false;

	// Group order is independent of ranks, so string/empty groups compare in one pass.
	auto right = other._members.begin();
	const group_compare compare;
	for (const auto& [key, group] : _members) {
		if (compare(key, right->first) || compare(right->first, key)
			|| group.size() != right->second.size())
			return false;

		for (auto left : group) {
			auto match = std::find_if(right->second.begin(), right->second.end(),
				[left](auto candidate) {
					return (left->modifier.empty() || left->modifier_type == candidate->modifier_type)
						&& !(*left < *candidate) && !(*candidate < *left);
				});
			if (match == right->second.end() || *left != **match)
				return false;
		}

		++right;
	}

	return true;
}

std::weak_ordering predicate_set::operator<=>(const predicate_set& other) const {
	// Group order is semantic; only numeric groups need sorting independently of ranks.
	auto semantic_values = [](const predicate_set& values) {
		std::vector<const predicate*> result;
		result.reserve(values.size());
		for (const auto& [key, group] : values._members) {
			const auto first = result.size();
			for (auto it : group) {
				result.push_back(&*it);
			}

			if (group.size() > 1 && !key.modifier.empty()
				&& key.modifier_type != TYPE_STRING) {
				std::sort(result.begin() + first, result.end(),
					[](const predicate* a, const predicate* b) {
						return *a < *b || (!(*b < *a) && a->modifier_type < b->modifier_type);
					});
			}
		}

		return result;
	};

	const auto left = semantic_values(*this);
	const auto right = semantic_values(other);
	return std::lexicographical_compare_three_way(left.begin(), left.end(),
		right.begin(), right.end(), [](const predicate* a, const predicate* b) -> std::weak_ordering {
			if (*a < *b)
				return std::weak_ordering::less;
			if (*b < *a)
				return std::weak_ordering::greater;
			// Compatible numeric values remain distinct members when their types differ.
			if (!a->modifier.empty())
				return a->modifier_type <=> b->modifier_type;
			return std::weak_ordering::equivalent;
		});
}

code predicate_set::order(const std::function<code(rank&, id_view)>& resolve) {
	storage ordered;
	std::map<id_string, rank> ranks;
	auto lookup = [&](rank& r, const id_string& id) {
		auto it = ranks.find(id);
		if (it == ranks.end()) {
			auto rc = resolve(r, id);
			if (rc != TAGD_OK)
				return rc;
			ranks.emplace(id, r);
		} else {
			r = it->second;
		}
		return TAGD_OK;
	};
	for (auto p : _values) {
		predicate::order_keys keys{p.relator, p.object, {}, {}};
		auto rc = lookup(keys.relator_rank, p.relator);
		if (rc != TAGD_OK)
			return rc;
		rc = lookup(keys.object_rank, p.object);
		if (rc != TAGD_OK)
			return rc;
		p._order = std::make_shared<const predicate::order_keys>(std::move(keys));
		ordered.insert(std::move(p));
	}

	predicate_set replacement;
	replacement._values.swap(ordered);
	replacement.rebuild_indexes();
	swap(replacement);
	return TAGD_OK;
}

void predicate_set::transform(const std::function<id_string(const id_string&)>& encode) {
	storage encoded;
	for (auto p : _values) {
		// Substitution preserves valid ranks, but must not revive ranks of edited IDs.
		if (p._order && (p.relator != p._order->relator_id
			|| p.object != p._order->object_id))
			p._order.reset();
		p.relator = encode(p.relator);
		p.object = encode(p.object);
		if (p._order) {
			auto keys = *p._order;
			keys.relator_id = p.relator;
			keys.object_id = p.object;
			p._order = std::make_shared<const predicate::order_keys>(std::move(keys));
		}
		encoded.insert(std::move(p));
	}

	predicate_set replacement;
	replacement._values.swap(encoded);
	replacement.rebuild_indexes();
	swap(replacement);
}

} // namespace tagd
