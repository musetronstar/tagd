# Tagspace Architecture

"To be, is to be related"

## 1. Mathematical Foundation

A tagspace is a pair (T, ⊑):

* T = set of tags
* ⊑ = prefix partial order induced by rank

From this, the Alexandrov topology is defined:

* Open sets = upward-closed sets
* Each tag has a smallest neighborhood

This structure is not metaphorical; it is implemented directly in code.

## 2. Two-Layer Structure

Each tag consists of:

* base space: rank (position in tree)
* fiber: predicate_set (horizontal relations)

This corresponds to:

* topology (vertical structure)
* attached data (horizontal structure)

## 3. C++ Mapping

### Value Types (`tagd`)

* `abstract_tag`
* `predicate`
* `rank`

Properties:

* immutable identity
* STL-compatible
* no dependency on tagspace

### Structural Interface (`tagspace`)

Provides:

```cpp
rank lookup_rank(id_view) const;
part_of_speech lookup_pos(id_view) const;
```

And operations:

```cpp
rank_comparator()
ordered_view()
subtree_view(tag)
neighborhood_view(tag)
children_view(tag)
parent_of(tag)
```

## 4. Ordering Model

Two modes exist:

### Mode 1 - Lexical (fallback)

Used when tags are unranked.

### Mode 2 - Rank-based (primary)

Used when tags are activated within a tagspace by setting rank to a non-empty value.

Rule:

> A tag is either fully unranked or fully ranked. No mixed mode.

## 5. STL Integration

All structures must be compatible with:

* `std::ranges`
* `std::sort`
* `std::set`

Design:

* data = passive
* algorithms = external
* ordering = provided by tagspace

Example:

```cpp
std::ranges::sort(predicates, ts.rank_comparator());
```

## 6. Views (Topological Operations)

All views are lazy ranges.

Definitions:

* `ordered_view()` -> canonical traversal
* `subtree_view(tag)` -> downward-closed (closed set in Alexandrov topology)
* `neighborhood_view(tag)` -> upward-closed (open set in Alexandrov topology)
* `children_view(tag)` -> immediate descendants

These correspond directly to:

* downward-closed sets
* upward-closed sets

## 7. Hard Tagspace

The hard tagspace is the invariant base subspace shared by all tagspaces.

It is:

* constexpr-defined
* structurally embedded
* not a runtime singleton

All tagspaces extend this base.

## 8. Design Invariants

* Identity is structural (rank)
* Tags are immutable once constructed
* No stringly-typed logic
* No hidden global state
* All structure is explicit

## 9. Summary

The system implements:

* set theory (tags)
* partial order (rank)
* topology (Alexandrov)
* fibered structure (predicates)

C++ expresses this via:

* value types (`tagd`)
* structural interface (`tagspace`)
* STL algorithms (composition)

This alignment ensures correctness, composability, and performance.
