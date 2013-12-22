#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdio>

#include "tagd.h"

namespace tagd {

void insert_predicate(predicate_set& P,
    const id_type &relator, const id_type &object, const id_type &modifier ) {
    if (object.empty())
        return;

    P.insert(make_predicate(relator, object, modifier));
}

void print_tags(const tag_set& T, std::ostream& os) {
	if (T.size() == 0) return;

	tagd::tag_set::iterator it = T.begin();
	os << *it;
	for (++it; it != T.end(); ++it) {
		os << std::endl << std::endl << *it;
	}
	os << std::endl;
}

void print_tag_ids(const tag_set& T, std::ostream& os) {
	if (T.size() == 0) return;

	tagd::tag_set::iterator it = T.begin();
	os << it->id();
	for (++it; it != T.end(); ++it) {
		os << ", " << it->id();
	}
	os << std::endl;
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
		size_t merged = 0;
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

bool abstract_tag::operator==(const abstract_tag& rhs) const {
	if (this->pos() == POS_REFERENT)
		return (const referent&)*this == (const referent&)rhs;

	return (
		_id == rhs._id
		&& _super_relator == rhs._super_relator
		&& _super_object == rhs._super_object
		&& _pos == rhs._pos
		&& _rank == rhs._rank
		// std::equal will return true if first is subset of last
		&& relations.size() == rhs.relations.size()
		&& std::equal(relations.begin(), relations.end(),
					rhs.relations.begin())
	);
}

bool abstract_tag::operator<(const abstract_tag& rhs) const {
	if (_rank.empty() && _id == HARD_TAG_ENTITY)
		return true;
	if (rhs._rank.empty() && rhs._id == HARD_TAG_ENTITY)
		return false;

	assert( !(_rank.empty() && !rhs._rank.empty()) );
	assert( !(!_rank.empty() && rhs._rank.empty()) );

	if (this->pos() == POS_REFERENT)
		return (const referent&)*this < (const referent&)rhs;
	else if (_rank.empty() && rhs._rank.empty())  // allows tag w/o ranks to be inserted in tagsets
		return this->id() < rhs.id();
	else
		return this->_rank < rhs._rank;
}


void abstract_tag::clear() {
	_id.clear();
	_super_relator.clear();
	_super_object.clear();
	// _pos = POS_UNKNOWN;  
	_rank.clear();
	if (!relations.empty()) relations.clear();
}

tagd_code abstract_tag::relation(const tagd::predicate &p) {
    if (p.object.empty())
        return TAG_ILLEGAL;

    predicate_pair pr = relations.insert(p);
    return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd_code abstract_tag::relation(const id_type &relator, const id_type &object) {
    if (object.empty())
        return TAG_ILLEGAL;

    predicate_pair pr = relations.insert(make_predicate(relator, object));
    return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd_code abstract_tag::relation(
    const id_type &relator, const id_type &object, const id_type &modifier ) {
    if (object.empty())
        return TAG_ILLEGAL;

    predicate_pair pr = relations.insert(make_predicate(relator, object, modifier));
    return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd_code abstract_tag::not_relation(const tagd::predicate &p) {
    if (p.object.empty())
        return TAG_ILLEGAL;

	size_t erased = relations.erase(p);
    return (erased ? TAGD_OK : TAG_UNKNOWN);
}

tagd_code abstract_tag::not_relation(const id_type &relator, const id_type &object) {
    if (object.empty())
        return TAG_ILLEGAL;

	size_t erased = relations.erase(make_predicate(relator, object));
    return (erased ? TAGD_OK : TAG_UNKNOWN);
}

void abstract_tag::predicates(const predicate_set &p) {
	this->relations.insert(p.begin(), p.end());
}

bool abstract_tag::has_relator(const id_type &r) const {
    if (r.empty())
        return false;

    for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->relator == r) {
            return true;
        }
    }
   
    return false;
}

bool abstract_tag::related(const id_type &object) const {
    if (object.empty())
        return false;

    // WTF not sure if is the best way - propably more efficient methods
    for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->object == object) {
            return true;
        }
    }
   
    return false;
}

size_t abstract_tag::related(const id_type &object, predicate_set& how) const {
    if (object.empty())
        return 0;

	size_t matches = 0;
    // WTF not sure if is the best way - propably more efficient methods
    for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->object == object) {
			how.insert(*it);
            matches++;
        }
    }
   
    return matches;
}

// referents
id_type referent::context() const {
    for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->relator == HARD_TAG_CONTEXT)
            return it->object;
    }
   
    return id_type();
}

// tag output functions

// whether a label should be quoted
bool do_quotes(const id_type& s) {
	for (size_t i=0; i<s.size(); i++) {
		if (isspace(s[i]))
			return true;

		switch (s[i]) {
			case ';':
			case ':':
			case '=':
				return true;
			default:
				continue;
		}
	}

	return false;
}

void print_object (std::ostream& os, const predicate& p) {
	os << p.object;
	if (!p.modifier.empty()) {
		os << " = ";
		if (do_quotes(p.modifier))
			os << '"' << p.modifier << '"';
		else
			os << p.modifier;
	} 
}

// note it is a tag friend function, not abstract_tag::operator<<
std::ostream& operator<<(std::ostream& os, const abstract_tag& t) {
	if (!t.super_object().empty() && t.pos() != POS_URL)  // urls' super determined by nature of being a url
		os << t.id() << ' ' << t.super_relator() << ' ' << t.super_object();
	else
		os << t.id();

	predicate_set::const_iterator it = t.relations.begin();
	if (it == t.relations.end())
		return os;

	if (t.relations.size() == 1 && t.super_object().empty()) {
		os << ' ' << it->relator << ' ';
		print_object(os, *it);
		return os;
	} 

	id_type last_relator;
	for (; it != t.relations.end(); ++it) {
		if (last_relator == it->relator) {
			os << ", ";
			print_object(os, *it);
		} else {
			// use wildcard for empty relators
			os << std::endl << (it->relator.empty() ? "*" : it->relator) << ' ';
			print_object(os, *it);
			last_relator = it->relator;
		}
	}
 
    return os;
}
// end tag output functions

char* util::csprintf(const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	va_end (args);

	return util::csprintf(fmt, args);
}

static const size_t CSPRINTF_MAX_SZ = 255;
static char CSPRINTF_BUF[CSPRINTF_MAX_SZ+1];
char* util::csprintf(const char *fmt, const va_list& args) {
	int sz = vsnprintf(CSPRINTF_BUF, CSPRINTF_MAX_SZ, fmt, args);
	if (sz <= 0) {
		std::cerr << "error vsnprintf failed: " << fmt << std::endl;
		return NULL;
	}

	if ((size_t)sz >= CSPRINTF_MAX_SZ) {
		CSPRINTF_BUF[CSPRINTF_MAX_SZ] = '\0';
		std::cerr << "error truncated to " << CSPRINTF_MAX_SZ << "chars: " << CSPRINTF_BUF << std::endl;
		return NULL;
	}

	return CSPRINTF_BUF;
}

id_type error::message() const {
    for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
        if (it->object == HARD_TAG_MESSAGE)
            return it->modifier;
    }
   
    return id_type();
}

// error helper class
tagd_code errorable::error(const tagd::error& err) {
	_code = err.code();
	_errors.push_back(err);
	return _code;
}

tagd_code errorable::error(tagd::code c, const predicate& p) {
	tagd::error err(c);
	err.relation(p);
	return this->error(err);
}

tagd_code errorable::error(tagd::code c, const std::string& s) {
	tagd::error err(c, s);
	return this->error(err);
}

tagd_code errorable::ferror(tagd::code c, const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	va_end (args);

	return this->verror(c, errfmt, args);
}

tagd_code errorable::verror(tagd::code c, const char *errfmt, const va_list& args) {
	_code = c;

	char *msg = util::csprintf(errfmt, args);
	if (msg == NULL)
		_errors.push_back(tagd::error(c, "errorable::error() failed"));
	else {
		_errors.push_back(tagd::error(c, msg));
	}

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
