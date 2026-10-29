#pragma once
#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include <sstream>

class EffectiveRelationsTester : public CxxTest::TestSuite {
	void declare(tagd::tagspace& ts, tagd::id_view id,
		tagd::id_view parent = tagd::HARD_TAG_ENTITY) {
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB, parent,
			tagd::POS_UNKNOWN)), tagd::TAGD_OK);
	}
	void predicate(tagd::tagspace& ts, tagd::id_view id, tagd::id_view object,
		tagd::id_view value = {}) {
		tagd::abstract_tag change(id);
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, object, value), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change), tagd::TAGD_OK);
	}
	void type_relation(tagd::tagspace& ts, tagd::id_view id, tagd::id_view object) {
		tagd::abstract_tag change(id);
		TS_ASSERT_EQUALS(change.type_relation(tagd::HARD_TAG_TYPE_OF, object), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change), tagd::TAGD_OK);
	}

public:
	void test_query_descriptions_match_get_for_contextual_tags_and_urls() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "context");
		declare(ts, "source");
		declare(ts, "subject");
		predicate(ts, "source", tagd::HARD_TAG_MESSAGE, "shared");
		predicate(ts, "subject", tagd::HARD_TAG_PATH, "local");
		type_relation(ts, "subject", "source");

		tagd::url url("https://example.com/subject");
		TS_ASSERT_EQUALS(url.type_relation(tagd::HARD_TAG_TYPE_OF, "source"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(url), tagd::TAGD_OK);
		for (const auto& ref : {
			tagd::referent("alias", "subject", "context"),
			tagd::referent("origin", "source", "context"),
			tagd::referent("has", tagd::HARD_TAG_HAS, "context"),
			tagd::referent("message", tagd::HARD_TAG_MESSAGE, "context"),
			tagd::referent("kind", tagd::HARD_TAG_TYPE_OF, "context"),
			tagd::referent("under", tagd::HARD_TAG_SUB, "context")}) {
			TS_ASSERT_EQUALS(ts.put(ref), tagd::TAGD_OK);
		}
		auto ssn = ts.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

		tagd::interrogator q(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "shared"), tagd::TAGD_OK);
		for (const tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_INCLUDE_INHERITED_PREDICATES),
			tagd::flags_t(tagd::F_NO_TRANSFORM_REFERENTS),
			tagd::flags_t(tagd::F_NO_TRANSFORM_REFERENTS | tagd::F_INCLUDE_INHERITED_PREDICATES)}) {
			tagd::tag_set found, expected;
			TS_ASSERT_EQUALS(ts.query(found, q, &ssn, flags), tagd::TAGD_OK);
			for (const auto& id : {std::string("source"), std::string("subject"), url.hduri()}) {
				tagd::abstract_tag get;
				TS_ASSERT_EQUALS(ts.get(get, id, &ssn, flags), tagd::TAGD_OK);
				expected.insert(std::move(get));
			}
			TS_ASSERT_EQUALS(found.size(), 3u);
			TS_ASSERT_EQUALS(found, expected);
		}

		q.relations.clear();
		TS_ASSERT_EQUALS(q.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_HAS), tagd::TAGD_OK);
		tagd::tag_set hard_tags;
		TS_ASSERT_EQUALS(ts.query(hard_tags, q, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(hard_tags.size(), 1u);
		if (!hard_tags.empty()) {
			TS_ASSERT_EQUALS(hard_tags.begin()->id(), tagd::HARD_TAG_HAS);
		}
	}

	void test_effective_predicates_are_independent_of_source_declaration_order() {
		tagd::predicate_set expected;
		for (const bool reverse : {false, true}) {
			tagd::tagspace::memory ts;
			TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
			declare(ts, "subject");
			const auto ids = reverse ? std::vector{"upper", "lower"} : std::vector{"lower", "upper"};
			for (const auto id : ids) {
				declare(ts, id);
				tagd::abstract_tag source(id);
				const bool lower = std::string_view(id) == "lower";
				TS_ASSERT_EQUALS(source.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE,
					lower ? "2" : "5", lower ? tagd::OP_GT : tagd::OP_LT,
					tagd::TYPE_INTEGER), tagd::TAGD_OK);
				TS_ASSERT_EQUALS(ts.put(source), tagd::TAGD_OK);
				type_relation(ts, "subject", id);
			}

			tagd::abstract_tag read;
			TS_ASSERT_EQUALS(ts.get(read, "subject", nullptr,
				tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(read.relations.size(), 2u);
			if (reverse) {
				TS_ASSERT_EQUALS(read.relations, expected);
			} else {
				expected = read.relations;
			}
		}
	}

	void test_expanded_get_returns_typed_relations_without_storing_them() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "integer", "floating", "dog"}) {
			declare(ts, id);
		}

		const tagd::predicate integer(tagd::HARD_TAG_HAS, "legs", "4",
			tagd::OP_EQ, tagd::TYPE_INTEGER);

		const tagd::predicate floating(tagd::HARD_TAG_HAS, "legs", "4.0",
			tagd::OP_EQ, tagd::TYPE_FLOAT);

		tagd::abstract_tag a("integer"), b("floating");
		TS_ASSERT_EQUALS(a.relation(integer), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(b.relation(floating), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(a), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(b), tagd::TAGD_OK);
		type_relation(ts, "integer", "floating");
		type_relation(ts, "dog", "integer");
		type_relation(ts, "dog", "floating");

		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);

		tagd::abstract_tag expanded, direct;
		TS_ASSERT_EQUALS(ts.get(expanded, "dog", nullptr,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(expanded.relations.size(), 2u);
		TS_ASSERT(expanded.relations.contains(integer));
		TS_ASSERT(expanded.relations.contains(floating));
		TS_ASSERT_EQUALS(expanded.type_relations.size(), 2u);
		TS_ASSERT_DIFFERS(expanded.str().find("legs = 4, legs = 4.0"), std::string::npos);
		TS_ASSERT_EQUALS(ts.get(direct, "dog"), tagd::TAGD_OK);
		TS_ASSERT(direct.relations.empty());
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.str(), before.str());

		tagd::abstract_tag remove("dog");
		TS_ASSERT_EQUALS(remove.type_relation(tagd::HARD_TAG_TYPE_OF, "integer"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(remove), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(direct, "dog", nullptr,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.relations.size(), 1u);
		TS_ASSERT(direct.relations.contains(floating));
		TS_ASSERT(!direct.relations.contains(integer));
		TS_ASSERT_EQUALS(expanded.relations.size(), 2u);
	}

	void test_root_contains_every_resolved_subject_and_predicate_component() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "legs");
		declare(ts, "animal");
		declare(ts, "dog", "animal");
		predicate(ts, "animal", tagd::HARD_TAG_ENTITY);
		predicate(ts, "dog", "legs");

		tagd::abstract_tag dog;
		TS_ASSERT_EQUALS(ts.get(dog, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.relations.size(), 1u);
		TS_ASSERT(dog.related(tagd::HARD_TAG_HAS, "legs"));

		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR, tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(query.relation(tagd::HARD_TAG_HAS, "legs"), tagd::TAGD_OK);

		tagd::tag_set found;
		TS_ASSERT_EQUALS(ts.query(found, query), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found.size(), 1u);
	}

	void test_valued_wildcards_bind_each_relation_pair_independently() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "blood");
		declare(ts, "animal");
		declare(ts, "dog", "animal");

		tagd::abstract_tag animal("animal");
		TS_ASSERT_EQUALS(animal.relation(tagd::HARD_TAG_HAS, "blood", "warm"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(animal.relation(tagd::HARD_TAG_RELATOR, "blood", "cold"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);

		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(query.relation({}, "blood", "warm"), tagd::TAGD_OK);

		tagd::tag_set found;
		TS_ASSERT_EQUALS(ts.query(found, query), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found.size(), 2u);
	}

	void test_type_relation_queries_use_the_integrated_reflexive_order() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "animal");
		declare(ts, "quadruped", "animal");
		declare(ts, "dog");
		declare(ts, "puppy", "dog");
		type_relation(ts, "dog", "quadruped");

		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(query.type_relation(tagd::HARD_TAG_TYPE_OF, "animal"), tagd::TAGD_OK);

		tagd::tag_set found;
		TS_ASSERT_EQUALS(ts.query(found, query), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found.size(), 4u);
		for (const auto& t : found) {
			TS_ASSERT(t.id() == "animal" || t.id() == "quadruped" || t.id() == "dog" || t.id() == "puppy");
		}
		query.type_relations.clear();
		TS_ASSERT_EQUALS(query.type_relation(tagd::HARD_TAG_TYPE_OF, "puppy"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.query(found, query), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(found.size(), 1u);
		if (!found.empty()) {
			TS_ASSERT_EQUALS(found.begin()->id(), "puppy");
		}
	}

	void test_crossed_component_ranks_do_not_absorb_assertions() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "part");
		declare(ts, "legs", "part");
		declare(ts, "ancestor");
		declare(ts, "child", "ancestor");

		tagd::abstract_tag ancestor("ancestor"), child("child");
		TS_ASSERT_EQUALS(ancestor.relation(tagd::HARD_TAG_RELATOR, "legs"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_HAS, "part"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(ancestor), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(child), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(child, "child", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(child.relations.size(), 2u);
	}

	void test_reparenting_recomputes_sources_and_keeps_direct_type_relations() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"old", "new", "kept", "before", "after", "shared"}) {
			declare(ts, id);
		}
		declare(ts, "child", "old");
		declare(ts, "grandchild", "child");
		predicate(ts, "old", "before");
		predicate(ts, "new", "after");
		predicate(ts, "kept", "shared");
		type_relation(ts, "child", "kept");
		type_relation(ts, "kept", "old");
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("child", tagd::HARD_TAG_SUB, "new",
			tagd::POS_UNKNOWN)), tagd::TAGD_OK);

		tagd::abstract_tag read;
		TS_ASSERT_EQUALS(ts.get(read, "grandchild", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT(read.related(tagd::HARD_TAG_HAS, "before"));
		TS_ASSERT(read.related(tagd::HARD_TAG_HAS, "after"));
		TS_ASSERT(read.related(tagd::HARD_TAG_HAS, "shared"));

		tagd::abstract_tag remove("kept");
		TS_ASSERT_EQUALS(remove.type_relation(tagd::HARD_TAG_TYPE_OF, "old"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(remove), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(read, "grandchild", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT(!read.related(tagd::HARD_TAG_HAS, "before"));
		TS_ASSERT_EQUALS(ts.get(read, "child"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.type_relations.size(), 1u);
		TS_ASSERT(read.relations.empty());
	}

	void test_local_modifiers_do_not_mask_predicates_from_type_relations() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "source", "dog"}) {
			declare(ts, id);
		}

		tagd::abstract_tag source("source"), local("dog");
		TS_ASSERT_EQUALS(source.relation(tagd::HARD_TAG_HAS, "legs", "4", tagd::OP_EQ,
			tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(source), tagd::TAGD_OK);
		type_relation(ts, "dog", "source");
		for (const auto type : {tagd::TYPE_INTEGER, tagd::TYPE_FLOAT}) {
			local.relations.clear();
			TS_ASSERT_EQUALS(local.relation(tagd::HARD_TAG_HAS, "legs", "4", tagd::OP_EQ, type), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.put(local), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.put(local), tagd::TS_DUPLICATE);
			TS_ASSERT_EQUALS(ts.put(local, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
		}

		tagd::abstract_tag remove("dog");
		remove.relations = source.relations;
		TS_ASSERT_EQUALS(ts.del(remove), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(ts.del(local), tagd::TAGD_OK);

		tagd::abstract_tag read;
		TS_ASSERT_EQUALS(ts.get(read, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations.size(), 1u);
		TS_ASSERT_EQUALS(read.relations.begin()->modifier, "4");
		TS_ASSERT_EQUALS(ts.get(read, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations.size(), 1u);
		TS_ASSERT(read.relations.begin()->modifier.empty());
	}

	void test_normalization_specializes_bare_pairs_and_keeps_valued_pairs() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "body_part");
		declare(ts, "legs", "body_part");
		declare(ts, "animal");
		declare(ts, "dog", "animal");

		tagd::abstract_tag animal("animal"), dog("dog");
		TS_ASSERT_EQUALS(animal.relation(tagd::HARD_TAG_RELATOR, "body_part"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.relation(tagd::HARD_TAG_HAS, "legs", "four"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(dog), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(animal, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(dog, "dog"), tagd::TAGD_OK);

		tagd::abstract_tag read;
		TS_ASSERT_EQUALS(ts.get(read, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations, dog.relations);

		// Valued constraints on unequal pairs are independent, even when the ranks contain one another.
		animal.relations.clear();
		TS_ASSERT_EQUALS(animal.relation(tagd::HARD_TAG_RELATOR, "body_part", "two"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(read, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations.size(), 2u);

		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(query.relation(tagd::HARD_TAG_RELATOR, "body_part", "four"), tagd::TAGD_OK);

		tagd::tag_set found;
		TS_ASSERT_EQUALS(ts.query(found, query), tagd::TS_NOT_FOUND);
	}

	void test_reparenting_cannot_invalidate_existing_relator_roles() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "kind", tagd::HARD_TAG_TYPE_OF);
		declare(ts, "source");
		declare(ts, "dog");

		tagd::abstract_tag dog("dog");
		TS_ASSERT_EQUALS(dog.type_relation("kind", "source"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(dog), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("kind", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_RELATOR, tagd::POS_UNKNOWN)), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.pos("kind"), tagd::POS_TYPE_RELATOR);
		declare(ts, "verb", tagd::HARD_TAG_RELATOR);
		dog.type_relations.clear();
		TS_ASSERT_EQUALS(dog.relation("verb", "source"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(dog), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("verb", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_TYPE_OF, tagd::POS_UNKNOWN)), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.pos("verb"), tagd::POS_RELATOR);
	}

	void test_existing_modifier_metadata_survives_storage_and_effective_resolution() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "integer_source", "float_source", "dog"}) {
			declare(ts, id);
		}

		tagd::abstract_tag integer("integer_source"), floating("float_source");
		TS_ASSERT_EQUALS(integer.relation(tagd::HARD_TAG_HAS, "legs", "4",
			tagd::OP_EQ, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(floating.relation(tagd::HARD_TAG_HAS, "legs", "4.0",
			tagd::OP_EQ, tagd::TYPE_FLOAT), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(integer), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(floating), tagd::TAGD_OK);
		type_relation(ts, "dog", "integer_source");
		type_relation(ts, "dog", "float_source");

		tagd::abstract_tag read;
		TS_ASSERT_EQUALS(ts.get(read, "integer_source"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations.begin()->modifier_type, tagd::TYPE_INTEGER);
		TS_ASSERT_EQUALS(ts.get(read, "dog", nullptr, tagd::F_INCLUDE_INHERITED_PREDICATES),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(read.relations.size(), 2u);

		tagd::abstract_tag string_value("dog");
		TS_ASSERT_EQUALS(string_value.relation(tagd::HARD_TAG_HAS, "legs", "4"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(string_value), tagd::TS_CONTRADICTION);
	}

	void test_constraint_conjunction_requires_a_witness_and_query_entailment() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "lower", "upper", "dog"}) {
			declare(ts, id);
		}

		tagd::abstract_tag lower("lower"), upper("upper");
		TS_ASSERT_EQUALS(lower.relation(tagd::HARD_TAG_HAS, "legs", "2",
			tagd::OP_GT, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(upper.relation(tagd::HARD_TAG_HAS, "legs", "5",
			tagd::OP_LT, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(lower), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(upper), tagd::TAGD_OK);
		type_relation(ts, "dog", "lower");
		type_relation(ts, "dog", "upper");

		tagd::tag_set found;
		tagd::interrogator q(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, "legs", "3",
			tagd::OP_EQ, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.query(found, q), tagd::TS_NOT_FOUND);
		q.relations.clear();
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, "legs", "1",
			tagd::OP_GT, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.query(found, q), tagd::TAGD_OK);
		TS_ASSERT(std::any_of(found.begin(), found.end(), [](const auto& t) { return t.id() == "dog"; }));
		upper.relations.clear();
		TS_ASSERT_EQUALS(upper.relation(tagd::HARD_TAG_HAS, "legs", "2",
			tagd::OP_LT_EQ, tagd::TYPE_INTEGER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(upper), tagd::TS_CONTRADICTION);
	}

	void test_subordinate_and_type_relation_diamonds_preserve_declarations() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"cells", "locomotion", "legs", "animal", "mobile", "named"}) {
			declare(ts, id);
		}
		declare(ts, "quadruped", "animal");
		declare(ts, "dog", "named");
		declare(ts, "BrayBoy", "dog");
		predicate(ts, "animal", "cells");
		predicate(ts, "mobile", "locomotion");
		predicate(ts, "quadruped", "legs", "four");
		type_relation(ts, "animal", "mobile");
		type_relation(ts, "quadruped", "mobile");
		type_relation(ts, "dog", "quadruped");
		predicate(ts, "BrayBoy", "legs");

		tagd::abstract_tag direct, effective;
		TS_ASSERT_EQUALS(ts.get(direct, "BrayBoy"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.relations.size(), 1u);
		TS_ASSERT(direct.type_relations.empty());

		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(effective, "BrayBoy", nullptr,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(effective.relations.size(), 3u);
		TS_ASSERT(effective.related(tagd::HARD_TAG_HAS, "cells"));
		TS_ASSERT(effective.related(tagd::HARD_TAG_HAS, "locomotion"));
		TS_ASSERT(effective.related(tagd::HARD_TAG_HAS, "legs", "four"));
		TS_ASSERT(effective.type_relations.empty());
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(before.str(), after.str());
	}

	void test_query_matches_effective_conjunction_in_both_display_modes() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "cells", "animal", "mobile", "dog"}) {
			declare(ts, id);
		}
		predicate(ts, "animal", "cells");
		predicate(ts, "mobile", "legs", "four");
		type_relation(ts, "dog", "animal");
		type_relation(ts, "dog", "mobile");

		tagd::interrogator q(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, "cells"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(q.relation(tagd::HARD_TAG_HAS, "legs", "four"), tagd::TAGD_OK);
		for (tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_INCLUDE_INHERITED_PREDICATES)}) {
			tagd::tag_set found;
			TS_ASSERT_EQUALS(ts.query(found, q, nullptr, flags), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(found.size(), 1u);
			if (!found.empty()) {
				TS_ASSERT_EQUALS(found.begin()->id(), "dog");
				TS_ASSERT_EQUALS(found.begin()->relations.size(), flags ? 2u : 0u);
			}
		}
	}

	void test_source_updates_validate_remote_dependents_and_roll_back_every_change() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "source", "other", "dog"}) {
			declare(ts, id);
		}
		predicate(ts, "source", "legs", "four");
		predicate(ts, "other", "legs", "four");
		type_relation(ts, "dog", "source");
		type_relation(ts, "dog", "other");
		declare(ts, "puppy", "dog");

		tagd::abstract_tag change("source");
		TS_ASSERT_EQUALS(change.relation(tagd::HARD_TAG_HAS, "legs", "two"), tagd::TAGD_OK);

		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(change, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TS_CONTRADICTION);
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.str(), before.str());

		tagd::abstract_tag parent("parent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY,
			tagd::POS_TAG);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, "legs", "two"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(parent), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("source", tagd::HARD_TAG_SUB,
			"parent", tagd::POS_TAG)), tagd::TS_CONTRADICTION);
	}

	void test_local_deletion_cannot_retract_predicates_from_type_relations() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"legs", "source", "dog"}) {
			declare(ts, id);
		}
		predicate(ts, "source", "legs", "four");
		type_relation(ts, "dog", "source");

		tagd::abstract_tag removal("dog");
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS, "legs"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(removal), tagd::TS_MISUSE);
		predicate(ts, "dog", "legs");
		TS_ASSERT_EQUALS(ts.del(removal), tagd::TAGD_OK);
		predicate(ts, "dog", "legs");
		removal.relations.clear();
		TS_ASSERT_EQUALS(removal.type_relation(tagd::HARD_TAG_TYPE_OF, "source"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(removal), tagd::TAGD_OK);

		tagd::abstract_tag effective;
		TS_ASSERT_EQUALS(ts.get(effective, "dog", nullptr,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT(effective.related(tagd::HARD_TAG_HAS, "legs"));
	}
};
