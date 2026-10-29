#include "tagdb/relation-resolver.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace tagdb {

const std::set<tagd::id_string>& relation_resolver::sources(tagd::id_view subject) const {
	if (const auto found = _sources.find(subject); found != _sources.end()) {
		return found->second;
	}

	std::set<tagd::id_string> reached;
	std::vector<tagd::id_string> pending{std::string(subject)};
	while (!pending.empty()) {
		auto id = std::move(pending.back());
		pending.pop_back();
		if (!reached.insert(id).second) {
			continue;
		}

		if (const auto cached = _sources.find(id); cached != _sources.end()) {
			reached.insert(cached->second.begin(), cached->second.end());
			continue;
		}

		const auto node = _declarations.find(id);
		if (node == _declarations.end()) {
			continue;
		}
		if (node->second.super_object() != id) {
			pending.push_back(node->second.super_object());
		}
		for (const auto& type : node->second.type_relations) {
			pending.push_back(type.object);
		}
	}

	return _sources.emplace(subject, std::move(reached)).first->second;
}

bool relation_resolver::structurally_contains(tagd::id_view general,
	tagd::id_view specific) const {
	const auto g = _ranks.find(general), s = _ranks.find(specific);
	return g != _ranks.end() && s != _ranks.end()
		&& (general == tagd::HARD_TAG_ENTITY || g->second.contains(s->second));
}

namespace {
// Bounds describe the existing numeric/text domains; absence is not a value constraint.
struct constraint_set {
	struct bound {
		long double number = 0;
		std::string text;
		bool inclusive = true;
	};
	bool numeric = false, supplied = false, valid = true, disjoint = false;
	std::optional<bound> lower, upper;

	int compare(const bound& a, const bound& b) const {
		if (numeric) {
			return a.number < b.number ? -1 : a.number > b.number ? 1 : 0;
		}
		return a.text.compare(b.text);
	}

	void add(const tagd::predicate& p) {
		if (p.modifier.empty()) {
			return;
		}
		const bool domain = p.modifier_type != tagd::TYPE_STRING;
		if (supplied && numeric != domain) {
			disjoint = true;
			return;
		}
		numeric = domain;
		supplied = true;
		bound b;
		b.text = p.modifier;
		if (numeric) {
			char* end = nullptr;
			b.number = std::strtold(p.modifier.c_str(), &end);
			valid &= end == p.modifier.c_str() + p.modifier.size() && std::isfinite(b.number);
		}
		b.inclusive = p.opr8r != tagd::OP_LT && p.opr8r != tagd::OP_GT;
		if (p.opr8r == tagd::OP_EQ || p.opr8r == tagd::OP_GT || p.opr8r == tagd::OP_GT_EQ) {
			if (!lower || compare(b, *lower) > 0) {
				lower = b;
			} else if (compare(b, *lower) == 0) {
				lower->inclusive &= b.inclusive;
			}
		}
		if (p.opr8r == tagd::OP_EQ || p.opr8r == tagd::OP_LT || p.opr8r == tagd::OP_LT_EQ) {
			if (!upper || compare(b, *upper) < 0) {
				upper = b;
			} else if (compare(b, *upper) == 0) {
				upper->inclusive &= b.inclusive;
			}
		}
	}

	bool inhabited() const {
		if (!valid || disjoint) {
			return false;
		}
		if (!supplied) {
			return true;
		}
		bound witness;
		if (numeric) {
			witness.number = -std::numeric_limits<long double>::max();
			if (lower) {
				witness.number = lower->inclusive ? lower->number
					: std::nextafter(lower->number, std::numeric_limits<long double>::infinity());
			}
			if (!std::isfinite(witness.number)) {
				return false;
			}
		} else {
			// Stored text is nonempty and NUL-free until the shared value representation replaces it.
			witness.text = lower ? lower->text : std::string(1, '\1');
			if (lower && !lower->inclusive) {
				witness.text.push_back('\1');
			}
		}
		return !upper || compare(witness, *upper) < 0
			|| (compare(witness, *upper) == 0 && upper->inclusive);
	}

	bool entails(const constraint_set& query) const {
		if (!supplied || !inhabited() || !query.valid || numeric != query.numeric) {
			return false;
		}
		if (query.lower && (!lower || compare(*lower, *query.lower) < 0
			|| (compare(*lower, *query.lower) == 0 && lower->inclusive && !query.lower->inclusive))) {
			return false;
		}
		if (query.upper && (!upper || compare(*upper, *query.upper) > 0
			|| (compare(*upper, *query.upper) == 0 && upper->inclusive && !query.upper->inclusive))) {
			return false;
		}
		return true;
	}
};

}

tagd::code relation_resolver::evaluate(tagd::predicate_set& result,
	tagd::id_view subject, tagd::errorable& errors) const {
	if (!_declarations.contains(subject)) {
		return tagd::TS_NOT_FOUND;
	}
	tagd::predicate_set effective;
	std::map<std::pair<tagd::id_string, tagd::id_string>, constraint_set> constraints;
	const auto& reached = sources(subject);
	for (const auto& id : reached) {
		const auto found = _declarations.find(id);
		if (found == _declarations.end()) {
			return errors.error(tagd::TS_INTERNAL_ERR, "unresolved inheritance source: " + id);
		}
		for (const auto& p : found->second.relations) {
			// Validate every contribution before normalization can absorb a bare assertion.
			constraints[{p.relator, p.object}].add(p);
			effective.insert(p);
		}
	}

	for (const auto& [pair, admitted] : constraints) {
		if (!admitted.inhabited()) {
			std::ostringstream text;
			text << "contradiction on " << subject << ":";
			// Original declarations supply diagnostics only when a constraint fails.
			for (const auto& id : reached) {
				for (const auto& p : _declarations.at(id).relations) {
					if (p.relator == pair.first && p.object == pair.second) {
						text << " [" << id << ' ' << p << ']';
					}
				}
			}
			return errors.error(admitted.valid ? tagd::TS_CONTRADICTION : tagd::TS_MISUSE, text.str());
		}
	}

	tagd::normalize_predicates(effective);
	result.swap(effective);
	return tagd::TAGD_OK;
}

bool relation_resolver::matches(const tagd::predicate_set& effective,
	const tagd::predicate& query) const {
	constraint_set required;
	std::map<std::pair<tagd::id_string, tagd::id_string>, constraint_set> admitted;
	required.add(query);
	for (const auto& p : effective) {
		if (query.modifier.empty()) {
			if ((query.relator.empty() || structurally_contains(query.relator, p.relator))
				&& (query.object.empty() || structurally_contains(query.object, p.object))) {
				return true;
			}
		} else if ((query.relator.empty() || p.relator == query.relator)
			&& (query.object.empty() || p.object == query.object)) {
			// A wildcard binds a concrete pair; constraints from different pairs never intersect.
			admitted[{p.relator, p.object}].add(p);
		}
	}
	return !query.modifier.empty() && std::any_of(admitted.begin(), admitted.end(),
		[&](const auto& pair) {
			return pair.second.entails(required);
		});
}

bool relation_resolver::matches_type_relation(tagd::id_view subject,
	const tagd::type_predicate& query) const {
	// The canonical relation expresses the reflexive order, including structural ancestry.
	if (query.relator == tagd::HARD_TAG_TYPE_OF) {
		return sources(subject).contains(query.object);
	}
	for (const auto& source : sources(subject)) {
		for (const auto& type : _declarations.at(source).type_relations) {
			if (structurally_contains(query.relator, type.relator)
				&& sources(type.object).contains(query.object)) {
				return true;
			}
		}
	}
	return false;
}

} // namespace tagdb
