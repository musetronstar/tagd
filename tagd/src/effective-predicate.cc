#include "tagd.h"
#include "relation-order.h"

namespace tagd {

bool predicate::specializes(const predicate& general) const {
	if (!general.modifier.empty()) {
		return false;
	}
	auto contains = [&](auto field, auto identity, auto position) {
		const auto& g = general.*field;
		const auto& s = this->*field;
		if (g == s) {
			return true;
		}

		rank hard_general, hard_specific;
		const auto* gr = detail::component_rank(g, general._order.get(), identity, position, hard_general);
		const auto* sr = detail::component_rank(s, _order.get(), identity, position, hard_specific);
		// An empty resolved rank denotes the root; unresolved labels establish no containment.
		return gr && sr && (gr->empty() || gr->contains(*sr));
	};

	// Both components specialize together; valued constraints never transfer to other pairs.
	return contains(&predicate::relator, &order_keys::relator_id, &order_keys::relator_rank)
		&& contains(&predicate::object, &order_keys::object_id, &order_keys::object_rank)
		&& (general.relator != relator || general.object != object || !modifier.empty());
}

void normalize_predicates(predicate_set& contributions) {
	enum class component_kind { label, rank };
	using component = std::pair<component_kind, std::string>;
	using pair = std::pair<component, component>;
	std::map<pair, std::vector<const predicate*>> bare;

	auto coordinates = [](const predicate& p, auto field, auto identity, auto position,
		bool ancestors) {
		std::vector<component> result{{component_kind::label, p.*field}};
		rank hard;
		const auto* resolved = detail::component_rank(p.*field, p._order.get(), identity, position, hard);
		if (resolved) {
			for (rank r = *resolved;; r.pop_back()) {
				result.emplace_back(component_kind::rank, r.str());
				if (!ancestors || r.empty()) {
					break;
				}
			}
		}
		return result;
	};
	auto relators = [&](const predicate& p, bool ancestors) {
		return coordinates(p, &predicate::relator, &predicate::order_keys::relator_id,
			&predicate::order_keys::relator_rank, ancestors);
	};
	auto objects = [&](const predicate& p, bool ancestors) {
		return coordinates(p, &predicate::object, &predicate::order_keys::object_id,
			&predicate::order_keys::object_rank, ancestors);
	};

	// Index bare targets by both label and resolved rank, preserving mixed snapshot comparisons.
	for (const auto& p : contributions) {
		if (!p.modifier.empty()) {
			continue;
		}
		for (const auto& r : relators(p, false)) {
			for (const auto& o : objects(p, false)) {
				bare[{r, o}].push_back(&p);
			}
		}
	}
	if (bare.empty()) {
		return;
	}

	std::set<const predicate*> absorbed;
	for (const auto& q : contributions) {
		// Only prefix pairs can be generalized; unrelated predicates need no pairwise comparison.
		for (const auto& r : relators(q, true)) {
			for (const auto& o : objects(q, true)) {
				const auto found = bare.find({r, o});
				if (found == bare.end()) {
					continue;
				}
				for (const auto* p : found->second) {
					if (p != &q && !absorbed.contains(p) && q.specializes(*p)) {
						absorbed.insert(p);
					}
				}
			}
		}
	}

	erase_if(contributions, [&](const auto& p) {
		return absorbed.contains(&p);
	});
}

} // namespace tagd
