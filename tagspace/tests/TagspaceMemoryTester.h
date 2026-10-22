#pragma once

#include "tagspace.h"
#include "tagd/hard-tags.h"

#include <initializer_list>
#include <sstream>

#include <cxxtest/TestSuite.h>

class TagspaceMemoryTester : public CxxTest::TestSuite {
public:
	void test_modifier_referent_collisions_remain_literal() {
		for (const auto literal : {"perro", "dog"}) {
			tagd::tagspace::memory ts;
			TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
			for (const auto id : {"context", "dog", "subject"}) {
				TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
					tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
			}
			for (const auto& ref : {
				tagd::referent("perro", "dog", "context"),
				tagd::referent("selected", "subject", "context"),
				tagd::referent("says", tagd::HARD_TAG_HAS, "context"),
				tagd::referent("text", tagd::HARD_TAG_MESSAGE, "context")}) {
				TS_ASSERT_EQUALS(ts.put(ref), tagd::TAGD_OK);
			}

			auto ssn = ts.get_session();
			TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

			tagd::abstract_tag input("selected"), resolved;
			TS_ASSERT_EQUALS(input.relation("says", "text", literal), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.resolve_referents(resolved, input, &ssn), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(resolved.id(), "subject");
			TS_ASSERT(resolved.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, literal));
			TS_ASSERT_EQUALS(ts.put(input, &ssn), tagd::TAGD_OK);

			tagd::abstract_tag stored, encoded;
			TS_ASSERT_EQUALS(ts.get(stored, "subject"), tagd::TAGD_OK);
			TS_ASSERT(stored.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, literal));
			TS_ASSERT_EQUALS(ts.get(encoded, "selected", &ssn), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(encoded.id(), "selected");
			TS_ASSERT(encoded.related("says", "text", literal));
			TS_ASSERT_EQUALS(ts.get(stored, "subject", &ssn,
				tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);
			TS_ASSERT(stored.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, literal));

			TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("child", tagd::HARD_TAG_SUB,
				"subject", tagd::POS_TAG)), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.get(encoded, "child", &ssn,
				tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
			TS_ASSERT(encoded.related("says", "text", literal));

			tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
			TS_ASSERT_EQUALS(query.relation("says", "text", literal), tagd::TAGD_OK);

			tagd::tag_set results;
			TS_ASSERT_EQUALS(ts.query(results, query, &ssn), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(results.size(), 1);
			for (const auto& result : results) {
				TS_ASSERT_EQUALS(result.id(), "selected");
				TS_ASSERT(result.related("says", "text", literal));
			}

			// Neither spelling may match the other through a contextual referent.
			const auto other = std::string_view(literal) == "perro" ? "dog" : "perro";
			query.relations.clear();
			TS_ASSERT_EQUALS(query.relation("says", "text", other), tagd::TAGD_OK);
			results.clear();
			TS_ASSERT_EQUALS(ts.query(results, query, &ssn), tagd::TS_NOT_FOUND);
			TS_ASSERT(results.empty());

			tagd::abstract_tag removal("selected");
			removal.relations = query.relations;
			TS_ASSERT_EQUALS(ts.del(removal, &ssn), tagd::TS_NOT_FOUND);
			TS_ASSERT_EQUALS(ts.get(stored, "subject"), tagd::TAGD_OK);
			TS_ASSERT(stored.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, literal));

			std::ostringstream dump;
			TS_ASSERT_EQUALS(ts.dump(dump), tagd::TAGD_OK);

			tagd::abstract_tag expected("subject");
			TS_ASSERT_EQUALS(expected.relation(tagd::HARD_TAG_HAS,
				tagd::HARD_TAG_MESSAGE, literal), tagd::TAGD_OK);
			TS_ASSERT_DIFFERS(dump.str().find(">> " + expected.str()), std::string::npos);

			TS_ASSERT_EQUALS(ts.del(input, &ssn), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ts.get(stored, "subject"), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(stored.relations.size(), 1);
			for (const auto& p : stored.relations) {
				TS_ASSERT(p.modifier.empty());
			}
		}
	}

	void test_url_modifiers_preserve_referent_spellings() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"context", "dog"}) {
			TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		}
		TS_ASSERT_EQUALS(ts.put(tagd::referent("https", "dog", "context")), tagd::TAGD_OK);

		auto ssn = ts.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

		tagd::url page("https://example.com/path");
		TS_ASSERT_EQUALS(page.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "dog"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(page, &ssn), tagd::TAGD_OK);
		for (auto session : {&ssn, static_cast<tagd::tagspace_session*>(nullptr)}) {
			tagd::url loaded;
			TS_ASSERT_EQUALS(ts.get(loaded, page.id(), session), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(loaded.id(), page.id());
			TS_ASSERT_EQUALS(loaded.scheme(), "https");
			TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_SCHEME, "https"));
			TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "dog"));
		}
	}

	void test_subordinate_relator_change_preserves_rank() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("kind", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_SUB, tagd::POS_TAG)), tagd::TAGD_OK);
		for (const auto id : {"first", "second"}) {
			TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		}

		tagd::abstract_tag before, after;
		TS_ASSERT_EQUALS(ts.get(before, "first"), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("first", "kind",
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(after, "first"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.rank(), before.rank());
		TS_ASSERT_EQUALS(after.sub_relator(), "kind");
	}

	void test_resolve_referents_without_writing() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("context", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::referent("parent", tagd::HARD_TAG_ENTITY, "context")),
			tagd::TAGD_OK);

		auto ssn = ts.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);

		tagd::abstract_tag input("new", tagd::HARD_TAG_SUB, "parent", tagd::POS_TAG);
		tagd::abstract_tag resolved;
		TS_ASSERT_EQUALS(ts.resolve_referents(resolved, input, &ssn), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(resolved.super_object(), tagd::HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(input.super_object(), "parent");
		TS_ASSERT(!ts.exists("new"));

		TS_ASSERT_EQUALS(ts.resolve_referents(resolved, input, nullptr), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(resolved.super_object(), "parent");
	}

	void test_failed_predicate_deletion_without_session_rolls_back() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag subject("subject", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(subject.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_MESSAGE, "stored"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(subject), tagd::TAGD_OK);

		tagd::abstract_tag removal("subject");
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_MESSAGE, "missing"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(removal.relation(tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_MESSAGE, "stored"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.del(removal), tagd::TS_NOT_FOUND);

		tagd::abstract_tag unchanged;
		TS_ASSERT_EQUALS(ts.get(unchanged, "subject"), tagd::TAGD_OK);
		TS_ASSERT(unchanged.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "stored"));

		removal.relations = subject.relations;
		TS_ASSERT_EQUALS(ts.del(removal), tagd::TAGD_OK);
		tagd::abstract_tag loaded;
		TS_ASSERT_EQUALS(ts.get(loaded, "subject"), tagd::TAGD_OK);
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE));
	}

	void test_get_inherits_nearest_predicates_without_changing_storage() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag parent("parent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "root"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_LINE_NUMBER, "10"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_CAUSED_BY, tagd::HARD_TAG_MESSAGE, "cause"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(parent), tagd::TAGD_OK);

		tagd::abstract_tag child("child", tagd::HARD_TAG_SUB, "parent", tagd::POS_TAG);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "near"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(child), tagd::TAGD_OK);

		tagd::abstract_tag leaf("leaf", tagd::HARD_TAG_SUB, "child", tagd::POS_TAG);
		TS_ASSERT_EQUALS(leaf.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_LINE_NUMBER), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(leaf), tagd::TAGD_OK);

		tagd::abstract_tag direct, inherited;
		TS_ASSERT_EQUALS(ts.get(direct, "leaf"), tagd::TAGD_OK);
		std::ostringstream before, after;
		TS_ASSERT_EQUALS(ts.dump(before), tagd::TAGD_OK);

		const auto flags = tagd::F_INCLUDE_INHERITED_PREDICATES;
		TS_ASSERT_EQUALS(ts.get(inherited, "leaf", nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(inherited.id(), direct.id());
		TS_ASSERT_EQUALS(inherited.super_object(), direct.super_object());
		TS_ASSERT_EQUALS(inherited.sub_relator(), direct.sub_relator());
		TS_ASSERT_EQUALS(inherited.pos(), direct.pos());
		TS_ASSERT_EQUALS(inherited.rank(), direct.rank());
		TS_ASSERT_EQUALS(inherited.relations.size(), 3);
		TS_ASSERT(inherited.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "near"));
		TS_ASSERT(inherited.related(tagd::HARD_TAG_CAUSED_BY, tagd::HARD_TAG_MESSAGE, "cause"));
		for (const auto& p : inherited.relations) {
			if (p.relator == tagd::HARD_TAG_HAS && p.object == tagd::HARD_TAG_LINE_NUMBER)
				TS_ASSERT(p.modifier.empty());
		}

		TS_ASSERT_EQUALS(ts.get(direct, "leaf"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(direct.relations.size(), 1);
		TS_ASSERT_EQUALS(ts.dump(after), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(after.str(), before.str());

		// Driver flags also reach writes; deletion must act only on stored predicates.
		tagd::abstract_tag removal(leaf.id());
		removal.relations = leaf.relations;
		TS_ASSERT_EQUALS(ts.del(removal, nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(inherited, "leaf", nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT(inherited.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_LINE_NUMBER, "10"));
		TS_ASSERT_EQUALS(ts.get(direct, "leaf"), tagd::TAGD_OK);
		TS_ASSERT(direct.relations.empty());

		TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag("leaf", tagd::HARD_TAG_SUB,
			"parent", tagd::POS_TAG), nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(direct, "leaf"), tagd::TAGD_OK);
		TS_ASSERT(direct.relations.empty());
		TS_ASSERT_EQUALS(ts.get(inherited, "leaf", nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT(inherited.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "root"));

		// Full-text queries use GET internally but must keep ordinary query results.
		tagd::interrogator query(tagd::HARD_TAG_INTERROGATOR);
		TS_ASSERT_EQUALS(query.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_TERMS, "leaf"),
			tagd::TAGD_OK);
		tagd::tag_set results;
		TS_ASSERT_EQUALS(ts.query(results, query, nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(results.size(), 1);
		for (const auto& result : results) {
			TS_ASSERT_EQUALS(result.id(), "leaf");
			TS_ASSERT(result.relations.empty());
		}

		TS_ASSERT_EQUALS(ts.get(inherited, tagd::HARD_TAG_ENTITY, nullptr, flags), tagd::TAGD_OK);
		TS_ASSERT(inherited.relations.empty());
		TS_ASSERT_EQUALS(ts.get(inherited, "missing", nullptr, flags), tagd::TS_NOT_FOUND);
	}

	void test_inherited_predicates_resolve_before_referent_encoding() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		for (const auto id : {"context", "setting", "other_setting"}) {
			TS_ASSERT_EQUALS(ts.put(tagd::abstract_tag(id, tagd::HARD_TAG_SUB,
				tagd::HARD_TAG_ENTITY, tagd::POS_TAG)), tagd::TAGD_OK);
		}

		tagd::abstract_tag parent("parent", tagd::HARD_TAG_SUB, tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, "setting", "ancestor"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, "other_setting", "other"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(parent), tagd::TAGD_OK);

		tagd::abstract_tag child("child", tagd::HARD_TAG_SUB, "parent", tagd::POS_TAG);
		TS_ASSERT_EQUALS(child.relation(tagd::HARD_TAG_HAS, "setting", "local"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(child), tagd::TAGD_OK);

		TS_ASSERT_EQUALS(ts.put(tagd::referent("alias", "setting", "context")), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(tagd::referent("selected", "child", "context")), tagd::TAGD_OK);

		auto ssn = ts.get_session();
		TS_ASSERT_EQUALS(ssn.push_context("context"), tagd::TAGD_OK);
		tagd::abstract_tag loaded;
		const auto flags = tagd::F_INCLUDE_INHERITED_PREDICATES;
		TS_ASSERT_EQUALS(ts.get(loaded, "selected", &ssn, flags), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.id(), "selected");
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, "alias", "local"));
		TS_ASSERT(!loaded.related(tagd::HARD_TAG_HAS, "alias", "ancestor"));
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, "other_setting", "other"));

		TS_ASSERT_EQUALS(ts.get(loaded, "child", &ssn,
			flags | tagd::F_NO_TRANSFORM_REFERENTS), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.id(), "child");
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, "setting", "local"));
	}

	void test_url_get_inherits_predicates_without_changing_url_fields() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag parent("pages", tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		TS_ASSERT_EQUALS(parent.relation(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "page"),
			tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(parent), tagd::TAGD_OK);

		tagd::url page("https://example.com/path");
		// Preserve the URL's parsed fields while assigning its explicit parent.
		static_cast<tagd::abstract_tag&>(page) = tagd::abstract_tag(
			page.id(), tagd::HARD_TAG_SUB, parent.id(), tagd::POS_URL);
		TS_ASSERT_EQUALS(ts.put(page), tagd::TAGD_OK);

		tagd::url loaded;
		TS_ASSERT_EQUALS(ts.get(loaded, page.id(), nullptr,
			tagd::F_INCLUDE_INHERITED_PREDICATES), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.id(), page.id());
		TS_ASSERT_EQUALS(loaded.host(), "example.com");
		TS_ASSERT_EQUALS(loaded.path(), "/path");
		TS_ASSERT_EQUALS(loaded.super_object(), parent.id());
		TS_ASSERT(loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE, "page"));

		TS_ASSERT_EQUALS(ts.get(loaded, page.id()), tagd::TAGD_OK);
		TS_ASSERT(!loaded.related(tagd::HARD_TAG_HAS, tagd::HARD_TAG_MESSAGE));
	}

	static bool rank_equals_bytes(
		const tagd::rank& rank,
		std::initializer_list<unsigned char> bytes
	) {
		if (rank.size() != bytes.size())
			return false;

		const char* data = rank.c_str();
		size_t index = 0;
		for (unsigned char byte : bytes) {
			if (static_cast<unsigned char>(data[index]) != byte)
				return false;
			++index;
		}

		return true;
	}

	void test_memory_tagspace_round_trips_explicit_identity() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag type_of(
			"type_of",
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_SUB,
			tagd::POS_SUB_RELATOR
		);
		tagd::abstract_tag explicit_tag(
			"signal",
			"type_of",
			tagd::HARD_TAG_INTERROGATOR,
			tagd::POS_INTERROGATOR
		);
		tagd::abstract_tag loaded;

		TS_ASSERT_EQUALS(ts.put(type_of), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(explicit_tag), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(loaded, "signal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.id(), explicit_tag.id());
		TS_ASSERT_EQUALS(loaded.sub_relator(), explicit_tag.sub_relator());
		TS_ASSERT_EQUALS(loaded.super_object(), explicit_tag.super_object());
		TS_ASSERT_EQUALS(loaded.pos(), explicit_tag.pos());
		TS_ASSERT(!loaded.rank().empty());
	}

	void test_memory_tagspace_allows_children_of_root_entity() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag is_a(
			"is_a",
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_SUB,
			tagd::POS_SUB_RELATOR
		);
		tagd::abstract_tag
			animal("animal", "is_a", tagd::HARD_TAG_ENTITY, tagd::POS_TAG);
		tagd::abstract_tag loaded;

		TS_ASSERT_EQUALS(ts.put(is_a), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.put(animal), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(ts.get(loaded, "animal"), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(loaded.super_object(), tagd::HARD_TAG_ENTITY);
		// User tags must not reuse ranks already assigned to hard tags.
		TS_ASSERT(!loaded.rank().empty());
		TS_ASSERT_DIFFERS(loaded.rank(), ts.lookup_rank(tagd::HARD_TAG_SUB));
		TS_ASSERT(ts.contains(tagd::HARD_TAG_ENTITY, "animal"));
	}

	void test_memory_tagspace_rejects_hard_tag_mutation() {
		tagd::tagspace::memory ts;
		TS_ASSERT_EQUALS(ts.init(), tagd::TAGD_OK);
		tagd::abstract_tag hard_tag(
			tagd::HARD_TAG_HAS,
			tagd::HARD_TAG_SUB,
			tagd::HARD_TAG_RELATOR,
			tagd::POS_RELATOR
		);

		TS_ASSERT_EQUALS(ts.put(hard_tag), tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(ts.del(hard_tag), tagd::TS_MISUSE);
	}
};
