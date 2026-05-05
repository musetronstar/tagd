#pragma once

#include "tagd/hard-tags.h"
#include "tagd/tagspace.h"

#include <functional>
#include <map>
#include <ostream>

namespace tagd {

// hard_tagspace is the immutable hard-tag basis shared by every tagspace.
// Its surface is read-only because the hard-tag axioms are compile-time truth.
class hard_tagspace final : public const_tagspace {
public:
    hard_tagspace() = default;
    ~hard_tagspace() override = default;

    tagd::code put(const abstract_tag&, session* = nullptr, flags_t = 0) = delete;
    tagd::code put(const url&, session* = nullptr, flags_t = 0) = delete;
    tagd::code put(const referent&, session* = nullptr, flags_t = 0) = delete;

    tagd::code del(const abstract_tag&, session* = nullptr, flags_t = 0) = delete;
    tagd::code del(const url&, session* = nullptr, flags_t = 0) = delete;
    tagd::code del(const referent&, session* = nullptr, flags_t = 0) = delete;

    size_t merge(tag_set&, const tag_set&) = delete;

    [[nodiscard]] rank lookup_rank(id_view id) const override;
    [[nodiscard]] part_of_speech lookup_pos(id_view id) const override;
    [[nodiscard]] bool contains(id_view ancestor, id_view descendant) const override;

    [[nodiscard]]
    std::function<bool(const predicate&, const predicate&)>
    rank_comparator() const override;

    [[nodiscard]] tagd::code get(
        abstract_tag& t,
        id_view id,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] tagd::code query(
        tag_set& result,
        const interrogator& q,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] bool exists(id_view id, flags_t flags = 0) const override;
    [[nodiscard]] tagd::code dump(std::ostream& os = std::cout) const override;

    session* new_session() override;
    void release_session(session* ssn) override;
};

class tagspace::memory final : public tagspace {
public:
    memory() = default;
    ~memory() override = default;

    [[nodiscard]] rank lookup_rank(id_view id) const override;
    [[nodiscard]] part_of_speech lookup_pos(id_view id) const override;
    [[nodiscard]] bool contains(id_view ancestor, id_view descendant) const override;

    [[nodiscard]]
    std::function<bool(const predicate&, const predicate&)>
    rank_comparator() const override;

    [[nodiscard]] tagd::code get(
        abstract_tag& t,
        id_view id,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] tagd::code put(
        const abstract_tag& t,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] tagd::code del(
        const abstract_tag& t,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] tagd::code query(
        tag_set& result,
        const interrogator& q,
        session* ssn = nullptr,
        flags_t flags = 0
    ) override;

    [[nodiscard]] bool exists(id_view id, flags_t flags = 0) const override;
    size_t merge(tag_set& A, const tag_set& B) override;
    [[nodiscard]] tagd::code dump(std::ostream& os = std::cout) const override;

    session* new_session() override;
    void release_session(session* ssn) override;

private:
    struct stored_tag {
        id_string id;
        id_string sub_relator;
        id_string super_object;
        part_of_speech pos{POS_UNKNOWN};
        rank tag_rank;
        predicate_set relations;
    };

    [[nodiscard]] const stored_tag* find_user_tag(id_view id) const;
    [[nodiscard]] stored_tag* find_user_tag(id_view id);
    [[nodiscard]] tagd::code populate_tag(abstract_tag& t, const stored_tag& stored) const;
    [[nodiscard]] tagd::code store_user_tag(const abstract_tag& t);
    [[nodiscard]] rank allocate_child_rank(const rank& parent_rank) const;
    [[nodiscard]] bool predicate_matches(const predicate& expected, const predicate& actual) const;
    [[nodiscard]] bool tag_matches(const stored_tag& stored, const interrogator& q) const;

    // Composition keeps the immutable hard-tag basis visible without exposing
    // a mutable surface for ids whose rank/topology is axiomatic.
    hard_tagspace _hard;
    // std::less<> keeps lookup by id_view allocation-free on the hot path.
    std::map<id_string, stored_tag, std::less<>> _tags;
};

} // namespace tagd
