#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <cstdarg>

#include "tagd.h"

namespace tagd {

inline bool valid_predicate(const predicate &p) {
    return !p.object.empty();
}

inline bool valid_predicate(const id_type &object) {
    return !object.empty();
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
    if (!valid_predicate(object))
        return;

    P.insert(make_predicate(relator, object, modifier));
}

void print_tag_ids(const tag_set& T, std::ostream& os) {
	if (T.size() == 0) return;

	tagd::tag_set::iterator it = T.begin();
	os << it->id();
	++it;
	for (; it != T.end(); ++it) {
		os << ", " << it->id();
	}
	os << std::endl;
}

std::string tag_ids_str(const tag_set& T) {
	if (T.size() == 0) return std::string();

	std::stringstream ss;
	tagd::tag_set::iterator it = T.begin();
	ss << it->id();
	++it;
	for (; it != T.end(); ++it) {
		ss << ", " << it->id();
	}

	return ss.str();
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
			// TODO check if a contains b, and if so, merge
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

size_t merge_containing_tags(tag_set& A, const tag_set& B) {
    assert (B.size() != 0);

    if (A.size() == 0) {
        A.insert(B.begin(), B.end());
        return A.size();
    }

    tagd::tag_set::iterator a = A.begin();
	while (a != A.end()) {
		int merged = 0;
		for(tagd::tag_set::iterator b = B.begin(); b != B.end(); ++b) {
			if (b->rank().contains(a->rank())) {  // rank can contain itself (b == a)
				//a->predicates(b->relations);
				merged++;
			} else if (a->rank().contains(b->rank())) {
				//b->predicates(a->relations);
				A.insert(*b);
			}
		}
		if (merged)
			++a;
		else
			A.erase(a++);
	}
    return A.size();
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

tagd_code abstract_tag::relation(const tagd::predicate &p) {
    if (!valid_predicate(p))
        return this->code(TAG_ILLEGAL);

    if (p.relator == super_relator(this->pos())) {
        if (this->_super == p.object) { 
            return this->code(TAG_DUPLICATE);
        } else {
            this->_super = p.object;
            return this->code(TAGD_OK);
        }
    }

    predicate_pair pr = relations.insert(p);
    return this->code((pr.second ? TAGD_OK : TAG_DUPLICATE));

}

tagd_code abstract_tag::relation(const id_type &relator, const id_type &object) {
    if (!valid_predicate(object))
        return this->code(TAG_ILLEGAL);

    predicate_pair p = relations.insert(make_predicate(relator, object));
    return this->code((p.second ? TAGD_OK : TAG_DUPLICATE));
}


tagd_code abstract_tag::relation(
    const id_type &relator, const id_type &object, const id_type &modifier ) {
    if (!valid_predicate(object))
        return this->code(TAG_ILLEGAL);

    predicate_pair p = relations.insert(make_predicate(relator, object, modifier));
    return this->code((p.second ? TAGD_OK : TAG_DUPLICATE));
}

tagd_code abstract_tag::not_relation(const tagd::predicate &p) {
    if (!valid_predicate(p))
        return this->code(TAG_ILLEGAL);

	size_t erased = relations.erase(p);
    return this->code((erased ? TAGD_OK : TAG_UNKNOWN));
}

tagd_code abstract_tag::not_relation(const id_type &relator, const id_type &object) {
    if (!valid_predicate(object))
        return this->code(TAG_ILLEGAL);

	size_t erased = relations.erase(make_predicate(relator, object));
    return this->code((erased ? TAGD_OK : TAG_UNKNOWN));
}


tagd_code abstract_tag::predicates(const predicate_set &p) {
    // WTF slow and temporary
    for (predicate_set::const_iterator it = p.begin(); it != p.end(); ++it) {
		this->relation(*it);
    }

    return this->code(TAGD_OK);
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

bool is_number(const std::string& s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

struct output_relations {
    std::ostream& os;
    id_type _last_relator;
    output_relations(std::ostream& out) : os(out), _last_relator() {}
    void operator() (const predicate& p) {
		// use wildcard for empty relators
		id_type relator(p.relator.empty() ? "*" : p.relator);
        if (_last_relator == relator) {
            os << ", " <<  p.object;
		} else {
            os << std::endl << relator << ' ' <<  p.object;
			_last_relator = relator;
		}

		if (!p.modifier.empty()) {
			if (is_number(p.modifier)) // TODO a more robust scanner of types
				os << ' ' << p.modifier;
			else
				os << ' ' << '"' << p.modifier << '"';
		} 
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

// error helper class
tagd_code errorable::error(tagd::code c, const char *errfmt, ...) {
	_code = c;

	const size_t max_sz = 255;
	char buf[max_sz+1];
	va_list args;
	va_start (args, errfmt);
	int sz = vsnprintf(buf, max_sz, errfmt, args);
	if (sz <= 0) {
		std::cerr << "error vsnprintf failed: " << errfmt << std::endl;
		return c;
	}
	if ((size_t)sz >= max_sz) {
		buf[max_sz] = '\0';
		std::cerr << "error truncated to " << max_sz << "chars: " << buf << std::endl;
	}
	
	// std::cerr << "error: " << tagd_code_str(c) << ", " << buf << std::endl;

	_errors.push_back(tagd::error(c, buf));
	va_end (args);

	return c;
}

void errorable::print_errors(std::ostream& os) const {
	errors_t::const_iterator it = _errors.begin();
	if (it != _errors.end()) {
		os << *it << std::endl;
		++it;
	}

    for(; it != _errors.end(); ++it)
		os << std::endl << *it << std::endl;
}

} // namespace tagd

// code strings
const char* tagd_code_str(tagd::code c) {
	switch (c) {

// cases for each tagd_code generated by gen-codes-inc.pl
#include "codes.inc"

		default:         return "STR_EMPTY";
	}
}
