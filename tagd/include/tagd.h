#pragma once

#include <string>
#include <set>
#include <vector>
#include <iostream>

#include "tagd/codes.h"
#include "tagd/config.h"
#include "tagd/hard-tags.h"
#include "tagd/rank.h"

#include <cstdarg>  // for va_list

namespace tagd {

const size_t MAX_TAG_LEN = 2112;  // We are the Priests of the Temples of Syrinx 
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

// make predicate and insert relator, object, modifier
void insert_predicate(predicate_set&, const id_type&, const id_type&, const id_type&);

class abstract_tag;

typedef std::vector<tagd::id_type> id_vec;
/**************** tag_set defs **************/
typedef std::set<abstract_tag> tag_set;
typedef std::pair<tag_set::iterator, bool> tag_set_pair;

void print_tags(const tagd::tag_set&, std::ostream&os = std::cout);
void print_tag_ids(const tagd::tag_set&, std::ostream&os = std::cout);

std::string tag_ids_str(const tagd::tag_set&);

// Merges (in-place) tags into A from B
// Upon merging, tag relations from B will be merged into tag relations in A
// tag ids and ranks must be set for each element in A and B
void merge_tags(tag_set& A, const tag_set& B);

// Merges (in-place) tags into A from B that are present in both A and B
// and erases tags from A that are not present in A and B
// Upon merging, tag relations from B will be merged into tag relations in A
// Tag ids and ranks must be set for each element in A and B
// The number of tags merged will be returned
size_t merge_tags_erase_diffs(tag_set& A, const tag_set& B);

// Merges tags from B into A where an element of A contains B
// or an element of B contains A.
size_t merge_containing_tags(tag_set& A, const tag_set& B);

// every member in A deep equals (===) every member in B
bool tag_set_equal(const tag_set A, const tag_set B);
/************** end tag_set defs ************/

// TAGL part of speech
typedef enum {
    POS_UNKNOWN = 0,
    POS_TAG = 1,
    POS_SUPER = 2,
// relator is used as a "Copula" to link subjects to predicates
// (even more general than a "linking verb")
    POS_RELATOR = 3,
    POS_INTERROGATOR = 4,
    POS_URL = 5,
    // POS_URI // TODO
    POS_ERROR = 6,
    POS_REFERENT = 7,
    POS_REFERS = 8,
    POS_REFERS_TO = 9,
    POS_CONTEXT = 10
} part_of_speech;

// wrap in struct so we can define here
struct tag_util {
    static std::string pos_str(part_of_speech p) {
        switch (p) {
            case POS_UNKNOWN: return "POS_UNKNOWN";
            case POS_TAG:    return "POS_TAG";
            case POS_SUPER:    return "POS_SUPER";
            case POS_RELATOR:    return "POS_RELATOR";
            case POS_INTERROGATOR: return "POS_INTERROGATOR";
            case POS_URL:    return "POS_URL";
            case POS_REFERENT:    return "POS_REFERENT";
            case POS_REFERS:    return "POS_REFERS";
            case POS_REFERS_TO:    return "POS_REFERS_TO";
            case POS_CONTEXT:    return "POS_CONTEXT";
            default:          return "STR_EMPTY";
        }
    }
};

#define pos_str(c) tagd::tag_util::pos_str(c)

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
id_type super_relator(const part_of_speech&);
id_type super_relator(const abstract_tag&);

class abstract_tag {
    protected:
        id_type _id;
        id_type _super; // superordinate, parent, or hyponym in tree
        part_of_speech _pos;
        tagd::rank _rank;
		tagd_code _code;

        // set and return
        tagd_code code(tagd_code c) { _code = c; return _code; }
    public:
        // empty tag
        abstract_tag() : _id(), _super(), _pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};
        virtual ~abstract_tag() {};

        abstract_tag(const id_type& id) : _id(id), _super(), _pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};

        abstract_tag(const part_of_speech& p)
            : _id(), _super(), _pos(p), _rank(), _code(TAGD_OK) {};

        // id, but no super
        abstract_tag(const id_type& id, const part_of_speech& p)
            : _id(id), _super(), _pos(p), _rank(), _code(TAGD_OK) {};

        abstract_tag(const id_type& id, const id_type& super, const part_of_speech& p)
            : _id(id), _super(super), _pos(p), _rank(), _code(TAGD_OK)  {};

        abstract_tag(const id_type& id, const id_type& super,
                     const part_of_speech& p, const tagd::rank& rank)
            : _id(id), _super(super), _pos(p), _rank(rank), _code(TAGD_OK) {};

        predicate_set relations;

		void clear();
		bool empty() const {
			return (
				_id.empty()
				&& _super.empty()
				&& _rank.empty()
				&& relations.empty()
			);
		}

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

        tagd_code code() const { return _code; }
        bool ok() const { return _code == TAGD_OK; }

        tagd_code relation(const predicate&);
                            // relator, object
        tagd_code relation(const id_type&, const id_type&);
                            // relator, object, modifier
        tagd_code relation(const id_type&, const id_type&, const id_type&);

        tagd_code not_relation(const predicate&);
                            // relator, object
        tagd_code not_relation(const id_type&, const id_type&);
  
		// modifier not needed for negations (erasing)
        // tagd_code not_relation(const id_type&, const id_type&, const id_type&);

        tagd_code predicates(const predicate_set&);

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

        bool related(const predicate& p) const {
            return ( relations.find(p) != relations.end() );
        }

        // deep equality - every member is tested for equality
        bool operator==(const abstract_tag&) const;
        bool operator!=(const abstract_tag& rhs) const { return !(*this == rhs); }
        bool operator<(const abstract_tag&) const;

        friend std::ostream& operator<<(std::ostream&, const abstract_tag&);
};

class tag : public abstract_tag {
    public:
        tag() : abstract_tag(POS_TAG) {};

        tag(const id_type& id) : abstract_tag(id, POS_TAG) {};

        tag(const id_type& id, const id_type& super)
            : abstract_tag(id, super, POS_TAG) {};

    protected:
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
};

class referent : public abstract_tag {
		// _id is the thing that refers
		// _super is the thing refered to
    public:
	
        referent()
            : abstract_tag(POS_REFERENT)
		{}

        referent(const id_type& refers, const id_type& rt)
            : abstract_tag(refers, rt, POS_REFERENT)
		{}

        referent(const id_type& refers, const id_type& rt, const id_type& c)
            : abstract_tag(refers, rt, POS_REFERENT)
		{ context(c); }

        const id_type& refers() const { return _id; }
        const id_type& refers_to() const { return _super; }
        id_type context() const;
        void context(const id_type& c) {
			this->relation( HARD_TAG_CONTEXT, c);
		}

        bool operator==(const referent& rhs) const {
			return (
				_id == rhs._id &&
				_super == rhs._super &&
				_pos == rhs._pos &&
				this->context() == rhs.context()
			);
		}

		// override so we can have duplicate ids
        bool operator<(const referent& rhs) const {
			return (
				_id < rhs._id ||
				(
				 _id == rhs._id &&
				 _super < rhs._super
				) ||
				(
				 _id == rhs._id &&
				 _super == rhs._super &&
				 this->context() < rhs.context()
				)
			);
		}
};


// TODO maybe
/*
   measures the object of a predicate (i.e. answers "how many?")
   however, it could also represent a more general sense of quantification
   such as determiners (grammer) or variable binding (logic)
   http://en.wikipedia.org/wiki/Quantifier
*/
// class modifier : public tag {

class error : public abstract_tag {
	public:
        error() : abstract_tag(POS_ERROR)
		{}

        error(const tagd_code c, const char *msg = NULL)
            : abstract_tag(tagd_code_str(c), HARD_TAG_ERROR, POS_ERROR)
		{
			_code = c;
			if (msg != NULL) this->relation(HARD_TAG_HAS, "_msg", msg);
		}
};

typedef std::vector<tagd::error> errors_t;

class errorable {
	protected:
		tagd_code _init;
		tagd_code _code;
		errors_t _errors;

	public:
		errorable(tagd_code c) : _init(c), _code(c), _errors() {}
		virtual ~errorable() {}

		bool ok() const { return _code == tagd::TAGD_OK; }
		tagd_code code() const { return _code; }
		tagd::error& last_error() {
			if (_errors.size() == 0)
				_errors.push_back(tagd::error());
			return _errors[_errors.size()-1];
		}

		// set and return
        tagd_code code(tagd_code c) { if(_code != c) _code = c; return c; }

		// set and return code, set err msg to printf style formatted list
		tagd_code error(tagd::code c, const char *, ...);

		void clear_errors() { _code = _init; _errors.clear(); }

		void print_errors(std::ostream& os = std::cerr) const;
};

struct util {
	// returns c string of formatted printf string, or NULL on failure
	static char* csprintf(const char *, ...);
	static char* csprintf(const char *, va_list&);
};

}  // namespace tagd

#include "tagd/domain.h"
#include "tagd/url.h"

