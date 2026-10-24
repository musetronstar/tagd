#include "tagd.h"

#include <cassert>
#include <iostream>

namespace {

tagd::predicate_set ranked(const char* relator) {
	tagd::predicate_set values{{relator, "object", "value"}};
	const auto status = values.order([](tagd::rank& r, tagd::id_view) {
		return r.init("a");
	});
	assert(status == tagd::TAGD_OK);
	return values;
}

} // namespace

int main() {
	auto source = ranked("source");
	auto target = ranked("target");
	const auto* address = &*source.begin();

	target.merge(source);
	const tagd::predicate key("source", "object", "value");
	assert(source.empty());
	assert(target.size() == 2);
	assert(&*target.find(key) == address);
	assert(target.lower_bound(key) != target.end());

	source = ranked("source");
	target = ranked("target");
	auto node = source.extract(source.begin());
	address = &node.value();
	auto result = target.insert(std::move(node));
	assert(result.inserted);
	assert(&*result.position == address);
	assert(target.contains(key));
	assert(target.contains({"target", "object", "value"}));

	source = ranked("source");
	address = &*source.begin();
	target.merge(source);
	assert(source.size() == 1);
	assert(&*source.begin() == address);

	node = source.extract(source.begin());
	auto rejected = target.insert(std::move(node));
	assert(!rejected.inserted);
	assert(!rejected.node.empty());
	assert(&rejected.node.value() == address);
	assert(target.size() == 2);

	std::cout << "Predicate transfers preserve node ownership\n";
}
