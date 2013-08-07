#include <algorithm>
#include <sstream>
#include <cstring>
#include <cassert>

#include "tagd.h"

namespace tagd {

inline bool valid_predicate(const predicate &p) {
    return !(p.relator.empty() || p.object.empty());
}

inline bool valid_predicate(const id_type &relator, const id_type &object) {
    return !(relator.empty() || object.empty());
}

inline predicate make_predicate(const id_type& r, const id_type& o, const id_type& q) {
    predicate p = { r, o, q };
    return p;
}

inline predicate make_predicate(const id_type& r, const id_type& o) {
    predicate p = { r, o, "" };
    return p;
}

void insert_predicate(predicate_set& P,
    const id_type &relator, const id_type &object, const id_type &modifier ) {
    if (!valid_predicate(relator, object))
        return;

    P.insert(make_predicate(relator, object, modifier));
}

void print_tag_ids(const tag_set& T) {
	if (T.size() == 0) return;

	tagd::tag_set::iterator it = T.begin();
	std::cout << it->id();
	++it;
	for (; it != T.end(); ++it) {
		std::cout << ", " << it->id();
	}
	std::cout << std::endl;
}

void merge_tags(tag_set& A, const tag_set& B) {
    if (A.empty()) {
        A.insert(B.begin(), B.end());
        return;
    }

    tagd::tag_set::iterator a = A.begin();
    for (tagd::tag_set::iterator b = B.begin(); b != B.end(); ++b) {
        a = A.insert(a, *b);
        if ( a->id() == b->id() ) { // duplicate
            // copy/erase/insert because sets are const iterators
            tagd::abstract_tag t(*a);
            t.predicates(b->relations);  // merge relations
            tagd::tag_set::iterator it = a; // for speeding up insertion
            if (it != A.begin()) --it;
            A.erase(a);
            a = A.insert(it, t);
        }
    }
}

size_t merge_tags_erase_diffs(tag_set& A, const tag_set& B) {
    assert (B.size() != 0);

    if (A.size() == 0) {
        A.insert(B.begin(), B.end());
        return A.size();
    }

    // TODO find a more efficient way to erase/insert against A without a temp containter
    tagd::tag_set T;  // tmp
    tagd::tag_set::iterator a = A.begin();
    tagd::tag_set::iterator b = B.begin();
    tagd::tag_set::iterator it = T.begin();  

    while (b != B.end()) {
        if (a == A.end())
            break; 
        
        if (*a < *b) {
            ++a;
        } else if (*b < *a) {
            ++b;
        } else {
            assert( a->id() == b->id() );
            tagd::abstract_tag t(*a);
            t.predicates(b->relations);  // merge relations
            it = T.insert(it, t);
            ++a;
            ++b;
        }
    }

    A.clear();
    if (T.size() > 0)
        A.insert(T.begin(), T.end());

    return T.size();
}

bool tag_set_equal(const tag_set A, const tag_set B) {
    if (A.size() != B.size())
        return false;

    tagd::tag_set::iterator a = A.begin();
    tagd::tag_set::iterator b = B.begin();

    while ( (a != A.end()) && (b != B.end()) ) {
        if (*a != *b) {
            return false;
        }
        ++a; ++b;
    }

    return true;
}

void abstract_tag::clear() {
	_id.clear();
	_super.clear();
	// _pos = POS_UNKNOWN;  
	_rank.clear();
	if (!relations.empty()) relations.clear();
}

bool abstract_tag::empty() {
	return (
		_id.empty()
		&& _super.empty()
		&& _rank.empty()
		&& relations.empty()
	);
}

tag_code abstract_tag::relation(const tagd::predicate &p) {
    if (!valid_predicate(p))
        return TAG_ILLEGAL;

    if (p.relator == super_relator(this->pos())) {
        if (this->_super == p.object) { 
            return TAG_DUPLICATE;
        } else {
            this->_super = p.object;
            return TAG_OK;
        }
    }

    predicate_pair pr = relations.insert(p);
    return (pr.second ? TAG_OK : TAG_DUPLICATE);

}

tag_code abstract_tag::relation(const id_type &relator, const id_type &object) {
    if (!valid_predicate(relator, object))
        return TAG_ILLEGAL;

    predicate_pair p = relations.insert(make_predicate(relator, object));
    return (p.second ? TAG_OK : TAG_DUPLICATE);
}


tag_code abstract_tag::relation(
    const id_type &relator, const id_type &object, const id_type &modifier ) {
    if (!valid_predicate(relator, object))
        return TAG_ILLEGAL;

    predicate_pair p = relations.insert(make_predicate(relator, object, modifier));
    return (p.second ? TAG_OK : TAG_DUPLICATE);
}

tag_code abstract_tag::not_relation(const tagd::predicate &p) {
    if (!valid_predicate(p))
        return TAG_ILLEGAL;

	size_t erased = relations.erase(p);
    return (erased ? TAG_OK : TAG_UNKNOWN);
}

tag_code abstract_tag::not_relation(const id_type &relator, const id_type &object) {
    if (!valid_predicate(relator, object))
        return TAG_ILLEGAL;

	size_t erased = relations.erase(make_predicate(relator, object));
    return (erased ? TAG_OK : TAG_UNKNOWN);
}


tag_code abstract_tag::predicates(const predicate_set &p) {
    // WTF slow and temporary
    for (predicate_set::const_iterator it = p.begin(); it != p.end(); ++it) {
		this->relation(*it);
    }

    return TAG_OK;
}

bool abstract_tag::related(const id_type &object, id_type *how) const {
    if (object.empty())
        return false;

    // WTF not sure if is the best way - propably more efficient methods
    for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->object == object) {
            if (how != NULL)
                *how = it->relator;  // answers how it is related
            return true;
        }
    }
   
    return false;
}

bool abstract_tag::operator==(const abstract_tag& rhs) const {
    return (
           _id == rhs._id
        && _super == rhs._super
        && _pos == rhs._pos
        && _rank == rhs._rank
        // std::equal will return true if first is subset of last
        && relations.size() == rhs.relations.size()
        && std::equal(relations.begin(), relations.end(),
                      rhs.relations.begin())
    );
}

// tag output functions
struct output_relations {
    std::ostream& os;
    id_type _last_relator;
    output_relations(std::ostream& out) : os(out), _last_relator() {}
    void operator() (const predicate& p) {
        if (_last_relator == p.relator) {
            os << ", " <<  p.object;
		} else {
            os << std::endl << p.relator << ' ' <<  p.object;
			_last_relator = p.relator;
		}

		if (!p.modifier.empty())
			os << ' ' << p.modifier;
    }
};


// note it is a tag freind function, not abstract_tag::operator<<
std::ostream& operator<<(std::ostream& os, const abstract_tag& t) {
	if (!t.super().empty())
		os << t.id() << ' ' << super_relator(t.pos()) << ' ' << t.super();
	else
		os << t.id();
    for_each ( t.relations.begin(), t.relations.end(), output_relations(os) );
    return os;
}
// end tag output functions

} // namespace tagd
