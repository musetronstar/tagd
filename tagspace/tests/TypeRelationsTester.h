#pragma once

#include <cxxtest/TestSuite.h>
#include "tagspace.h"
#include <sstream>

class TypeRelationsTester : public CxxTest::TestSuite {
	void declare(tagd::tagspace& ts, tagd::id_view id,
		tagd::id_view parent = tagd::HARD_TAG_ENTITY) {
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB, parent, tagd::POS_UNKNOWN)),
			tagd::TAGD_OK);
	}

public:
	void test_structural_reparenting_rejects_a_descendant_parent_atomically() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "a");
		declare(ts, "b", "a");
		declare(ts, "c", "b");

		tagd::abstract_tag original, restored;
		TS_ASSERT_EQUALS(ts.get(original, "a"), tagd::TAGD_OK);
		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);
		for (const tagd::flags_t flags : {0u, tagd::flags_t(tagd::F_IGNORE_DUPLICATES)}) {
			TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("a", tagd::HARD_TAG_SUB, "c",
				tagd::POS_UNKNOWN), nullptr, flags), tagd::TS_MISUSE);
		}

		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.str(), before.str());
		TS_ASSERT_EQUALS(ts.get(restored, "a"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(restored.super_object(), original.super_object());
		TS_ASSERT_EQUALS(restored.rank(), original.rank());
	}

	void test_mixed_cycles_and_reparenting_preserve_original_state() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "animal");
		declare(ts, "dog", "animal");
		declare(ts, "mobile");

		tagd::abstract_tag assignment("dog"), reverse("mobile"), stored;
		TS_ASSERT_EQUALS(assignment.type_relation(tagd::HARD_TAG_TYPE_OF, "mobile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(assignment), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(reverse.type_relation(tagd::HARD_TAG_TYPE_OF, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(reverse), tagd::TAGD_OK);

		tagd::abstract_tag cycle("animal");
		TS_ASSERT_EQUALS(cycle.type_relation(tagd::HARD_TAG_TYPE_OF, "dog"), tagd::TAGD_OK);

		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(cycle, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("mobile", tagd::HARD_TAG_SUB, "dog",
			tagd::POS_UNKNOWN)), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(before.str(), after.str());

		// A currently implied direct edge is retained because a later move may need it.
		assignment.type_relations.clear();
		TS_ASSERT_EQUALS(assignment.type_relation(tagd::HARD_TAG_TYPE_OF, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(assignment), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(stored, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(stored.type_relations.size(), 2u);
	}

	void test_type_relations_preserve_identity_and_references() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.pos(tagd::HARD_TAG_TYPE_OF), tagd::POS_TYPE_RELATOR);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("classifies", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_TYPE_OF, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.pos("classifies"), tagd::POS_TYPE_RELATOR);
		declare(ts, "animal");
		declare(ts, "quadruped", "animal");
		declare(ts, "dog", "animal");

		tagd::abstract_tag before, after, assertion("dog");
		TS_ASSERT_EQUALS(ts.get(before, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(assertion.type_relation("classifies", "quadruped"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(assertion), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(after, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.rank(), before.rank());
		TS_ASSERT_EQUALS(after.super_object(), "animal");
		TS_ASSERT_EQUALS(after.type_relations, assertion.type_relations);
		TS_ASSERT(after.relations.empty());
		TS_ASSERT_EQUALS(ts.put(assertion), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(ts.put(assertion, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(tagd::abstract_tag("quadruped")), tagd::TS_RELATION_DEPENDENCY);
		TS_ASSERT_EQUALS(ts.del(tagd::abstract_tag("classifies")), tagd::TS_RELATION_DEPENDENCY);
		TS_ASSERT_EQUALS(ts.del(assertion), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(after, "dog"), tagd::TAGD_OK);
		TS_ASSERT(after.type_relations.empty());
		TS_ASSERT_EQUALS(ts.del(assertion), tagd::TS_NOT_FOUND);
	}

	void test_self_and_root_type_relations_are_validated_noops() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "animal");
		declare(ts, "dog", "animal");

		tagd::abstract_tag animal("animal");
		TS_ASSERT_EQUALS(animal.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "inherited"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);

		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);

		for (const tagd::id_view object : {tagd::id_view("dog"), tagd::HARD_TAG_ENTITY}) {
			tagd::abstract_tag axiom("dog"), stored;
			TS_ASSERT_EQUALS(axiom.type_relation(tagd::HARD_TAG_TYPE_OF, object), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.put(axiom), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.put(axiom, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.get(stored, "dog"), tagd::TAGD_OK);
			TS_ASSERT(stored.type_relations.empty());
		}

		tagd::abstract_tag invalid("dog");
		TS_ASSERT_EQUALS(invalid.type_relation(tagd::HARD_TAG_TYPE_OF, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(invalid.type_relation(tagd::HARD_TAG_TYPE_OF, "missing"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid), tagd::TS_OBJECT_UNK);
		invalid.type_relations.erase(tagd::type_predicate(tagd::HARD_TAG_TYPE_OF, "missing"));
		TS_ASSERT_EQUALS(invalid.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "conflict"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TS_CONTRADICTION);

		tagd::abstract_tag cycle("animal");
		TS_ASSERT_EQUALS(cycle.type_relation(tagd::HARD_TAG_TYPE_OF, tagd::HARD_TAG_ENTITY), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(cycle.type_relation(tagd::HARD_TAG_TYPE_OF, "dog"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(cycle), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.str(), before.str());

		// Axioms must not make an unchanged local assertion count as a new declaration.
		invalid.relations.clear();
		TS_ASSERT_EQUALS(invalid.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "inherited"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid), tagd::TS_DUPLICATE);
		TS_ASSERT_EQUALS(ts.put(invalid, nullptr, tagd::F_IGNORE_DUPLICATES), tagd::TAGD_OK);
		invalid.relations.clear();
		TS_ASSERT_EQUALS(invalid.type_relation(tagd::HARD_TAG_TYPE_OF, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(invalid), tagd::TS_DUPLICATE);
	}

	void test_type_relation_validation_is_atomic() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		declare(ts, "dog");
		declare(ts, "mobile");

		tagd::abstract_tag assertion("dog"), stored;
		TS_ASSERT_EQUALS(assertion.type_relation(tagd::HARD_TAG_TYPE_OF, "mobile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(assertion.type_relation(tagd::HARD_TAG_TYPE_OF, "missing"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(assertion), tagd::TS_OBJECT_UNK);
		TS_ASSERT_EQUALS(ts.get(stored, "dog"), tagd::TAGD_OK);
		TS_ASSERT(stored.type_relations.empty());
		assertion.type_relations.clear();
		TS_ASSERT_EQUALS(assertion.type_relation(tagd::HARD_TAG_HAS, "mobile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(assertion), tagd::TS_MISUSE);

		tagd::abstract_tag no_parent("new_dog");
		TS_ASSERT_EQUALS(no_parent.type_relation(tagd::HARD_TAG_TYPE_OF, "mobile"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(no_parent), tagd::TS_SUB_UNK);
		TS_ASSERT(!ts.exists("new_dog"));
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("bad_parent", tagd::HARD_TAG_TYPE_OF,
			"mobile", tagd::POS_UNKNOWN)), tagd::TS_MISUSE);
	}
};
