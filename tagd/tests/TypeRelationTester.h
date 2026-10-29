#include <cxxtest/TestSuite.h>
#include "tagd.h"

class TypeRelationTester : public CxxTest::TestSuite {
public:
	void test_type_relation_membership_survives_copy_move_reorder_and_erasure() {
		tagd::type_predicate_set values{{"r", "a"}, {"r", "b"}};
		TS_ASSERT_EQUALS(values.order([](tagd::rank& rank, tagd::id_view id) {
			return rank.init(id == "a" ? "z" : "a");
		}), tagd::TAGD_OK);
		auto copied = values;
		tagd::type_predicate_set assigned;
		assigned = copied;
		values.clear();
		TS_ASSERT(copied.contains({"r", "a"}));
		TS_ASSERT_EQUALS(assigned, copied);

		auto moved = std::move(copied);
		TS_ASSERT(moved.contains({"r", "a"}));
		TS_ASSERT_EQUALS(moved.erase({"r", "a"}), 1u);
		TS_ASSERT(!moved.contains({"r", "a"}));
		TS_ASSERT(assigned.contains({"r", "a"}));
		moved.erase(moved.begin());
		TS_ASSERT(moved.empty());
		TS_ASSERT(!moved.contains({"r", "b"}));

		moved = std::move(assigned);
		moved.swap(values);
		TS_ASSERT(values.contains({"r", "a"}));
		values.transform([](const tagd::id_string& id) { return "alias_" + id; });
		TS_ASSERT(!values.contains({"r", "a"}));
		TS_ASSERT(values.contains({"alias_r", "alias_a"}));
	}

	void test_normalization_preserves_mixed_ranked_and_unranked_component_identity() {
		tagd::predicate_set ranked{{"specific", "part", "4"}};
		TS_ASSERT_EQUALS(ranked.order([](tagd::rank& rank, tagd::id_view id) {
			return rank.init(id == "specific" ? "ab" : "cd");
		}), tagd::TAGD_OK);
		tagd::predicate_set general{{"general", "whole"}};
		TS_ASSERT_EQUALS(general.order([](tagd::rank& rank, tagd::id_view id) {
			return rank.init(id == "general" ? "a" : "c");
		}), tagd::TAGD_OK);
		general.insert({"specific", "part"});
		general.insert({"unknown", "part"});
		general.insert(ranked.begin(), ranked.end());

		tagd::normalize_predicates(general);
		TS_ASSERT_EQUALS(general.size(), 2u);
		TS_ASSERT(general.contains({"specific", "part", "4"}));
		TS_ASSERT(general.contains({"unknown", "part"}));
		auto normalized = general;
		tagd::normalize_predicates(general);
		TS_ASSERT_EQUALS(general, normalized);
	}

	void test_root_identity_has_irreflexive_container_ordering() {
		tagd::abstract_tag root(tagd::HARD_TAG_ENTITY, tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT(!(root < root));

		tagd::tag_set roots{root, root};
		TS_ASSERT_EQUALS(roots.size(), 1u);
	}

	void test_declarations_have_independent_value_semantics() {
		tagd::abstract_tag original("dog", tagd::HARD_TAG_IS_A, "mammal", tagd::POS_TAG);
		TS_ASSERT_EQUALS(original.type_relation(tagd::HARD_TAG_TYPE_OF, "quadruped"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(original.type_relation(tagd::HARD_TAG_TYPE_OF, "quadruped"), tagd::TAG_DUPLICATE);
		TS_ASSERT_EQUALS(original.type_relation({}, "quadruped"), tagd::TAG_ILLEGAL);
		TS_ASSERT_EQUALS(original.type_relation(tagd::HARD_TAG_TYPE_OF, {}), tagd::TAG_ILLEGAL);
		TS_ASSERT(original.relations.empty());

		tagd::abstract_tag copied = original;
		TS_ASSERT_EQUALS(copied, original);
		TS_ASSERT_EQUALS(copied.type_relations.erase({tagd::HARD_TAG_TYPE_OF, "quadruped"}), 1u);
		TS_ASSERT_DIFFERS(copied, original);
		TS_ASSERT_EQUALS(original.type_relations.size(), 1u);

		tagd::abstract_tag assigned;
		assigned = original;
		tagd::abstract_tag moved = std::move(assigned);
		TS_ASSERT_EQUALS(moved, original);
		tagd::rank rank;
		TS_ASSERT_EQUALS(rank.init("abc"), tagd::TAGD_OK);

		tagd::abstract_tag ranked(original, rank);
		TS_ASSERT_EQUALS(ranked.type_relations, original.type_relations);
		moved.clear();
		TS_ASSERT(moved.empty());
		TS_ASSERT(moved.type_relations.empty());
		swap(moved, copied);
		TS_ASSERT_EQUALS(moved.id(), "dog");
	}

	void test_type_relations_serialize_without_modifiers() {
		tagd::abstract_tag dog("dog", tagd::HARD_TAG_IS_A, "mammal", tagd::POS_TAG);
		TS_ASSERT_EQUALS(dog.type_relation(tagd::HARD_TAG_TYPE_OF, "quadruped"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.type_relation(tagd::HARD_TAG_TYPE_OF, "mobile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.relation("_has", "legs", "4"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(dog.str(), "dog _is_a mammal\n_type_of mobile, quadruped\n_has legs = \"4\"");
	}

	void test_rank_order_is_owned_and_lookup_failure_is_atomic() {
		tagd::type_predicate_set values{{"classifies", "a"}, {"classifies", "z"}};
		auto lookup = [](tagd::rank& rank, tagd::id_view id) {
			return rank.init(id == "z" ? "a" : "z");
		};
		TS_ASSERT_EQUALS(values.order(lookup), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(values.begin()->object, "z");
		TS_ASSERT(values.contains({"classifies", "a"}));
		TS_ASSERT(!values.insert({"classifies", "a"}).second);
		auto copied = values;
		TS_ASSERT_EQUALS(values.order([](tagd::rank&, tagd::id_view) {
			return tagd::TS_NOT_FOUND;
		}), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(values, copied);
		TS_ASSERT_EQUALS(values.begin()->object, "z");

		values.transform([](const tagd::id_string& id) {
			return "alias_" + id;
		});
		TS_ASSERT_EQUALS(values.begin()->object, "alias_z");
		TS_ASSERT(values.contains({"alias_classifies", "alias_a"}));
		TS_ASSERT_EQUALS(copied.begin()->object, "z");
	}

	void test_merging_tags_retains_type_declarations() {
		tagd::abstract_tag a("dog", tagd::HARD_TAG_IS_A, "mammal", tagd::POS_TAG);

		tagd::abstract_tag b = a;
		TS_ASSERT_EQUALS(a.type_relation(tagd::HARD_TAG_TYPE_OF, "quadruped"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(b.type_relation(tagd::HARD_TAG_TYPE_OF, "mobile"), tagd::TAGD_OK);

		tagd::tag_set left{a}, right{b};
		tagd::merge_tags(left, right);
		TS_ASSERT_EQUALS(left.size(), 1u);
		TS_ASSERT_EQUALS(left.begin()->type_relations.size(), 2u);
	}
};
