#pragma once

#include "tagd/codes.h"
#include "tagd/config.h"
#include "tagd/hard-tags.h"
#include "tagd/rank.h"

#include <string>
#include <set>
#include <vector>
#include <memory> // for shared_ptr
#include <iostream>
#include <sstream>
#include <cstdarg>  // for va_list

namespace tagd {

const size_t MAX_TAG_LEN = 2112;  // We are the Priests of the Temples of Syrinx
typedef std::string id_type;

typedef enum {
	OP_EQ,		// =
	OP_GT,  	// >
	OP_GT_EQ,	// >=
	OP_LT,		// <
	OP_LT_EQ	// <=
} operator_t;

typedef enum {
	TYPE_TEXT,
	TYPE_INTEGER,
	TYPE_FLOAT
} data_t;

/* TODO
typedef enum {
	PRINT_PRETTY,		// use whitespace for maximum readability
	PRINT_DELIM_NL,		// use newlines when delimitting lists
	PRINT_ID_ONLY,		// only print the tag id
	PRINT_ID_SUB_ONLY	// only print the tag id and the sub relation
} print_options_t;
*/

struct predicate {
	id_type relator;
	id_type object;
	id_type modifier;
	operator_t opr8r; // operates on the modifier
	data_t modifier_type;

	predicate() : opr8r{OP_EQ}, modifier_type{TYPE_TEXT} {}
	predicate(const id_type &r, const id_type &o) :
		relator(r), object(o), opr8r{OP_EQ}, modifier_type{TYPE_TEXT} {}
	predicate(const id_type &r, const id_type &o, const id_type &m) :
		relator(r), object(o), modifier(m), opr8r{OP_EQ}, modifier_type{TYPE_TEXT} {}
	predicate(const id_type &r, const id_type &o, const id_type &m, operator_t op) :
		relator(r), object(o), modifier(m), opr8r{op}, modifier_type{TYPE_TEXT} {}
	predicate(const id_type &r, const id_type &o, const id_type &m, operator_t op, data_t d) :
		relator(r), object(o), modifier(m), opr8r{op}, modifier_type{d} {}

	const char* op_c_str() const {
		switch (opr8r) {
			case OP_EQ:		return "=";
			case OP_GT:		return ">";
			case OP_GT_EQ:	return ">=";
			case OP_LT:		return "<";
			case OP_LT_EQ:	return "<=";
			default:		return "=";
		}
	}

	// modifier not used in comparisions
    // relator and object make a predicate unique - modifier is incendental
    bool operator<(const predicate& p) const {
        return ( relator < p.relator
                || ( relator == p.relator
                     && object < p.object )
                || ( relator == p.relator
                     && object == p.object
                     && modifier < p.modifier )
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
			&& opr8r == OP_EQ
		);
	}
};

std::ostream& operator<<(std::ostream&, const predicate&);

typedef std::set<predicate> predicate_set;
typedef std::pair<predicate_set::iterator,bool> predicate_pair;

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
    POS_SUB_RELATOR   = 1 << 1,
    POS_SUB_OBJECT    = 1 << 2,
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
    POS_FLAG          = 1 << 15,
    POS_INCLUDE       = 1 << 16
} part_of_speech;
const int POS_END     = 1 << 17;

/*\
|*| tagd_pos rank tree
|*| ------------------
|*| Each tagd_pos is a tagd::rank type
|*| that holds the HARD_TAG_POS rank + tadg_pos_ subtree rank 
|*|
|*| Encoding the tad_pos into the tagspace rank tree help us
|*|   * eliminate t_id <=> id_t conversion in tagddb
|*|     The rank now fills the role of:
|*|     * each rank uniquely identifies rank == old t_id 
|*|     * rank == t_id 
|*|       relation    tad rank     offset
|*|       --------    --------     ------
|*|       id          == tagd::rank + POS_ID (0)
|*|       sub_relator == tagd::rank + POS_SUB
|*|       sub_relator == tagd::rank + POS_SUB
|*|               pos == tagd::rank + POS_POS
|*|     * rank == (rank + pos) to deduce value of other each pos in a tag
|*|     * makes a tagd rank an iterable set over a buffer of data
|*|       from start == rank == id to end
|*|     * allow to deduce the end of a tag in a buffer of data given any rank,
|*|		* allows us to copy an entire tagdb/tagspace (by value)
|*|		  using only a buffer of utf8 and list of ranks
|*|       VERY PORTABLE!
|*| So that each tagd_id utf8 string value 
|*| tagdb indexes will optimize reads of
|*| an entire tag as all the data will be

|*| Also this allows us to make tagd_pos searchable and extensible in TAGL
|*| And allow to seek directly within a rank index
\*/

// wrap in struct so we can define here
struct tag_util {
    static std::string pos_str(part_of_speech p) {
        switch (p) {
            case POS_UNKNOWN: return "POS_UNKNOWN";
            case POS_TAG:    return "POS_TAG";
            case POS_SUB_RELATOR:    return "POS_SUB_RELATOR";
            case POS_SUB_OBJECT:    return "POS_SUB_OBJECT";
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
        id_type _sub_relator; // subordinate relation (i.e. _is_a)
        id_type _super_object;  // superordinate, parent, or hypernym object in tree
        part_of_speech _pos;
        tagd::rank _rank;
		tagd::code _code;

        // set and return
        tagd::code code(tagd::code c) { _code = c; return _code; }
    public:
        // empty tag
        abstract_tag() :
			_id(), _sub_relator(HARD_TAG_SUB), _super_object(),
			_pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};
        virtual ~abstract_tag() {};

        abstract_tag(const id_type& id) :
			_id(id), _sub_relator(HARD_TAG_SUB), _super_object(),
			_pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};

        abstract_tag(const part_of_speech& p) :
			_id(), _sub_relator(HARD_TAG_SUB), _super_object(),
			_pos(p), _rank(), _code(TAGD_OK) {};

        // id, but no sub
        abstract_tag(const id_type& id, const part_of_speech& p) :
			_id(id), _sub_relator(HARD_TAG_SUB), _super_object(),
			_pos(p), _rank(), _code(TAGD_OK) {};

        abstract_tag(const id_type& id, const id_type& sub_obj, const part_of_speech& p) :
			_id(id), _sub_relator(HARD_TAG_SUB), _super_object(sub_obj),
			_pos(p), _rank(), _code(TAGD_OK) {};

        abstract_tag(
				const id_type& id,
				const id_type& rel,
				const id_type& obj,
				const part_of_speech& p
				) :
			_id(id), _sub_relator(rel), _super_object(obj),
			_pos(p), _rank(), _code(TAGD_OK) {};

        predicate_set relations;

		void clear();
		bool empty() const {
			return (
				_id.empty()
				//&& _sub_relator.empty()  // sub relators are set by constructor
				&& _super_object.empty()
				&& _rank.empty()
				&& relations.empty()
			);
		}

        const id_type& id() const { return _id; }
        void id(const id_type& A) { _id = A; }

        const id_type& sub_relator() const { return _sub_relator; }
        void sub_relator(const id_type& s) { _sub_relator = s; }

        const id_type& super_object() const { return _super_object; }
        void super_object(const id_type& s) { _super_object = s; }

        const part_of_speech& pos() const { return _pos; }
        void pos(const part_of_speech& p) { _pos = p; }

        const tagd::rank& rank() const { return _rank; }
        void rank(const tagd::rank& r) { _rank = r; }

        // returns code because rank::init() will fail on invalid bytes
        tagd::code rank(const char *bytes) { return _rank.init(bytes); }

        tagd::code code() const { return _code; }
        bool ok() const { return _code == TAGD_OK; }

        tagd::code relation(const predicate&);
                            // relator, object
        tagd::code relation(const id_type&, const id_type&);
                            // relator, object, modifier
        tagd::code relation(const id_type&, const id_type&, const id_type&);
                            // relator, object, modifier, opr8r
        tagd::code relation(const id_type&, const id_type&, const id_type&, operator_t);
                            // relator, object, modifier, opr8r, modifier_type
        tagd::code relation(const id_type&, const id_type&, const id_type&, operator_t, data_t);

        tagd::code not_relation(const predicate&);
                            // relator, object
        tagd::code not_relation(const id_type&, const id_type&);

		// modifier not needed for negations (erasing)
        // tagd::code not_relation(const id_type&, const id_type&, const id_type&);

        void predicates(const predicate_set&);

        bool has_relator(const id_type&) const;
        bool has_relator(const id_type&, predicate_set& how) const;
		// related to object
        bool related(const id_type& object) const;
        // fill predicate set with predicates matching object, return num matches
        size_t related(const id_type& object, predicate_set& how) const;

        // relator, object
        bool related(const id_type& relator, const id_type& object) const;
        bool related(const predicate& p) const {
			auto it = relations.find(p);
            if (it == relations.end())
				return false;

			return (p.modifier.empty() || p.modifier == it->modifier);
        }

        // relator, object, modifier
        bool related(const id_type& r, const id_type& o, const id_type& m) const {
            return this->related(predicate(r, o, m));
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
			_sub_relator = HARD_TAG_IS_A;
		};

        tag(const id_type& id, const id_type& sub_obj) :
			abstract_tag(id, HARD_TAG_IS_A, sub_obj, POS_TAG) {};

        tag(const id_type& id, const id_type& sub_rel, const id_type& sub_obj) :
			abstract_tag(id, sub_rel, sub_obj, POS_TAG) {};
};

// relates a subject to an object
// known as the linguistic "Copula" - usually a linking verb,
// but not necissarily so
// e.g. "dog is_a animal"; 'is_a' being the relator
class relator : public abstract_tag {
    public:
        relator(const id_type& id) :
			abstract_tag(id, HARD_TAG_TYPE_OF, HARD_TAG_RELATOR, POS_RELATOR) {};

        relator(const id_type& id, const id_type& sub_obj) :
			abstract_tag(id, HARD_TAG_TYPE_OF, sub_obj, POS_RELATOR) {};

        relator(const id_type& id, const id_type& sub_rel, const id_type& sub_obj) :
			abstract_tag(id, sub_rel, sub_obj, POS_RELATOR) {};
};

// 'wh.*|how' words (eg who, what, where, when, why, how)
// there are two types of interrogator:
// 1. one that identifies and defines a tag that is a type of interrogator
//    (ie. subordinate to "_interrogator"), used in get, put, etc. operations
//    ex. what _is_a _interrogator
// 2. or, one that holds an object of inquiry used in query operations
//    ex. what _is_a mammal has tail can bark
class interrogator : public abstract_tag {
    public:
        interrogator(const id_type& id) :
			abstract_tag(id, POS_INTERROGATOR) {};

        interrogator(const id_type& id, const id_type& sub_obj) :
			abstract_tag(id, HARD_TAG_TYPE_OF, sub_obj, POS_INTERROGATOR) {};

        interrogator(const id_type& id, const id_type& sub_rel, const id_type& sub_obj) :
			abstract_tag(id, sub_rel, sub_obj, POS_INTERROGATOR) {};
};

class referent : public abstract_tag {
		// _id is the thing that refers
		// _super_object is the thing refered to
    public:

        referent() :
			abstract_tag(POS_REFERENT)
		{
			_sub_relator = HARD_TAG_REFERS_TO;
		};

        referent(const abstract_tag& t) :
			abstract_tag(t.id(), t.sub_relator(), t.super_object(), POS_REFERENT)
		{ relations = t.relations; }

        referent(const id_type& refers, const id_type& refers_to) :
			abstract_tag(refers, HARD_TAG_REFERS_TO, refers_to, POS_REFERENT)
		{}

        referent(const id_type& refers, const id_type& refers_to, const id_type& c) :
			abstract_tag(refers, HARD_TAG_REFERS_TO, refers_to, POS_REFERENT)
		{ context(c); }

		// TODO don't allow overridding HARD_TAG_REFERS_TO
		// the problem, though, is that the getter method also gets deleted
        // void sub_relator(const id_type& s) = delete;

        const id_type& refers() const { return _id; }
        const id_type& refers_to() const { return _super_object; }
        id_type context() const;
        void context(const id_type& c) {
			this->relation( HARD_TAG_CONTEXT, c);
		}

        bool operator==(const referent& rhs) const {
			return (
				_id == rhs._id &&
				// leave _sub_relator out of equality and less comparisons
				// because _id, _super_object, and _context make it unique
				//_sub_relator == rhs._sub_relator &&
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

        error(const tagd::code c) :
			abstract_tag(tagd_code_str(c), HARD_TAG_TYPE_OF, HARD_TAG_ERROR, POS_ERROR)
		{
			_code = c;
		}

        error(const tagd::code c, const std::string& msg) :
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
		tagd::code _init;
		tagd::code _code;

		// errors_t pointer uses lazy initialization
		std::shared_ptr<errors_t> _errors;

		void init_errors() {
			if (_errors == nullptr) {
				// create even if this is not the owner
				// the owners destructor will delete it
				_errors = std::make_shared<errors_t>();
			}
		}

	public:
		bool report_errors;  // turn on/off error reporting

		errorable() :
			_init{TAGD_OK}, _code{TAGD_OK}, _errors{nullptr}, report_errors{true} {}

		errorable(tagd::code c) :
			_init{c}, _code{c}, _errors{nullptr}, report_errors{true} {}

		virtual ~errorable() {}

		bool ok() const { return _code == TAGD_OK; }
		size_t size() const { return (_errors == nullptr ? 0 : _errors.get()->size()); }
		bool has_errors() const { return (_code >= TAGD_ERR || this->size() > 0); }
		tagd::code code() const { return _code; }

		tagd::error last_error() const;
		tagd::code last_error_relation(predicate);

		// set and return
        tagd::code code(tagd::code c) { return _code = c; }

		// return the most severe error code in the set
		// compared to tagd::code passed in
        tagd::code most_severe(tagd::code);
        tagd::code most_severe() { return most_severe(_init); }

		tagd::code error(const tagd::error&);
		tagd::code error(tagd::code, const predicate&);
		tagd::code error(tagd::code, const std::string&);

		// copy rhs._errors into this->_errors return *this
		errorable& copy_errors(const errorable &);

		// copy rhs _errors to this and
		// assign this _errors pointer to rhs _errors pointer
		errorable& share_errors(errorable &);

		// set and return code, set err msg to printf style formatted list
		tagd::code ferror(tagd::code, const char *, ...);
		tagd::code verror(tagd::code, const char *, va_list&);

		void clear_errors();
		void print_errors(std::ostream& os = std::cerr) const;
		errors_t errors() const;
};

struct util {
	// returns c string of formatted printf string, or NULL on failure
	static char* csprintf(const char *, ...);
	static char* csprintf(const char *, va_list&);
	static std::string esc_and_quote(id_type);
};

}  // namespace tagd

#include "tagd/domain.h"
#include "tagd/url.h"

