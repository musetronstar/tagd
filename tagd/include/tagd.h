#pragma once

#include <string>
#include <set>

#include "tagd/config.h"
#include "tagd/hard-tags.h"
#include "tagd/rank.h"

namespace tagd {

const size_t MAX_TAG_LEN = 128;
typedef std::string id_type;

struct predicate {
    id_type relator;
    id_type object;
    id_type modifier;

    // modifier not used in comparisions
    // relator and object make a predicate unique - modifier is incendental

    bool operator<(const predicate& p) const {
        return ( relator < p.relator 
                || ( relator == p.relator
                     && object < p.object )
        );
    }

    bool operator==(const predicate& p) const {
        return ( 
          relator == p.relator
          && object == p.object
        );
    }
};

typedef std::set<predicate> predicate_set;
typedef std::pair<predicate_set::iterator,bool> predicate_pair;

// relator, object, modifier
predicate make_predicate(const id_type&, const id_type&, const id_type&);
// relator, object
predicate make_predicate(const id_type&, const id_type&);

class abstract_tag;

/**************** tag_set defs **************/
typedef std::set<abstract_tag> tag_set;
typedef std::pair<tag_set::iterator, bool> tag_set_pair;

// Merges (in-place) tags into A from B
// Upon merging, tag relations from B will be merged into tag relations in A
// tag ids and ranks must be set for each element in A and B
void merge_tags(tag_set& A, const tag_set& B);

// Merges (in-place) tags into A from B that are present in both A and B
// and erases tags from A that are not present in B
// Upon merging, tag relations from B will be merged into tag relations in A
// Tag ids and ranks must be set for each element in A and B
// The number of tags merged will be returned
size_t merge_tags_erase_diffs(tag_set& A, const tag_set& B);

// every member in A deep equals (===) every member in B
bool tag_set_equal(const tag_set A, const tag_set B);
/************** end tag_set defs ************/

typedef enum {
    TAG_OK,
    TAG_INSERTED,
    TAG_UNKNOWN,
    TAG_DUPLICATE,
    TAG_ILLEGAL
} tag_code;

// TAGL part of speech
typedef enum {
    POS_UNKNOWN = 0,
    POS_TAG = 1,
// relator is used as a "Copula" to link subjects to predicates
// (even more general than a "linking verb")
    POS_RELATOR = 2,
    POS_INTERROGATOR = 3,
    POS_URL = 4
    // POS_URI = 5; // TODO
} part_of_speech;

/* hard tag that can be used in a super relation */
inline bool is_super_hard_tag(const id_type &id) {
    return (
        id == "_entity" ||
        id == HARD_TAG_RELATOR ||
        id == HARD_TAG_URL ||
        id == HARD_TAG_INTERROGATOR
    );
}

/* name of super relator (for semantic meaning) */
inline id_type super_relator(const part_of_speech &pos) {
    switch (pos) {
        case POS_TAG: return HARD_TAG_IS_A;
        default: return HARD_TAG_TYPE_OF;
    }
}



// wrap in struct so we can define here
struct tag_util {
    static std::string tag_code_str(tag_code c) {
        switch (c) {
            case TAG_OK:        return "TAG_OK";
            case TAG_INSERTED:  return "TAG_INSERTED";
            case TAG_UNKNOWN:   return "TAG_UNKNOWN";
            case TAG_DUPLICATE: return "TAG_DUPLICATE";
            case TAG_ILLEGAL:   return "TAG_ILLEGAL";
            default:            return "STR_EMPTY";
        }
    }

    static std::string pos_str(part_of_speech p) {
        switch (p) {
            case POS_UNKNOWN: return "POS_UNKNOWN";
            case POS_TAG:    return "POS_TAG";
            case POS_RELATOR:    return "POS_RELATOR";
            case POS_INTERROGATOR: return "POS_INTERROGATOR";
            case POS_URL:    return "POS_URL";
            default:          return "STR_EMPTY";
        }
    }
};

#define tag_code_str(c) tagd::tag_util::tag_code_str(c)
#define pos_str(c) tagd::tag_util::pos_str(c)

class abstract_tag {
    protected:
        id_type _id;
        id_type _super; // superordinate, parent, or hyponym in tree
        part_of_speech _pos;
        tagd::rank _rank;

    public:
        // empty tag
        // make noun default pos - better that than junk
        abstract_tag() : _id(), _super(), _pos(POS_UNKNOWN), _rank() {};
        virtual ~abstract_tag() {};

        abstract_tag(const id_type& id) : _id(id), _super(), _pos(POS_UNKNOWN), _rank() {};

        abstract_tag(const id_type& id, const tagd::rank& rank)
            : _id(id), _super(), _pos(POS_UNKNOWN), _rank(rank) {};

        abstract_tag(const part_of_speech& p)
            : _id(), _super(), _pos(p), _rank() {};

        // id, but no super
        abstract_tag(const id_type& id, const part_of_speech& p)
            : _id(id), _super(), _pos(p), _rank() {};

        abstract_tag(const id_type& id, const id_type& super, const part_of_speech& p)
            : _id(id), _super(super), _pos(p), _rank()  {};

        abstract_tag(const id_type& id, const id_type& super,
                     const part_of_speech& p, const tagd::rank& rank)
            : _id(id), _super(super), _pos(p), _rank(rank) {};

        predicate_set relations;

		void clear();
		bool empty();

        const id_type& id() const { return _id; }
        void id(const id_type& A) { _id = A; }

        const id_type& super() const { return _super; }
        void super(const id_type& s) { _super = s; }
        //void super(const char *s); // have to deal with NULL*

        const part_of_speech& pos() const { return _pos; }
        void pos(const part_of_speech& p) { _pos = p; }

        const tagd::rank& rank() const { return _rank; }
        void rank(const tagd::rank& r) { _rank = r; }

        // returns code because rank::init() will fail on invalid bytes
        rank_code rank(const byte_t *bytes) { return _rank.init(bytes); }

        tag_code relation(const predicate&);
                            // relator, object
        tag_code relation(const id_type&, const id_type&);
                            // relator, object, modifier
        tag_code relation(const id_type&, const id_type&, const id_type&);

        tag_code not_relation(const predicate&);
                            // relator, object
        tag_code not_relation(const id_type&, const id_type&);
  
		// modifier not needed for negations (erasing)
        // tag_code not_relation(const id_type&, const id_type&, const id_type&);

        tag_code predicates(const predicate_set&);

        // object, optional id if supplied will get set the relator of the relation (if related)
        bool related(const id_type&, id_type *how=NULL) const;

        // relator, object
        bool related(const id_type& relator, const id_type& object) const {
            return ( relations.find(make_predicate(relator, object)) != relations.end() );
        }

        // relator, object, modifier
        bool related(const id_type& r, const id_type& o, const id_type& m) const {
            return ( relations.find(make_predicate(r, o, m)) != relations.end() );
		}

        inline bool related(const predicate& p) const {
            return ( relations.find(p) != relations.end() );
        }

        // deep equality - every member is tested for equality
        bool operator==(const abstract_tag&) const;
        inline bool operator!=(const abstract_tag& rhs) const { return !(*this == rhs); }
        inline bool operator<(const abstract_tag& rhs) const { return this->_rank < rhs._rank; }

        friend std::ostream& operator<<(std::ostream&, const abstract_tag&);
};

class tag : public abstract_tag {
    public:
        tag() : abstract_tag(POS_TAG) {};

        tag(const id_type& id) : abstract_tag(id, POS_TAG) {};

        tag(const id_type& id, const id_type& super)
            : abstract_tag(id, super, POS_TAG) {};

        tag(const id_type& id, const id_type& super, const tagd::rank& rank)
            : abstract_tag(id, super, POS_TAG, rank) {};

        const id_type& is_a() const { return _super; }
        void is_a(const id_type& i) { _super = i; }

    protected:
        tag(const id_type& id, const id_type& super, const part_of_speech& p)
            : abstract_tag(id, super, p) {};
        tag(const id_type& id, const id_type& super,
            const part_of_speech& p, const tagd::rank& rank)
            : abstract_tag(id, super, p, rank) {};
};

// relates a subject to an object
// known as the linguistic "Copula" - usually a linking verb,
// but not necissarily so
// e.g. "dog is_a animal"; 'is_a' being the relator
class relator : public abstract_tag {
    public:
        relator(const id_type& id)
            : abstract_tag(id, HARD_TAG_RELATOR, POS_RELATOR) {};

        relator(const id_type& id, const id_type& super)
            : abstract_tag(id, super, POS_RELATOR) {};

        const id_type& type_of() const { return _super; }
        void type_of(const id_type& i) { _super = i; }
};

// 'wh.*|how' words (eg who, what, where, when, why, how)
// there are two types of interrogator:
// 1. one that identifies and defines a tag that is a type of interrogator
//    (ie. subordinate to "_interrogator"), used in get, put, etc. operations
//    ex. what _is_a _interrogator
// 2. or, one that holds an object of inquiry used in query operations
//    ex. what _is_a mammal has tail can bark
//    this type cannot be inserted into a tagspace
class interrogator : public abstract_tag {
    public:
        interrogator(const id_type& id)
            : abstract_tag(id, POS_INTERROGATOR) {};

        interrogator(const id_type& id, const id_type& super)
            : abstract_tag(id, super, POS_INTERROGATOR) {};

        const id_type& type_of() const { return _super; }
        void type_of(const id_type& i) { _super = i; }
};

// TODO maybe
/*
   measures the object of a predicate (i.e. answers "how many?")
   however, it could also represent a more general sense of quantification
   such as determiners (grammer) or variable binding (logic)
   http://en.wikipedia.org/wiki/Quantifier
*/
// class modifier : public tag {

}  // namespace tagd

#include "tagd/domain.h"
#include "tagd/url.h"

