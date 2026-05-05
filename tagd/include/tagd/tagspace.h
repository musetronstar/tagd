#pragma once

#include "tagd.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ostream>

namespace tagd {

// These flags preserve legacy call shapes; they alter execution details, not
// the topological meaning of ids, ranks, or containership.
typedef enum {
    F_NO_POS_CAST             = 1 << 0,
    F_NO_TRANSFORM_REFERENTS  = 1 << 1,
    F_NO_NOT_FOUND_ERROR      = 1 << 2,
    F_IGNORE_DUPLICATES       = 1 << 3,
    F_NO_RESET                = 1 << 4
} ts_flags;

const int TS_FLAGS_END = 1 << 5;
typedef uint32_t flags_t;

// const_tagspace is the read-only semantic surface: rank/pos/containership are
// the stable truths, while sessions/flags remain compatibility plumbing.
class const_tagspace : public tagd::errorable {
public:
    virtual ~const_tagspace() = default;

    [[nodiscard]] virtual tagd::rank lookup_rank(id_view id) const = 0;
    [[nodiscard]] virtual part_of_speech lookup_pos(id_view id) const = 0;
    [[nodiscard]] virtual bool contains(id_view ancestor, id_view descendant) const = 0;

    [[nodiscard]] virtual
    std::function<bool(const predicate&, const predicate&)>
    rank_comparator() const = 0;

    [[nodiscard]] virtual tagd::code get(
        abstract_tag& t,
        id_view id,
        session* ssn = nullptr,
        flags_t flags = 0
    ) = 0;

    [[nodiscard]] virtual tagd::code get(
        url& u,
        id_view id,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        return this->get(static_cast<abstract_tag&>(u), id, ssn, flags);
    }

    [[nodiscard]] virtual tagd::code query(
        tag_set& result,
        const interrogator& q,
        session* ssn = nullptr,
        flags_t flags = 0
    ) = 0;

    [[nodiscard]] virtual part_of_speech pos(
        id_view id,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        (void)ssn;
        (void)flags;
        return this->lookup_pos(id);
    }

    [[nodiscard]] virtual bool exists(id_view id, flags_t flags = 0) const = 0;

    [[nodiscard]] virtual tagd::code dump(std::ostream& os = std::cout) const = 0;
    [[nodiscard]] virtual tagd::code dump_grid(std::ostream& os = std::cout) const {
        (void)os;
        return tagd::TS_NOT_IMPLEMENTED;
    }
    [[nodiscard]] virtual tagd::code dump_terms(std::ostream& os = std::cout) const {
        (void)os;
        return tagd::TS_NOT_IMPLEMENTED;
    }
    [[nodiscard]] virtual tagd::code dump_search(std::ostream& os = std::cout) const {
        (void)os;
        return tagd::TS_NOT_IMPLEMENTED;
    }

    // Session lifetime is explicit because some implementations borrow storage
    // resources while others are pure in-memory or constexpr-backed surfaces.
    virtual session* new_session() = 0;
    virtual void release_session(session*) = 0;

protected:
    const_tagspace() : tagd::errorable(tagd::TS_INIT) {}

    const_tagspace(const const_tagspace&) = delete;
    const_tagspace& operator=(const const_tagspace&) = delete;
    const_tagspace(const_tagspace&&) = delete;
    const_tagspace& operator=(const_tagspace&&) = delete;
};

class tagspace : public const_tagspace {
public:
    class memory;

    ~tagspace() override = default;

    virtual size_t merge(tag_set& A, const tag_set& B) = 0;

    [[nodiscard]] virtual tagd::code put(
        const abstract_tag& t,
        session* ssn = nullptr,
        flags_t flags = 0
    ) = 0;

    [[nodiscard]] virtual tagd::code put(
        const url& u,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        return this->put(static_cast<const abstract_tag&>(u), ssn, flags);
    }

    [[nodiscard]] virtual tagd::code put(
        const referent& r,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        return this->put(static_cast<const abstract_tag&>(r), ssn, flags);
    }

    [[nodiscard]] virtual tagd::code del(
        const abstract_tag& t,
        session* ssn = nullptr,
        flags_t flags = 0
    ) = 0;

    [[nodiscard]] virtual tagd::code del(
        const url& u,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        return this->del(static_cast<const abstract_tag&>(u), ssn, flags);
    }

    [[nodiscard]] virtual tagd::code del(
        const referent& r,
        session* ssn = nullptr,
        flags_t flags = 0
    ) {
        return this->del(static_cast<const abstract_tag&>(r), ssn, flags);
    }

protected:
    tagspace() = default;
    tagspace(const tagspace&) = delete;
    tagspace& operator=(const tagspace&) = delete;
    tagspace(tagspace&&) = delete;
    tagspace& operator=(tagspace&&) = delete;
};

} // namespace tagd
