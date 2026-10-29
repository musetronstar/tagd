#pragma once
#include "tagd.h"
#include <map>
#include <set>

namespace tagdb {

// One operation owns canonical declarations and ranks from a single storage snapshot.
class relation_resolver {
public:
	using declarations_t = std::map<tagd::id_string, tagd::abstract_tag, std::less<>>;
	using ranks_t = std::map<tagd::id_string, tagd::rank, std::less<>>;

	relation_resolver() = default;
	relation_resolver(declarations_t declarations, ranks_t ranks)
		: _declarations(std::move(declarations)), _ranks(std::move(ranks)) {
	}

	const declarations_t& declarations() const {
		return _declarations;
	}
	const std::set<tagd::id_string>& sources(tagd::id_view) const;
	bool structurally_contains(tagd::id_view general, tagd::id_view specific) const;
	[[nodiscard]] tagd::code evaluate(tagd::predicate_set&, tagd::id_view, tagd::errorable&) const;
	bool matches(const tagd::predicate_set&, const tagd::predicate&) const;
	bool matches_type_relation(tagd::id_view subject, const tagd::type_predicate&) const;

private:
	declarations_t _declarations;
	ranks_t _ranks;
	mutable std::map<tagd::id_string, std::set<tagd::id_string>, std::less<>> _sources;
};

} // namespace tagdb
