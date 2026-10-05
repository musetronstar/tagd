#pragma once

#include "tagd/hard-tagspace.h"
#include "tagd/hard-tags.h"
#include "tagd/tagspace.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <type_traits>
#include <vector>

#include <cxxtest/TestSuite.h>

static_assert(tagd::hard_tag_axiom_for(tagd::HARD_TAG_ENTITY) != nullptr);
static_assert(tagd::hard_tag_axiom_for(tagd::HARD_TAG_ENTITY)->packed_rank == 0x0000000000000000ULL);
static_assert(tagd::hard_tag_axiom_for(tagd::HARD_TAG_ENTITY)->rank_size == 1);

class HardTagTester : public CxxTest::TestSuite {
public:
	void test_boolean_axioms_and_parts_of_speech() {
		tagd::hard_tagspace ts;
		for (const auto id : {tagd::HARD_TAG_TRUE, tagd::HARD_TAG_FALSE}) {
			const auto expected = id == tagd::HARD_TAG_TRUE ? tagd::POS_TRUE : tagd::POS_FALSE;
			const auto* axiom = tagd::hard_tag_axiom_for(id);
			TS_ASSERT(axiom != nullptr);
			if (axiom == nullptr) continue;
			TS_ASSERT_EQUALS(axiom->pos, expected);
			TS_ASSERT_EQUALS(ts.lookup_pos(id), expected);
			TS_ASSERT(ts.contains(tagd::HARD_TAG_ENTITY, id));
			TS_ASSERT(!ts.contains(tagd::HARD_TAG_TRUE, tagd::HARD_TAG_FALSE));
			TS_ASSERT(!ts.contains(tagd::HARD_TAG_FALSE, tagd::HARD_TAG_TRUE));
			TS_ASSERT_EQUALS(tagd::pos_str(expected),
				id == tagd::HARD_TAG_TRUE ? "POS_TRUE" : "POS_FALSE");
		}
	}

	template <typename T>
	static constexpr bool has_put_abstract_tag_v = requires(T& ts, const tagd::abstract_tag& t) {
		ts.put(t);
	};

	template <typename T>
	static constexpr bool has_del_abstract_tag_v = requires(T& ts, const tagd::abstract_tag& t) {
		ts.del(t);
	};

	template <typename T>
	static constexpr bool has_merge_v = requires(T& ts, tagd::tag_set& a, const tagd::tag_set& b) {
		ts.merge(a, b);
	};

	static bool rank_equals_bytes(const tagd::rank& rank, std::initializer_list<unsigned char> bytes) {
		if (rank.size() != bytes.size())
			return false;

		const char *data = rank.c_str();
		size_t index = 0;
		for (unsigned char byte : bytes) {
			if (static_cast<unsigned char>(data[index]) != byte)
				return false;
			++index;
		}

		return true;
	}

	class session_owner {
	public:
		explicit session_owner(tagd::const_tagspace& ts) :
			_tagspace(ts),
			_session(ts.new_session())
		{}

		~session_owner() {
			if (_session != nullptr)
				_tagspace.release_session(_session);
		}

		tagd::tagspace_session* get() const { return _session; }

	private:
		tagd::const_tagspace& _tagspace;
		tagd::tagspace_session* _session;
	};

	class stub_const_tagspace final : public tagd::const_tagspace {
	public:
		mutable tagd::id_string last_lookup_pos_id;
		mutable unsigned lookup_pos_calls{0};
		tagd::abstract_tag* last_get_tag{nullptr};

		[[nodiscard]] tagd::rank lookup_rank(tagd::id_view) const override {
			return tagd::rank{};
		}

		[[nodiscard]] tagd::part_of_speech lookup_pos(tagd::id_view id) const override {
			last_lookup_pos_id = tagd::id_string{id};
			++lookup_pos_calls;
			return id == "dog" ? tagd::POS_TAG : tagd::POS_UNKNOWN;
		}

		[[nodiscard]] bool contains(tagd::id_view, tagd::id_view) const override {
			return false;
		}

		[[nodiscard]]
		std::function<bool(const tagd::predicate&, const tagd::predicate&)>
		rank_comparator() const override {
			return [](const tagd::predicate&, const tagd::predicate&) {
				return false;
			};
		}

		[[nodiscard]] tagd::code get(
			tagd::abstract_tag& t,
			tagd::id_view,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			last_get_tag = &t;
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] tagd::code query(
			tagd::tag_set&,
			const tagd::interrogator&,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] bool exists(tagd::id_view, tagd::flags_t = 0) const override {
			return false;
		}

		tagd::tagspace_session* new_session() override {
			return new tagd::tagspace_session([this](tagd::id_view id) { return this->exists(id); });
		}

		void release_session(tagd::tagspace_session* ssn) override {
			delete ssn;
		}

		[[nodiscard]] tagd::code dump(std::ostream& = std::cout) const override {
			return tagd::TS_NOT_IMPLEMENTED;
		}
	};

	class stub_tagspace final : public tagd::tagspace {
	public:
		const tagd::abstract_tag* last_put_tag{nullptr};
		const tagd::abstract_tag* last_del_tag{nullptr};
		tagd::abstract_tag* last_get_tag{nullptr};

		[[nodiscard]] tagd::rank lookup_rank(tagd::id_view) const override {
			return tagd::rank{};
		}

		[[nodiscard]] tagd::part_of_speech lookup_pos(tagd::id_view) const override {
			return tagd::POS_UNKNOWN;
		}

		[[nodiscard]] bool contains(tagd::id_view, tagd::id_view) const override {
			return false;
		}

		[[nodiscard]]
		std::function<bool(const tagd::predicate&, const tagd::predicate&)>
		rank_comparator() const override {
			return [](const tagd::predicate&, const tagd::predicate&) {
				return false;
			};
		}

		[[nodiscard]] tagd::code get(
			tagd::abstract_tag& t,
			tagd::id_view,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			last_get_tag = &t;
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] tagd::code put(
			const tagd::abstract_tag& t,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			last_put_tag = &t;
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] tagd::code del(
			const tagd::abstract_tag& t,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			last_del_tag = &t;
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] tagd::code query(
			tagd::tag_set&,
			const tagd::interrogator&,
			tagd::tagspace_session* = nullptr,
			tagd::flags_t = 0
		) override {
			return tagd::TS_NOT_IMPLEMENTED;
		}

		[[nodiscard]] bool exists(tagd::id_view, tagd::flags_t = 0) const override {
			return false;
		}

		size_t merge(tagd::tag_set&, const tagd::tag_set&) override {
			return 0;
		}

		tagd::tagspace_session* new_session() override {
			return new tagd::tagspace_session([this](tagd::id_view id) { return this->exists(id); });
		}

		void release_session(tagd::tagspace_session* ssn) override {
			delete ssn;
		}

		[[nodiscard]] tagd::code dump(std::ostream& = std::cout) const override {
			return tagd::TS_NOT_IMPLEMENTED;
		}
	};

	void test_const_tagspace_contract_compiles() {
		stub_const_tagspace ts;
		tagd::abstract_tag tag;
		tagd::tag_set result;
		tagd::interrogator query;
		tagd::flags_t flags = tagd::F_NO_RESET;

		TS_ASSERT(ts.lookup_rank("dog").empty());
		TS_ASSERT_EQUALS(ts.lookup_pos("dog"), tagd::POS_TAG);
		TS_ASSERT(!ts.contains("animal", "dog"));
		TS_ASSERT_EQUALS(ts.get(tag, "dog", nullptr, flags), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT_EQUALS(ts.query(result, query, nullptr, flags), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT(!ts.exists("dog", flags));
	}

	void test_tagspace_contract_compiles() {
		stub_tagspace ts;
		tagd::abstract_tag tag;
		tagd::tag_set result;
		tagd::flags_t flags = tagd::F_NO_RESET;

		TS_ASSERT_EQUALS(ts.put(tag, nullptr, flags), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT_EQUALS(ts.del(tag, nullptr, flags), tagd::TS_NOT_IMPLEMENTED);
		TS_ASSERT_EQUALS(ts.merge(result, result), 0u);
	}

	void test_legacy_pos_delegates_to_lookup_pos() {
		stub_const_tagspace ts;
		session_owner ssn(ts);

		TS_ASSERT_EQUALS(ts.pos("dog", ssn.get(), tagd::F_NO_RESET), tagd::POS_TAG);
		TS_ASSERT_EQUALS(ts.lookup_pos_calls, 1u);
		TS_ASSERT_EQUALS(ts.last_lookup_pos_id, "dog");
	}

	void test_hard_tag_axiom_lookup_exposes_core_metadata() {
		const tagd::hard_tag_axiom* entity = tagd::hard_tag_axiom_for(tagd::HARD_TAG_ENTITY);
		const tagd::hard_tag_axiom* sub = tagd::hard_tag_axiom_for(tagd::HARD_TAG_SUB);
		const tagd::hard_tag_axiom* has = tagd::hard_tag_axiom_for(tagd::HARD_TAG_HAS);

		TS_ASSERT(entity != nullptr);
		TS_ASSERT(sub != nullptr);
		TS_ASSERT(has != nullptr);

		TS_ASSERT_EQUALS(entity->sub_relator, tagd::HARD_TAG_SUB);
		TS_ASSERT_EQUALS(entity->super_object, tagd::HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(entity->pos, tagd::POS_TAG);
		TS_ASSERT_EQUALS(entity->packed_rank, 0x0000000000000000ULL);
		TS_ASSERT_EQUALS(entity->rank_size, 1u);

		TS_ASSERT_EQUALS(sub->super_object, tagd::HARD_TAG_ENTITY);
		TS_ASSERT_EQUALS(sub->pos, tagd::POS_SUB_RELATOR);

		TS_ASSERT_EQUALS(has->super_object, tagd::HARD_TAG_RELATOR);
		TS_ASSERT_EQUALS(has->pos, tagd::POS_RELATOR);
	}

	void test_hard_tag_id_index_is_sorted_and_consistent() {
		TS_ASSERT(tagd::HARD_TAG_AXIOM_COUNT > 50u);

		for (size_t i = 0; i < tagd::HARD_TAG_ID_INDEX.size(); ++i) {
			TS_ASSERT(tagd::HARD_TAG_ID_INDEX[i].axiom_index < tagd::HARD_TAG_AXIOM_COUNT);
			if (i != 0)
				TS_ASSERT(tagd::HARD_TAG_ID_INDEX[i - 1].id < tagd::HARD_TAG_ID_INDEX[i].id);

			TS_ASSERT_EQUALS(
				tagd::HARD_TAG_AXIOMS[tagd::HARD_TAG_ID_INDEX[i].axiom_index].id,
				tagd::HARD_TAG_ID_INDEX[i].id
			);
		}
	}

	void test_hard_tagspace_lookup_and_contains_follow_axioms() {
		tagd::hard_tagspace ts;
		tagd::abstract_tag t;

		TS_ASSERT(ts.exists(tagd::HARD_TAG_ENTITY));
		TS_ASSERT_EQUALS(ts.lookup_pos(tagd::HARD_TAG_ENTITY), tagd::POS_TAG);
		TS_ASSERT(ts.contains(tagd::HARD_TAG_ENTITY, tagd::HARD_TAG_ENTITY));
		TS_ASSERT(ts.contains(tagd::HARD_TAG_ENTITY, tagd::HARD_TAG_HAS));
		const auto has_rank = ts.lookup_rank(tagd::HARD_TAG_HAS);
		const auto relator_rank = ts.lookup_rank(tagd::HARD_TAG_RELATOR);
		const auto can_rank = ts.lookup_rank(tagd::HARD_TAG_CAN);
		// Test hierarchy independently of declaration-order rank numbering.
		TS_ASSERT(!has_rank.empty());
		TS_ASSERT_DIFFERS(has_rank, relator_rank);
		TS_ASSERT(relator_rank.contains(has_rank));
		TS_ASSERT(!has_rank.contains(relator_rank));
		TS_ASSERT_DIFFERS(has_rank, can_rank);
		TS_ASSERT(!has_rank.contains(can_rank));
		TS_ASSERT(!can_rank.contains(has_rank));
		TS_ASSERT_EQUALS(ts.get(t, tagd::HARD_TAG_HAS), tagd::TAGD_OK);
		TS_ASSERT_EQUALS(t.id(), tagd::HARD_TAG_HAS);
		TS_ASSERT_EQUALS(t.sub_relator(), tagd::HARD_TAG_SUB);
		TS_ASSERT_EQUALS(t.super_object(), tagd::HARD_TAG_RELATOR);
		TS_ASSERT_EQUALS(t.pos(), tagd::POS_RELATOR);
	}

	void test_rank_comparator_orders_hard_tags_before_unranked_values() {
		tagd::hard_tagspace ts;
		std::vector<tagd::predicate> predicates{
			tagd::predicate("z_rel", "literal"),
			tagd::predicate(tagd::HARD_TAG_HAS, "literal"),
			tagd::predicate(tagd::HARD_TAG_CAN, "literal")
		};

		std::ranges::sort(predicates, ts.rank_comparator());

		TS_ASSERT_EQUALS(predicates[0].relator, tagd::HARD_TAG_HAS);
		TS_ASSERT_EQUALS(predicates[1].relator, tagd::HARD_TAG_CAN);
		TS_ASSERT_EQUALS(predicates[2].relator, "z_rel");
	}

};
