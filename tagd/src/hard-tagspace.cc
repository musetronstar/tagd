#include "tagd/hard-tagspace.h"

#include <algorithm>
#include <string>
#include <vector>

namespace tagd {

namespace {

struct rank_key {
    int class_order;
    std::string bytes;
};

[[nodiscard]] bool hard_axiom_for_id(
    id_view id,
    const hard_tag_axiom*& axiom
) {
    axiom = hard_tag_axiom_for(id);
    return axiom != nullptr;
}

[[nodiscard]] rank rank_from_packed(uint64_t packed) {
    rank r;
    // The generated axioms store topology as packed uint64_t path bytes.
    (void)r.init(packed);
    return r;
}

[[nodiscard]] std::string packed_rank_bytes(const hard_tag_axiom& axiom) {
    std::string bytes;
    bytes.reserve(axiom.rank_size);

    for (uint8_t index = 0; index < axiom.rank_size; ++index) {
        bytes.push_back(static_cast<char>((axiom.packed_rank >> (56 - (index * 8))) & 0xff));
    }

    return bytes;
}

[[nodiscard]] std::string rank_bytes(const rank& r) {
    return std::string(r.c_str(), r.size());
}

[[nodiscard]] bool rank_bytes_have_prefix(
    const std::string& prefix,
    const std::string& full
) {
    if (prefix.empty() || prefix.size() > full.size()) {
        return false;
    }

    return full.compare(0, prefix.size(), prefix) == 0;
}

[[nodiscard]] rank_key classify_id_rank(
    id_view id,
    const std::function<rank(id_view)>& lookup_rank_fn
) {
    // Comparator order is: axiomatic root first, then ranked ids by topology,
    // then unresolved ids last so lexical fallback never outranks real rank.
    const hard_tag_axiom* axiom = nullptr;
    if (hard_axiom_for_id(id, axiom)) {
        if (axiom->id == HARD_TAG_ENTITY) {
            return {0, std::string()};
        }

        return {1, packed_rank_bytes(*axiom)};
    }

    const rank resolved = lookup_rank_fn(id);
    if (resolved.empty()) {
        return {2, std::string()};
    }

    return {1, rank_bytes(resolved)};
}

[[nodiscard]] int compare_ranked_ids(
    id_view lhs,
    id_view rhs,
    const std::function<rank(id_view)>& lookup_rank_fn
) {
    // This yields a total order that still respects rank topology when present.
    const rank_key lhs_key = classify_id_rank(lhs, lookup_rank_fn);
    const rank_key rhs_key = classify_id_rank(rhs, lookup_rank_fn);

    if (lhs_key.class_order != rhs_key.class_order) {
        return lhs_key.class_order < rhs_key.class_order ? -1 : 1;
    }

    if (lhs_key.class_order == 1 && lhs_key.bytes != rhs_key.bytes) {
        return lhs_key.bytes < rhs_key.bytes ? -1 : 1;
    }

    if (lhs != rhs) {
        return lhs < rhs ? -1 : 1;
    }

    return 0;
}

[[nodiscard]] bool hard_ancestor_contains_rank(
    const hard_tag_axiom& ancestor,
    const rank& descendant_rank
) {
    // HARD_TAG_ENTITY contains every ranked descendant even though its packed
    // form is the root sentinel rather than a normal non-empty prefix.
    if (ancestor.id == HARD_TAG_ENTITY) {
        return true;
    }

    return rank_bytes_have_prefix(packed_rank_bytes(ancestor), rank_bytes(descendant_rank));
}

[[nodiscard]] tagd::abstract_tag populated_tag(
    id_view id,
    id_view sub_relator,
    id_view super_object,
    part_of_speech pos,
    const rank& tag_rank,
    const predicate_set& relations = predicate_set{}
) {
    abstract_tag semantic(id, sub_relator, super_object, pos);
    semantic.relations = relations;

    return tag_rank.empty()
        ? semantic
        : abstract_tag(semantic, tag_rank);
}

} // namespace

rank hard_tagspace::lookup_rank(id_view id) const {
    const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
    return axiom == nullptr ? rank{} : rank_from_packed(axiom->packed_rank);
}

part_of_speech hard_tagspace::lookup_pos(id_view id) const {
    const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
    return axiom == nullptr ? POS_UNKNOWN : axiom->pos;
}

bool hard_tagspace::contains(id_view ancestor, id_view descendant) const {
    const hard_tag_axiom* ancestor_axiom = hard_tag_axiom_for(ancestor);
    const hard_tag_axiom* descendant_axiom = hard_tag_axiom_for(descendant);

    if (ancestor == descendant) {
        // Unknown ids must not self-contain: string equality is not membership.
        return ancestor_axiom != nullptr;
    }

    if (ancestor_axiom == nullptr || descendant_axiom == nullptr) {
        return false;
    }

    return rank_prefix_of(
        ancestor_axiom->packed_rank,
        ancestor_axiom->rank_size,
        descendant_axiom->packed_rank,
        descendant_axiom->rank_size
    );
}

std::function<bool(const predicate&, const predicate&)>
hard_tagspace::rank_comparator() const {
    return [this](const predicate& lhs, const predicate& rhs) {
        const auto lookup = [this](id_view id) { return this->lookup_rank(id); };
        const int relator_cmp = compare_ranked_ids(lhs.relator, rhs.relator, lookup);
        if (relator_cmp != 0) {
            return relator_cmp < 0;
        }

        const int object_cmp = compare_ranked_ids(lhs.object, rhs.object, lookup);
        if (object_cmp != 0) {
            return object_cmp < 0;
        }

        if (lhs.modifier != rhs.modifier) {
            return lhs.modifier < rhs.modifier;
        }

        if (lhs.opr8r != rhs.opr8r) {
            return lhs.opr8r < rhs.opr8r;
        }

        return lhs.modifier_type < rhs.modifier_type;
    };
}

code hard_tagspace::query(tag_set&, const interrogator&, session*, flags_t) {
    return TS_NOT_IMPLEMENTED;
}

bool hard_tagspace::exists(id_view id, flags_t) const {
    return hard_tag_axiom_for(id) != nullptr;
}

code hard_tagspace::dump(std::ostream&) const {
    return TS_NOT_IMPLEMENTED;
}

session* hard_tagspace::new_session() {
    return new session{};
}

void hard_tagspace::release_session(session* ssn) {
    delete ssn;
}

code hard_tagspace::get(abstract_tag& t, id_view id, session*, flags_t) {
    const hard_tag_axiom* axiom = hard_tag_axiom_for(id);
    if (axiom == nullptr) {
        t.clear();
        return TS_NOT_FOUND;
    }

    t = populated_tag(
        axiom->id,
        axiom->sub_relator,
        axiom->super_object,
        axiom->pos,
        rank_from_packed(axiom->packed_rank)
    );
    return TAGD_OK;
}

rank tagspace::memory::lookup_rank(id_view id) const {
    const rank hard_rank = _hard.lookup_rank(id);
    if (!hard_rank.empty() || _hard.lookup_pos(id) != POS_UNKNOWN) {
        return hard_rank;
    }

    const stored_tag* stored = this->find_user_tag(id);
    return stored == nullptr ? rank{} : stored->tag_rank;
}

part_of_speech tagspace::memory::lookup_pos(id_view id) const {
    const part_of_speech hard_pos = _hard.lookup_pos(id);
    if (hard_pos != POS_UNKNOWN) {
        return hard_pos;
    }

    const stored_tag* stored = this->find_user_tag(id);
    return stored == nullptr ? POS_UNKNOWN : stored->pos;
}

bool tagspace::memory::contains(id_view ancestor, id_view descendant) const {
    if (ancestor == descendant) {
        return this->exists(ancestor);
    }

    const hard_tag_axiom* ancestor_axiom = hard_tag_axiom_for(ancestor);
    const hard_tag_axiom* descendant_axiom = hard_tag_axiom_for(descendant);

    if (ancestor_axiom != nullptr && descendant_axiom != nullptr) {
        return rank_prefix_of(
            ancestor_axiom->packed_rank,
            ancestor_axiom->rank_size,
            descendant_axiom->packed_rank,
            descendant_axiom->rank_size
        );
    }

    const rank ancestor_rank = this->lookup_rank(ancestor);
    const rank descendant_rank = this->lookup_rank(descendant);

    if (ancestor_axiom != nullptr) {
        // Hard ancestors must still contain user tags when the user rank lives
        // under the same packed-rank prefix topology.
        return !descendant_rank.empty()
            && hard_ancestor_contains_rank(*ancestor_axiom, descendant_rank);
    }

    if (descendant_axiom != nullptr) {
        // User ancestors compare against hard descendants through the shared
        // rank encoding rather than by hard/user type distinction.
        return !ancestor_rank.empty()
            && ancestor_rank.contains(rank_from_packed(descendant_axiom->packed_rank));
    }

    if (descendant_rank.empty()) {
        return false;
    }

    return ancestor_rank.contains(descendant_rank);
}

std::function<bool(const predicate&, const predicate&)>
tagspace::memory::rank_comparator() const {
    return [this](const predicate& lhs, const predicate& rhs) {
        const auto lookup = [this](id_view id) { return this->lookup_rank(id); };
        const int relator_cmp = compare_ranked_ids(lhs.relator, rhs.relator, lookup);
        if (relator_cmp != 0) {
            return relator_cmp < 0;
        }

        const int object_cmp = compare_ranked_ids(lhs.object, rhs.object, lookup);
        if (object_cmp != 0) {
            return object_cmp < 0;
        }

        if (lhs.modifier != rhs.modifier) {
            return lhs.modifier < rhs.modifier;
        }

        if (lhs.opr8r != rhs.opr8r) {
            return lhs.opr8r < rhs.opr8r;
        }

        return lhs.modifier_type < rhs.modifier_type;
    };
}

code tagspace::memory::get(abstract_tag& t, id_view id, session*, flags_t) {
    if (_hard.exists(id)) {
        return _hard.get(t, id, nullptr, 0);
    }

    const stored_tag* stored = this->find_user_tag(id);
    if (stored == nullptr) {
        t.clear();
        return TS_NOT_FOUND;
    }

    return this->populate_tag(t, *stored);
}

code tagspace::memory::put(const abstract_tag& t, session* ssn, flags_t flags) {
    (void)flags;

    if (ssn != nullptr && !(flags & F_NO_RESET)) {
        ssn->code(TAGD_OK);
    }

    if (_hard.exists(t.id())) {
        if (ssn != nullptr) {
            ssn->code(TS_MISUSE);
        }
        return TS_MISUSE;
    }

    if (t.id().empty()) {
        if (ssn != nullptr) {
            ssn->code(TAG_ILLEGAL);
        }
        return TAG_ILLEGAL;
    }

    const tagd::code rc = this->store_user_tag(t);
    if (ssn != nullptr) {
        ssn->code(rc);
    }
    return rc;
}

code tagspace::memory::del(const abstract_tag& t, session* ssn, flags_t flags) {
    (void)flags;

    if (ssn != nullptr && !(flags & F_NO_RESET)) {
        ssn->code(TAGD_OK);
    }

    if (_hard.exists(t.id())) {
        if (ssn != nullptr) {
            ssn->code(TS_MISUSE);
        }
        return TS_MISUSE;
    }

    stored_tag* stored = this->find_user_tag(t.id());
    if (stored == nullptr) {
        if (ssn != nullptr) {
            ssn->code(TS_NOT_FOUND);
        }
        return TS_NOT_FOUND;
    }

    if (t.relations.empty()) {
        _tags.erase(stored->id);
    } else {
        for (const predicate& relation : t.relations) {
            stored->relations.erase(relation);
        }
    }

    if (ssn != nullptr) {
        ssn->code(TAGD_OK);
    }
    return TAGD_OK;
}

code tagspace::memory::query(tag_set& result, const interrogator& q, session*, flags_t) {
    result.clear();

    if (!q.id().empty() && q.id() != HARD_TAG_INTERROGATOR) {
        abstract_tag matched;
        const tagd::code rc = this->get(matched, q.id(), nullptr, 0);
        if (rc == TAGD_OK) {
            result.insert(matched);
        }
        return rc;
    }

    for (const auto& [id, stored] : _tags) {
        (void)id;
        if (!this->tag_matches(stored, q)) {
            continue;
        }

        abstract_tag matched;
        (void)this->populate_tag(matched, stored);
        result.insert(matched);
    }

    return TAGD_OK;
}

bool tagspace::memory::exists(id_view id, flags_t) const {
    return _hard.exists(id) || this->find_user_tag(id) != nullptr;
}

size_t tagspace::memory::merge(tag_set& A, const tag_set& B) {
    return merge_containing_tags(A, B);
}

code tagspace::memory::dump(std::ostream& os) const {
    for (const auto& [id, stored] : _tags) {
        os << id << ' ' << stored.super_object << '\n';
    }
    return TAGD_OK;
}

session* tagspace::memory::new_session() {
    return new session{};
}

void tagspace::memory::release_session(session* ssn) {
    delete ssn;
}

const tagspace::memory::stored_tag* tagspace::memory::find_user_tag(id_view id) const {
    const auto it = _tags.find(id);
    return it == _tags.end() ? nullptr : &it->second;
}

tagspace::memory::stored_tag* tagspace::memory::find_user_tag(id_view id) {
    const auto it = _tags.find(id);
    return it == _tags.end() ? nullptr : &it->second;
}

code tagspace::memory::populate_tag(abstract_tag& t, const stored_tag& stored) const {
    t = populated_tag(
        stored.id,
        stored.sub_relator,
        stored.super_object,
        stored.pos,
        stored.tag_rank,
        stored.relations
    );
    return TAGD_OK;
}

code tagspace::memory::store_user_tag(const abstract_tag& t) {
    const stored_tag* existing = this->find_user_tag(t.id());
    const bool has_relation_only_update =
        existing != nullptr
        && t.sub_relator().empty()
        && t.super_object().empty()
        && t.pos() == POS_UNKNOWN;

    if (has_relation_only_update) {
        // Relation-only updates extend an existing witness; they must not
        // rewrite the tag's structural identity or rank.
        if (t.relations.empty()) {
            return TS_MISUSE;
        }

        stored_tag next = *existing;
        next.relations.insert(t.relations.begin(), t.relations.end());
        _tags[next.id] = std::move(next);
        return TAGD_OK;
    }

    if (t.sub_relator().empty()) {
        return TS_SUB_UNK;
    }

    if (t.super_object().empty()) {
        return TS_OBJECT_UNK;
    }

    if (this->lookup_pos(t.sub_relator()) != POS_SUB_RELATOR) {
        return TS_RELATOR_UNK;
    }

    if (!this->exists(t.super_object())) {
        return TS_OBJECT_UNK;
    }

    const rank parent_rank = this->lookup_rank(t.super_object());
    const bool parent_is_root = t.super_object() == HARD_TAG_ENTITY;
    if (parent_rank.empty() && !parent_is_root) {
        return TS_OBJECT_UNK;
    }

    stored_tag next;
    next.id = t.id();
    next.sub_relator = t.sub_relator();
    next.super_object = t.super_object();
    next.pos = t.pos();
    next.relations = t.relations;

    if (existing != nullptr) {
        if (existing->super_object != t.super_object()) {
            // Constructive identity fixes a tag's parent once its rank exists.
            return TS_MISUSE;
        }

        next.tag_rank = existing->tag_rank;
        next.relations.insert(existing->relations.begin(), existing->relations.end());
    } else {
        next.tag_rank = this->allocate_child_rank(parent_rank);
        if (next.tag_rank.empty()) {
            return RANK_MAX_VALUE;
        }
    }

    _tags[next.id] = std::move(next);
    return TAGD_OK;
}

rank tagspace::memory::allocate_child_rank(const rank& parent_rank) const {
    const std::string parent = rank_bytes(parent_rank);
    unsigned next_component = 1;

    // A user tag rank is parent-prefix plus the next available child ordinal.
    for (const auto& [id, stored] : _tags) {
        (void)id;
        const std::string candidate = rank_bytes(stored.tag_rank);
        if (candidate.size() != parent.size() + 1) {
            continue;
        }
        if (candidate.compare(0, parent.size(), parent) != 0) {
            continue;
        }

        const unsigned component = static_cast<unsigned>(
            static_cast<unsigned char>(candidate.back())
        );
        next_component = std::max(next_component, component + 1);
    }

    if (next_component > 0xffu) {
        return rank{};
    }

    std::string next = parent;
    next.push_back(static_cast<char>(next_component));
    rank value;
    (void)value.init(next.c_str());
    return value;
}

bool tagspace::memory::predicate_matches(
    const predicate& expected,
    const predicate& actual
) const {
    return expected.relator == actual.relator
        && expected.object == actual.object
        && expected.modifier == actual.modifier
        && expected.opr8r == actual.opr8r
        && expected.modifier_type == actual.modifier_type;
}

bool tagspace::memory::tag_matches(const stored_tag& stored, const interrogator& q) const {
    if (!q.id().empty() && q.id() != HARD_TAG_INTERROGATOR && stored.id != q.id()) {
        return false;
    }

    for (const predicate& expected : q.relations) {
        // `_sub X` queries are structural interrogations over super_object, not
        // ordinary predicate-fiber matches stored in relations.
        const bool is_sub_query =
            expected.relator == HARD_TAG_SUB && expected.modifier.empty();
        if (is_sub_query && stored.super_object == expected.object) {
            continue;
        }

        const auto relation = std::find_if(
            stored.relations.begin(),
            stored.relations.end(),
            [this, &expected](const predicate& actual) {
                return this->predicate_matches(expected, actual);
            }
        );
        if (relation == stored.relations.end()) {
            return false;
        }
    }

    return true;
}

} // namespace tagd
