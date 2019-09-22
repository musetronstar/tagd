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

	P.insert(predicate(relator, object, modifier));
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
	if (B.empty())
		return 0;

	if (A.size() == 0) {
		A.insert(B.begin(), B.end());
		return A.size();
	}

	auto a = A.begin();
	while (a != A.end()) {
		size_t merged = 0;
		for(auto b = B.begin(); b != B.end(); ++b) {
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
		&& _sub_relator == rhs._sub_relator
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

	assert( (_rank.empty() && rhs._rank.empty()) ||
			(!_rank.empty() && !rhs._rank.empty()) );

	if (this->pos() == POS_REFERENT)
		return (const referent&)*this < (const referent&)rhs;
	else if (_rank.empty() && rhs._rank.empty())  // allows tag w/o ranks to be inserted in tagsets
		return this->id() < rhs.id();
	else
		return this->_rank < rhs._rank;
}


void abstract_tag::clear() {
	_id.clear();
	_sub_relator.clear();
	_super_object.clear();
	// _pos = POS_UNKNOWN;  
	_rank.clear();
	if (!relations.empty()) relations.clear();
}

tagd::code abstract_tag::relation(const tagd::predicate &p) {
	if (p.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(p);
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(const id_type &relator, const id_type &object) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	const id_type &relator, const id_type &object, const id_type &modifier ) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object, modifier));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	const id_type &relator, const id_type &object, const id_type &modifier, operator_t op ) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object, modifier, op));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	const id_type &relator, const id_type &object, const id_type &modifier, operator_t op, data_t d) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object, modifier, op, d));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::not_relation(const tagd::predicate &p) {
	if (p.empty())
		return TAG_ILLEGAL;

	size_t erased = relations.erase(p);
	return (erased ? TAGD_OK : TAG_UNKNOWN);
}

tagd::code abstract_tag::not_relation(const id_type &relator, const id_type &object) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	size_t erased = relations.erase(predicate(relator, object));
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

bool abstract_tag::has_relator(const id_type &r, predicate_set& P) const {
	if (r.empty())
		return false;

	bool match = false;
	for (auto it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == r) {
			match = true;
			P.insert(*it);
		}
	}
   
	return match;
}

bool abstract_tag::related(const id_type &object) const {
	if (object.empty())
		return false;

	// WTF not sure if is the best way - propably more efficient methods
	for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->object == object)
			return true;
	}
   
	return false;
}

bool abstract_tag::related(const id_type &relator, const id_type &object) const {
	for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == relator && it->object == object)
			return true;
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
const id_type& referent::context() const {
	for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == HARD_TAG_CONTEXT)
			return it->object;
	}

	return EMPTY_ID;
}

// tag output functions

// whether a label should be quoted
std::string util::esc_and_quote(id_type s) {
	bool do_quotes = false;
	for (size_t i=0; i<s.size(); i++) {
		if (isspace(s[i]))
			do_quotes = true;
		switch (s[i]) {
			case ':':
				if ((s.size()-i) > 2 && s[i+1] == '/' && s[i+2] == '/')
					return s;	// don't quote urls
				break;
			case '\\':
				i += 2;
				do_quotes = true;
				break;
			case '"':
				// escape the "
				s.insert(i, "\\");
				i += 2;
				do_quotes = true;
				break;
			case '/':
			case ';':
			case ',':
			case '=':
			case '-':
				do_quotes = true;
				[[fallthrough]];
			default:
				continue;
		}
	}

	return (do_quotes ? std::string("\"").append(s).append("\"") : s);
}

inline void print_quotable(std::ostream& os, const id_type& s) {
			os << util::esc_and_quote(s);
}

void print_object (std::ostream& os, const predicate& p) {
	print_quotable(os, p.object);
	if (!p.modifier.empty()) {
		os << ' ' << p.op_c_str() << ' ';
		print_quotable(os, p.modifier);
	} 
}

std::ostream& operator<<(std::ostream& os, const predicate& p) {
	os << p.relator << ' ';
	print_object(os, p);
	return os;
}

// note it is a tag friend function, not abstract_tag::operator<<
std::ostream& operator<<(std::ostream& os, const abstract_tag& t) {
	if (!t.super_object().empty() && t.pos() != POS_URL) {  // urls' sub determined by nature of being a url
		print_quotable(os, t.id());
		os << ' ';
		print_quotable(os, t.sub_relator());
		os << ' ';
		print_quotable(os, t.super_object());
	} else {
		os << t.id();
	}

	predicate_set::const_iterator it = t.relations.begin();
	if (it == t.relations.end())
		return os;

	if (t.relations.size() == 1 && t.super_object().empty()) {
		os << ' ';
		print_quotable(os, it->relator);
		os << ' ';
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
			os << std::endl;
			if (it->relator.empty())
				os << '*';
			else
				print_quotable(os, it->relator);
			os << ' ';
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
	char *s = util::csprintf(fmt, args);
	va_end (args);

	return s;
}

static const size_t CSPRINTF_MAX_SZ = 255;
static char CSPRINTF_BUF[CSPRINTF_MAX_SZ+1];
char* util::csprintf(const char *fmt, va_list& args) {
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

const id_type& error::message() const {
	for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->object == HARD_TAG_MESSAGE)
			return it->modifier;
	}

	return EMPTY_ID;
}

error error::ferror(tagd::code c, const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	char *msg = util::csprintf(errfmt, args);
	error e(c, (msg == NULL ? "error::ferror() failed" : msg));
	va_end (args);

	return e;
}

const tagd::error& errorable::last_error() const {
	const static tagd::error empty_error;

	if (_errors == nullptr || _errors.get()->size() == 0)
		return empty_error;
	else
		return (*_errors.get())[_errors.get()->size()-1];
}

tagd::code errorable::last_error_relation(predicate p) {
	if (_errors == nullptr || _errors.get()->size() == 0)
		return tagd::TS_NOT_FOUND;

	return (*_errors.get())[_errors.get()->size()-1].relation(p);
}

tagd::code errorable::most_severe(tagd::code c) {
	tagd::code most_severe = c;
	if (_errors != nullptr) {
		for (auto e : *_errors.get()) {
			if (e.code() > most_severe)
				most_severe = e.code();
		}
	}
	return most_severe;
}

errorable& errorable::copy_errors(const errorable &E) {
	if (E._errors != nullptr) {
		this->init_errors();
		_errors.get()->insert(_errors.get()->end(), E._errors.get()->begin(), E._errors.get()->end());
	}
	return *this;
}

errorable& errorable::share_errors(errorable &E) {
	this->init_errors();  // this->_errors can't be nullptr

	if (E._errors == this->_errors)
		return *this;

	this->copy_errors(E);
	E._errors = this->_errors;

	return *this;
}

void errorable::clear_errors() {
	_code = _init;
	if (_errors != nullptr)
		_errors.get()->clear();
}

const errors_t& errorable::errors() const {
	const static errors_t empty_errors;
	if (_errors == nullptr)
		return empty_errors;
	return *_errors.get();
}

tagd::code errorable::error(const tagd::error& err) {
	if (!report_errors) return err.code();

	this->init_errors();
	_code = err.code();
	_errors.get()->push_back(err);
	return _code;
}

tagd::code errorable::error(tagd::code c, const predicate& p) {
	if (!report_errors) return c;

	tagd::error err(c);
	err.relation(p);
	return this->error(err);
}

tagd::code errorable::error(tagd::code c, const std::string& s) {
	if (!report_errors) return c;

	tagd::error err(c, s);
	return this->error(err);
}

tagd::code errorable::ferror(tagd::code c, const char *errfmt, ...) {
	if (!report_errors) return c;

	va_list args;
	va_start (args, errfmt);
	tagd::code tc = this->verror(c, errfmt, args);
	va_end (args);

	return tc;
}

tagd::code errorable::verror(tagd::code c, const char *errfmt, va_list& args) {
	if (!report_errors) return c;

	_code = c;
	char *msg = util::csprintf(errfmt, args);
	return this->error(c, (msg == NULL ? "errorable::error() failed" : msg));
}

void errorable::print_errors(std::ostream& os) const {
	if (_errors == nullptr)
		return;

	errors_t::const_iterator it = _errors.get()->begin();
	if (it != _errors.get()->end()) {
		os << *it << std::endl;
		++it;
	}

	for(; it != _errors.get()->end(); ++it)
		os << std::endl << *it << std::endl;
}

// code strings
const char* code_str(tagd::code c) {
	switch (c) {
// case statement for each tagd::code generated by gen-codes-inc.pl
#include "tagd-codes.inc"
		default: assert(0); return "STR_EMPTY";
	}
}

const char* pos_str(tagd::part_of_speech p) {
	switch (p) {
// case statement for each tagd::part_of_speech generated by gen-codes-inc.pl
#include "part-of-speech.inc"
		default: assert(0); return "STR_EMPTY";
	}
}

std::string pos_list_str(tagd::part_of_speech p) {
	if (p == tagd::POS_UNKNOWN)
		return "POS_UNKNOWN";

	std::string s;
	int i = 0;
	tagd::part_of_speech pos = (tagd::part_of_speech)(1 << i);
	if ((p & pos) == pos)
		s.append( pos_str(pos) );
	while ((pos=(tagd::part_of_speech)(1<<(++i))) < tagd::POS_END) {
		if ((p & pos) == pos) {
			if (s.size() > 0)
				s.append(",");
			s.append(pos_str(pos));
		}
	}

	return s;
}

} // namespace tagd
