#pragma once
#define TAGD_H_INCLUDED

#include "tagd/codes.h"
#include "tagd/config.h"
#include "tagd/hard-tags.h"
#include "tagd/http.h"
#include "tagd/rank.h"

#include <string>
#include <string_view>
#include <set>
#include <vector>
#include <memory> // for shared_ptr
#include <atomic>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <cstdarg>  // for va_list
#include <cassert>

namespace tagd {

const size_t MAX_TAG_LEN = 2112;   // We are the Priests of the Temples of Syrinx
using id_string = std::string;     // owning string
using id_view = std::string_view;  // non-owning read-only <pointer + size> to c-string
const static id_string EMPTY_ID;     // for returning empty `const id_string&`

struct predicate {
	id_string relator;
	id_string object;
	id_string modifier;
	operator_t opr8r; // operates on the modifier
	data_t modifier_type;

	// string value members keep implicit copy/move semantics sufficient here
	predicate() : opr8r{OP_EQ}, modifier_type{TYPE_STRING} {}
	predicate(id_view r, id_view o) :
		relator(r), object(o), opr8r{OP_EQ}, modifier_type{TYPE_STRING} {}
	predicate(id_view r, id_view o, id_view m) :
		relator(r), object(o), modifier(m), opr8r{OP_EQ}, modifier_type{TYPE_STRING} {}
	predicate(id_view r, id_view o, id_view m, operator_t op) :
		relator(r), object(o), modifier(m), opr8r{op}, modifier_type{TYPE_STRING} {}
	predicate(id_view r, id_view o, id_view m, operator_t op, data_t d) :
		relator(r), object(o), modifier(m), opr8r{op}, modifier_type{d} {}

	// numeric string promotion preserves stable set ordering across declared modifier types
	static bool cmp_modifier_lt(const predicate& lhs, const predicate& rhs); // returns whether lhs < rhs
	// numeric string promotion keeps deep equality aligned with modifier typing rules
	static bool cmp_modifier_eq(const predicate& rhs, const predicate& lhs); // returns whether rhs == lhs
	const char* op_c_str() const;

	/*\
	|*| TODO: Activation and canonical ordering
	|*| Currently orders by relator, object (string comparison), then modifier as a
	|*| literal tiebreaker via cmp_modifier_lt. Safe for unranked predicates.
	|*| Planned: canonical tagspace dump() requires rank-aware ordering — relator and
	|*| object are tag ids whose ranks, once assigned via tagdb lookup, define the
	|*| canonical predicate sequence within a predicate_set.
	|*| modifier is always a literal value and is not rank-promoted.
	|*| Resolution depends on the same activation/comparator design as abstract_tag::operator<.
	|*| When and where the rank lookup take place is TBD.
	\*/
	// predicate_set ordering contract; deeper predicate equality still lives in operator==
	// noexcept not safe: cmp_modifier_lt calls std::stold which throws on malformed numeric strings
    bool operator<(const predicate& p) const;
    bool operator==(const predicate& p) const;
	bool operator!=(const predicate& p) const;
	bool empty() const;
};

std::ostream& operator<<(std::ostream&, const predicate&);

typedef std::set<predicate> predicate_set;
typedef std::pair<predicate_set::iterator,bool> predicate_pair;

// make predicate and insert relator, object, modifier
void insert_predicate(predicate_set&, id_view, id_view, id_view);

class abstract_tag;

typedef std::vector<tagd::id_string> id_vec;
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
bool tag_set_equal(const tag_set& A, const tag_set& B);
/************** end tag_set defs ************/

class abstract_tag {
    protected:
        id_string _id;
        id_string _sub_relator; // subordinate relation (i.e. _is_a)
        id_string _super_object;  // superordinate, parent, or hypernym object in tree
        part_of_speech _pos;
        tagd::rank _rank;
		tagd::code _code;
        tagd::code code(tagd::code c) { return _code = c; } // set and return
    public:
        // Semantic Identity: {_id, _sub_relator, _super_object, _rank} are immutable after construction.
        // Structural Identity: a tag rank represents a fixed point in the tagspace topology.

        // Empty tag used only as a temporary bridge where legacy out-parameter APIs
        // still replace the whole value after lookup/parsing.
        abstract_tag() :
				_id(), _sub_relator(HARD_TAG_SUB), _super_object(),
				_pos(POS_UNKNOWN), _rank(), _code(TAGD_OK) {};
        // virtual destructor suppresses implicit moves for STL-friendly value semantics
        abstract_tag(const abstract_tag&) = default;
        abstract_tag(abstract_tag&&) noexcept = default;
        virtual ~abstract_tag() {};

        // Bare subject: parser ">> dog _has ..." — id only, sub_relator defaults to HARD_TAG_SUB
        abstract_tag(id_view id, const part_of_speech& p = POS_UNKNOWN) :
			_id(id), _sub_relator(HARD_TAG_SUB), _super_object(),
			_pos(p), _rank(), _code(TAGD_OK) {};

        // Semantic: full identity construction with explicit sub-relation
		abstract_tag(
				id_view id,
				id_view sub_rel,
				id_view super_obj,
				const part_of_speech& p
				) :
			_id(id), _sub_relator(sub_rel), _super_object(super_obj),
			_pos(p), _rank(), _code(TAGD_OK) {};

        // Rank upgrade: tagdb activation assigns structural rank to a semantic tag.
        abstract_tag(const abstract_tag& sem, const tagd::rank& r) :
			_id(sem._id),
			_sub_relator(sem._sub_relator),
			_super_object(sem._super_object),
			_pos(sem._pos),
			_rank(r),
			_code(sem._code),
			relations(sem.relations)
		{};

        predicate_set relations;

		// member swap as single noexcept seam for whole-tag exchange
		void swap(abstract_tag&) noexcept;
		abstract_tag& operator=(const abstract_tag&);
		abstract_tag& operator=(abstract_tag&&) noexcept = default;
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

        const id_string& id() const { return _id; }
		void id(id_view) = delete;

        const id_string& sub_relator() const { return _sub_relator; }
		void sub_relator(id_view) = delete;

        const id_string& super_object() const { return _super_object; }
		void super_object(id_view) = delete;

        part_of_speech pos() const { return _pos; }
        void pos(const part_of_speech& p) { _pos = p; }

        const tagd::rank& rank() const { return _rank; }
		void rank(const tagd::rank&) = delete;
		void rank(const char *) = delete;
        bool has_rank() const { return !_rank.empty(); }

        tagd::code code() const { return _code; }
        bool ok() const { return _code == TAGD_OK; }

        // TODO(cpp23-return-contracts): keep these mutation seams [[nodiscard]]
        // and drive callers toward immediate-check style instead of relying on
        // later ambient error/session state inspection.
        [[nodiscard]] tagd::code relation(const predicate&);
        [[nodiscard]] tagd::code relation(id_view, id_view); // relator, object
        [[nodiscard]] tagd::code relation(id_view, id_view, id_view); // relator, object, modifier
        [[nodiscard]] tagd::code relation(id_view, id_view, id_view, operator_t); // relator, object, modifier, opr8r
        [[nodiscard]] tagd::code relation(id_view, id_view, id_view, operator_t, data_t); // relator, object, modifier, opr8r, modifier_type

        // TODO(cpp23-return-contracts): audit whether not_relation should also
        // be [[nodiscard]] and require the same immediate-check discipline.
        tagd::code not_relation(const predicate&);
                            // relator, object
        tagd::code not_relation(id_view, id_view);

		// modifier not needed for negations (erasing)
        // tagd::code not_relation(const id_string&, const id_string&, const id_string&);

        void predicates(const predicate_set&);

        bool has_relator(id_view) const;
        bool has_relator(id_view, predicate_set& how) const;
		// related to object
        bool related(id_view object) const;
        // fill predicate set with predicates matching object, return num matches
        size_t related(id_view object, predicate_set& how) const;

        // relator, object
        bool related(id_view relator, id_view object) const;
        bool related(const predicate& p) const {
			auto it = relations.find(p);
            if (it == relations.end())
				return false;

			return (p.modifier.empty() || p.modifier == it->modifier);
        }

        // relator, object, modifier
        bool related(id_view r, id_view o, id_view m) const {
            return this->related(predicate(r, o, m));
		}

        // deep equality for whole-tag state; intentionally stronger than operator<
        bool operator==(const abstract_tag&) const;
        bool operator!=(const abstract_tag& rhs) const { return !(*this == rhs); }

		/*\
		|*| TODO: Activation and canonical ordering
		|*| Currently orders by _id (string comparison) — safe for unranked tags.
		|*| Planned: when a tag has rank assigned via tagdb lookup, ordering
		|*| and equality should promote to rank-based comparison for canonical correctness.
		|*| Design options under consideration:
		|*|   1. Just-in-time lookup: pass tagdb context to rank-aware free functions or
		|*|      algorithm overloads; operator< stays id-based; rank-aware comparisons
		|*|      use explicit comparator types (e.g. tag_rank_order) at the call site.
		|*|   2. operator== falls back to _id comparison if either operand has empty rank;
		|*|      compares by rank only when both tags are ranked.
		|*| Resolution of this design will affect tag_set ordering, equality, and output.
		\*/
        // tag_set identity ordering; intentionally shallower than operator==
        bool operator<(const abstract_tag&) const;

		std::string str() const {
			std::stringstream ss;
			ss << *this;
			return ss.str();
		}

        friend std::ostream& operator<<(std::ostream&, const abstract_tag&);
};

// ADL swap hook for generic constant-time tag exchange
void swap(abstract_tag&, abstract_tag&) noexcept;

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

// relates a subject to an object
// known as the linguistic "Copula" - usually a linking verb,
// but not necissarily so
// e.g. "dog is_a animal"; 'is_a' being the relator
class relator : public abstract_tag {
    public:
        relator(id_view id) :
			abstract_tag(id, HARD_TAG_SUB, HARD_TAG_RELATOR, POS_RELATOR) {};

        relator(id_view id, id_view sub_obj) :
			abstract_tag(id, HARD_TAG_SUB, sub_obj, POS_RELATOR) {};

        relator(id_view id, id_view sub_rel, id_view sub_obj) :
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
        interrogator() :
			abstract_tag(id_view{}, POS_INTERROGATOR) {};

        interrogator(id_view id) :
			abstract_tag(id, POS_INTERROGATOR) {};

        interrogator(id_view id, id_view sub_obj) :
			abstract_tag(id, HARD_TAG_SUB, sub_obj, POS_INTERROGATOR) {};

        interrogator(id_view id, id_view sub_rel, id_view sub_obj) :
			abstract_tag(id, sub_rel, sub_obj, POS_INTERROGATOR) {};
};

class referent : public abstract_tag {
	protected:
		tagd::code validate() {
			const id_string& c = this->context();
			if ( _id.empty() || _id == HARD_TAG_ENTITY  // _refers
			  || _super_object.empty() // `_refers_to _entity` -- OK
			  || c.empty() || c == HARD_TAG_ENTITY )
			{
				return this->code(tagd::TS_MISUSE);
			} else {
				return this->code(tagd::TAGD_OK);
			}
		}

    public:
		// _id is the thing that refers
		// _super_object is the thing refered to
        // One-shot: sub_relator explicitly REFERS_TO, no post-construction mutation
        referent() :
			abstract_tag("", HARD_TAG_REFERS_TO, "", POS_REFERENT)
		{};

        referent(const abstract_tag& t) :
			abstract_tag(t.id(), t.sub_relator(), t.super_object(), POS_REFERENT)
		{ relations = t.relations; }

        referent(id_view refers, id_view refers_to, id_view c) :
			abstract_tag(refers, HARD_TAG_REFERS_TO, refers_to, POS_REFERENT)
		{
			context(c);
			this->validate();
		}

        const id_string& refers() const { return _id; }
        const id_string& refers_to() const { return _super_object; }
		const id_string& context() const;
		tagd::code context(id_view c) {
			auto tc = this->relation(HARD_TAG_CONTEXT, c);
			if (tc != tagd::TAGD_OK)
				return tc;
			else
				return this->validate();
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


#include "tagd/event.h"

// TODO modifier maybe
/*
   measures the object of a predicate (i.e. answers "how many?")
   however, it could also represent a more general sense of quantification
   such as determiners (grammer) or variable binding (logic)
   http://en.wikipedia.org/wiki/Quantifier
*/
// class modifier : public tag {

class error : public event {
	public:
        error() : event()
		{
			_pos = POS_ERROR;
		}

        // Create a new error instance for a tagd status code.
        error(const tagd::code);

		// Parse an existing err: identity into an error object.
		error(const std::string&);

		// Create a new error instance with a message relation.
		error(const tagd::code, const std::string&);

        const id_string& message() const;

		// returns error object given printf style formatted list
		static error ferror(tagd::code, const char *, ...);
};

typedef std::vector<tagd::error> errors_t;

class errorable {
	protected:
		tagd::code _init;
		tagd::code _code;

		// shared_ptr keeps share_errors aliasing cheap across cooperating errorable instances
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

		const tagd::error& last_error() const;
		tagd::code last_error_relation(const predicate&);

		// set and return
        tagd::code code(tagd::code c) { return _code = c; }

		// return the most severe error code in the set
		// compared to tagd::code passed in
        tagd::code most_severe(tagd::code) const;
        tagd::code most_severe() const { return most_severe(_init); }

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

		// TODO WTF cant we return a const reference instead of a copy?
		const errors_t& errors() const;
};

class session : public errorable {
	protected:
		id_string _id;
		id_string _started_at;
		std::atomic<uint64_t> _sequence;

	public:
		session();
		session(const id_string&, const id_string&);
		// atomic sequence suppresses implicit copy semantics, so session spells them out
		session(const session&);
		session& operator=(const session&);
		virtual ~session() {}

		const id_string& id() const { return _id; }
		const id_string& started_at() const { return _started_at; }
		uint64_t sequence() const { return _sequence.load(); }
		uint64_t next_sequence() { return ++_sequence; }
};

class session_factory {
	private:
		class impl;
		std::shared_ptr<impl> _impl;

	public:
		session_factory();
		session create();
};

struct util {
	// returns c string of formatted printf string, or NULL on failure
	static char* csprintf(const char *, ...);
	static char* csprintf(const char *, va_list&);
	static std::string esc_and_quote(id_string);
};

}  // namespace tagd

#include "tagd/domain.h"
#include "tagd/url.h"
#include "tagd/io.h"
#include "tagd/file.h"
