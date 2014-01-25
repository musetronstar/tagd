#pragma once

#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdarg>

#include "tagd/codes.h"
#include "tagd/config.h"
#include "tagd/hard-tags.h"
#include "tagd/rank.h"

#include <cstdarg>  // for va_list

namespace tagd {

const size_t MAX_TAG_LEN = 2112;  // We are the Priests of the Temples of Syrinx 
typedef std::string id_type;
typedef long long int rowid_t;
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
          && modifier == p.modifier
        );
    }

	bool operator!=(const predicate& p) const {
		return (!(*this == p));
	}

	bool empty() const {
		return (
			relator.empty()
			&& object.empty()
			&& modifier.empty()
		);
	}
};

typedef std::set<predicate> predicate_set;
typedef std::pair<predicate_set::iterator,bool> predicate_pair;

// relator, object, modifier
inline predicate make_predicate(const id_type& r, const id_type& o, const id_type& q) {
    return predicate{ r, o, q };
}

// relator, object
inline predicate make_predicate(const id_type& r, const id_type& o) {
    return predicate{ r, o, "" };
}

// make predicate and insert relator, object, modifier
void insert_predicate(predicate_set&, const id_type&, const id_type&, const id_type&);

class abstract_tag;

typedef std::vector<tagd::id_type> id_vec;
/**************** tag_set defs **************/
typedef std::set<abstract_tag> tag_set;

void print_tags(const tagd::tag_set&, std::ostream&os = std::cout);
void print_tag_ids(const tagd::tag_set&, std::ostream&os = std::cout);

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
    POS_UNKNOWN       = 0,
    POS_TAG           = 1 << 0,
    POS_SUPER_RELATOR = 1 << 1,
    POS_SUPER_OBJECT  = 1 << 2,
// relator is used as a "Copula" to link subjects to predicates
// (even more general than a "linking verb")
    POS_RELATOR       = 1 << 3,
    POS_INTERROGATOR  = 1 << 4,
    POS_URL           = 1 << 5,
    // POS_URI // TODO
    POS_ERROR         = 1 << 6,
	POS_SUBJECT       = 1 << 7,
	// a relator at use in a relation
	POS_RELATED       = 1 << 8,
	POS_OBJECT        = 1 << 9,
	POS_MODIFIER      = 1 << 10,
    POS_REFERENT      = 1 << 11,
    POS_REFERS        = 1 << 12,
    POS_REFERS_TO     = 1 << 13,
    POS_CONTEXT       = 1 << 14,
    POS_FLAG          = 1 << 15
} part_of_speech;
const int POS_END     = 1 << 16;

// wrap in struct so we can define here
struct tag_util {
    static std::string pos_str(part_of_speech p) {
        switch (p) {
            case POS_UNKNOWN: return "POS_UNKNOWN";
            case POS_TAG:    return "POS_TAG";
            case POS_SUPER_RELATOR:    return "POS_SUPER_RELATOR";
            case POS_SUPER_OBJECT:    return "POS_SUPER_OBJECT";
            case POS_RELATOR:    return "POS_RELATOR";
            case POS_INTERROGATOR: return "POS_INTERROGATOR";
            case POS_URL:    return "POS_URL";
            case POS_ERROR:    return "POS_ERROR";
            case POS_SUBJECT:    return "POS_SUBJECT";
            case POS_RELATED:    return "POS_RELATED";
            case POS_OBJECT:    return "POS_OBJECT";
            case POS_MODIFIER:    return "POS_MODIFIER";
            case POS_REFERENT:    return "POS_REFERENT";
            case POS_REFERS:    return "POS_REFERS";
            case POS_REFERS_TO:    return "POS_REFERS_TO";
            case POS_CONTEXT:    return "POS_CONTEXT";
            default:          return "POS_STR_DEFAULT";
        }
    }

	static std::string pos_list_str(part_of_speech p) {
		if (p == POS_UNKNOWN)
			return "POS_UNKNOWN";

		std::string s;
		int i = 0;
		part_of_speech pos = (part_of_speech)(1 << i);
		if ((p & pos) == pos)
			s.append( pos_str(pos) );
		while ((pos=(part_of_speech)(1<<(++i))) < POS_END) {
			if ((p & pos) == pos) {
				if (s.size() > 0)
					s.append(",");
				s.append(pos_str(pos));
			}
		}

		return s;
	}
};

#define pos_str(c) tagd::tag_util::pos_str(c)
#define pos_list_str(c) tagd::tag_util::pos_list_str(c)

class abstract_tag {
    protected:
        id_type _id;
        id_type _super_relator; // superordinate relation (i.e. _is_a)
        id_type _super_object;  // superordinate, parent, or hyponym in tree
        part_of_speech _pos;
        tagd::rank _rank;
		tagd_code _code;

        // set and return
        tagd_code code(tagd_code c) { _code = c; return _code; }
    public:
        // empty tag
        abstract_tag() :
			_id(), _super_relator(HARD_TAG_SUPER), _super_object(),
			_pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};
        virtual ~abstract_tag() {};

        abstract_tag(const id_type& id) :
			_id(id), _super_relator(HARD_TAG_SUPER), _super_object(),
			_pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};

        abstract_tag(const part_of_speech& p) :
			_id(), _super_relator(HARD_TAG_SUPER), _super_object(),
			_pos(p), _rank(), _code(TAGD_OK) {};

        // id, but no super
        abstract_tag(const id_type& id, const part_of_speech& p) :
			_id(id), _super_relator(HARD_TAG_SUPER), _super_object(),
			_pos(p), _rank(), _code(TAGD_OK) {};

        abstract_tag(const id_type& id, const id_type& super_obj, const part_of_speech& p) :
			_id(id), _super_relator(HARD_TAG_SUPER), _super_object(super_obj),
			_pos(p), _rank(), _code(TAGD_OK) {};

        abstract_tag(
				const id_type& id,
				const id_type& rel,
				const id_type& obj,
				const part_of_speech& p
				) :
			_id(id), _super_relator(rel), _super_object(obj),
			_pos(p), _rank(), _code(TAGD_OK) {};

        predicate_set relations;

		void clear();
		bool empty() const {
			return (
				_id.empty()
				//&& _super_relator.empty()  // super relators are set by constructor
				&& _super_object.empty()
				&& _rank.empty()
				&& relations.empty()
			);
		}

        const id_type& id() const { return _id; }
        void id(const id_type& A) { _id = A; }

        const id_type& super_relator() const { return _super_relator; }
        void super_relator(const id_type& s) { _super_relator = s; }

        const id_type& super_object() const { return _super_object; }
        void super_object(const id_type& s) { _super_object = s; }

        const part_of_speech& pos() const { return _pos; }
        void pos(const part_of_speech& p) { _pos = p; }

        const tagd::rank& rank() const { return _rank; }
        void rank(const tagd::rank& r) { _rank = r; }

        // returns code because rank::init() will fail on invalid bytes
        tagd_code rank(const char *bytes) { return _rank.init(bytes); }

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

        void predicates(const predicate_set&);

        bool has_relator(const id_type&) const;
        bool has_relator(const id_type&, predicate_set& how) const;
		// related to object
        bool related(const id_type& object) const;
        // fill predicate set with predicates matching object, return num matches
        size_t related(const id_type& object, predicate_set& how) const;

        // relator, object
        bool related(const id_type& relator, const id_type& object) const {
            return ( relations.find(make_predicate(relator, object)) != relations.end() );
        }

        bool related(const predicate& p) const {
			auto it = relations.find(p);
            if (it == relations.end())
				return false;

			if (!p.modifier.empty())
				return (p.modifier == it->modifier);
			else
				return true;
        }

        // relator, object, modifier
        bool related(const id_type& r, const id_type& o, const id_type& m) const {
            return this->related(make_predicate(r, o, m));
		}

        // deep equality - every member is tested for equality
        bool operator==(const abstract_tag&) const;
        bool operator!=(const abstract_tag& rhs) const { return !(*this == rhs); }
        bool operator<(const abstract_tag&) const;

		std::string str() const {
			std::stringstream ss;
			ss << *this;
			return ss.str();
		}

        friend std::ostream& operator<<(std::ostream&, const abstract_tag&);
};

template <class T>
std::string tag_ids_str(const T& t) {
	if (t.size() == 0) return std::string();

	std::stringstream ss;
	auto it = t.begin();
	ss << it->id();
	++it;
	for (; it != t.end(); ++it) {
		ss << ", " << it->id();
	}

	return ss.str();
}

class tag : public abstract_tag {
    public:
        tag() : abstract_tag(POS_TAG) {};

        tag(const id_type& id) : abstract_tag(id, POS_TAG)
		{
			_super_relator = HARD_TAG_IS_A;
		};

        tag(const id_type& id, const id_type& super_obj) :
			abstract_tag(id, HARD_TAG_IS_A, super_obj, POS_TAG) {};

        tag(const id_type& id, const id_type& super_rel, const id_type& super_obj) :
			abstract_tag(id, super_rel, super_obj, POS_TAG) {};
};

// relates a subject to an object
// known as the linguistic "Copula" - usually a linking verb,
// but not necissarily so
// e.g. "dog is_a animal"; 'is_a' being the relator
class relator : public abstract_tag {
    public:
        relator(const id_type& id) :
			abstract_tag(id, HARD_TAG_TYPE_OF, HARD_TAG_RELATOR, POS_RELATOR) {};

        relator(const id_type& id, const id_type& super_obj) :
			abstract_tag(id, HARD_TAG_TYPE_OF, super_obj, POS_RELATOR) {};

        relator(const id_type& id, const id_type& super_rel, const id_type& super_obj) :
			abstract_tag(id, super_rel, super_obj, POS_RELATOR) {};
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
        interrogator(const id_type& id) :
			abstract_tag(id, POS_INTERROGATOR) {};

        interrogator(const id_type& id, const id_type& super_obj) :
			abstract_tag(id, HARD_TAG_TYPE_OF, super_obj, POS_INTERROGATOR) {};

        interrogator(const id_type& id, const id_type& super_rel, const id_type& super_obj) :
			abstract_tag(id, super_rel, super_obj, POS_INTERROGATOR) {};
};

class referent : public abstract_tag {
		// _id is the thing that refers
		// _super_object is the thing refered to
    public:
	
        referent() :
			abstract_tag(POS_REFERENT)
		{
			_super_relator = HARD_TAG_REFERS_TO;
		};

        referent(const abstract_tag& t) :
			abstract_tag(t.id(), t.super_relator(), t.super_object(), POS_REFERENT)
		{ relations = t.relations; }

        referent(const id_type& refers, const id_type& refers_to) :
			abstract_tag(refers, HARD_TAG_REFERS_TO, refers_to, POS_REFERENT)
		{}

        referent(const id_type& refers, const id_type& refers_to, const id_type& c) :
			abstract_tag(refers, HARD_TAG_REFERS_TO, refers_to, POS_REFERENT)
		{ context(c); }

		// TODO don't allow overridding HARD_TAG_REFERS_TO
		// the problem, though, is that the getter method also gets deleted
        // void super_relator(const id_type& s) = delete;

        const id_type& refers() const { return _id; }
        const id_type& refers_to() const { return _super_object; }
        id_type context() const;
        void context(const id_type& c) {
			this->relation( HARD_TAG_CONTEXT, c);
		}

        bool operator==(const referent& rhs) const {
			return (
				_id == rhs._id &&
				// leave _super_relator out of equality and less comparisons
				// because _id, _super_object, and _context make it unique
				//_super_relator == rhs._super_relator &&
				_super_object == rhs._super_object &&
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
				 _super_object < rhs._super_object
				) ||
				(
				 _id == rhs._id &&
				 _super_object == rhs._super_object &&
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

        error(const tagd_code c) :
			abstract_tag(tagd_code_str(c), HARD_TAG_TYPE_OF, HARD_TAG_ERROR, POS_ERROR)
		{
			_code = c;
		}

        error(const tagd_code c, const std::string& msg) :
			abstract_tag(tagd_code_str(c), HARD_TAG_TYPE_OF, HARD_TAG_ERROR, POS_ERROR)
		{
			_code = c;
			this->relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, msg);
		}

        id_type message() const;
};

typedef std::vector<tagd::error> errors_t;

class errorable {
	protected:
		tagd_code _init;
		tagd_code _code;
		errors_t _errors;

	public:
		errorable() : _init{TAGD_OK}, _code{TAGD_OK}, _errors{} {}
		errorable(tagd_code c) : _init{c}, _code{c}, _errors{} {}
		virtual ~errorable() {}

		bool ok() const { return _code == TAGD_OK; }
		bool has_error() const { return (_code >= TAGD_ERR || _errors.size() > 0); }
		tagd_code code() const { return _code; }
		tagd::error last_error() const {
			if (_errors.size() == 0)
				return tagd::error();
			else
				return _errors[_errors.size()-1];
		}

		tagd_code last_error_relation(predicate p) {
			if (_errors.size() == 0)
				return tagd::TS_NOT_FOUND;

			return _errors[_errors.size()-1].relation(p);
		}

		// set and return
        tagd_code code(tagd_code c) { if(_code != c) _code = c; return c; }

		tagd_code error(const tagd::error&);
		tagd_code error(tagd::code, const predicate&);
		tagd_code error(tagd::code, const std::string&);

		// set and return code, set err msg to printf style formatted list
		tagd_code ferror(tagd::code, const char *, ...);
		tagd_code verror(tagd::code, const char *, const va_list&);

		void clear_errors() { _code = _init; _errors.clear(); }

		void print_errors(std::ostream& os = std::cerr) const;

		const errors_t& errors() const { return _errors; }
};

struct util {
	// returns c string of formatted printf string, or NULL on failure
	static char* csprintf(const char *, ...);
	static char* csprintf(const char *, const va_list&);
};


struct hard_tag {
	static part_of_speech pos(const std::string &id);
	static tagd_code get(abstract_tag&, const std::string &id);
	static part_of_speech term_pos(const id_type&, rowid_t*);
};

}  // namespace tagd

#include "tagd/domain.h"
#include "tagd/url.h"

