#include "tagd.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {

// Fail one allocation at each position, then permit cleanup and rollback to run.
int remaining_allocations = -1;
constexpr int allocation_positions = 48;

void* allocate(std::size_t bytes) {
	if (remaining_allocations == 0) {
		remaining_allocations = -1;
		throw std::bad_alloc();
	}

	if (remaining_allocations > 0)
		--remaining_allocations;

	if (void* memory = std::malloc(bytes ? bytes : 1))
		return memory;

	throw std::bad_alloc();
}

tagd::predicate_set ranked(const char* relator) {
	tagd::predicate_set values{{relator, "object", "value"}};
	const auto status = values.order([](tagd::rank& r, tagd::id_view) {
		return r.init("a");
	});
	assert(status == tagd::TAGD_OK);
	return values;
}

} // namespace

// Isolated executable: allocation injection must not affect the other test runners.
void* operator new(std::size_t bytes) {
	return allocate(bytes);
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
	std::free(memory);
}

int main() {
	int merge_failures = 0, node_failures = 0;
	for (int position = 0; position < allocation_positions; ++position) {
		auto source = ranked("source");
		auto target = ranked("target");
		const auto* address = &*source.begin();

		remaining_allocations = position;
		try {
			target.merge(source);
		} catch (const std::bad_alloc&) {
			++merge_failures;
		}
		remaining_allocations = -1;

		assert(source.size() + target.size() == 2);
		const tagd::predicate key("source", "object", "value");
		const auto& owner = source.empty() ? target : source;
		assert(owner.contains(key));
		assert(&*owner.find(key) == address);
		assert(owner.lower_bound(key) != owner.end());

		source = ranked("source");
		target = ranked("target");
		auto node = source.extract(source.begin());
		address = &node.value();

		remaining_allocations = position;
		try {
			auto result = target.insert(std::move(node));
			assert(result.inserted);
			assert(&*result.position == address);
		} catch (const std::bad_alloc&) {
			++node_failures;
			assert(!node.empty());
			assert(&node.value() == address);
			assert(target.size() == 1);
		}
		remaining_allocations = -1;

		assert(target.contains({"target", "object", "value"}));
		assert(target.contains(key) == node.empty());
	}

	assert(merge_failures > 0 && merge_failures < allocation_positions);
	assert(node_failures > 0 && node_failures < allocation_positions);
	std::cout << "Predicate allocation failures preserve node ownership\n";
}
