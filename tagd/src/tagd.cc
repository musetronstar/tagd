#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

#include "tagd.h"
#include "tagd/ulid.h"

namespace tagd {

static std::string utc_now_ms() {
	using namespace std::chrono;
	auto now = system_clock::now();
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
	auto t = system_clock::to_time_t(now);
	std::tm tm;
	gmtime_r(&t, &tm);

	std::stringstream ss;
	ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
	   << '.' << std::setw(3) << std::setfill('0') << ms.count() << 'Z';
	return ss.str();
}

static session_factory& default_session_factory() {
	static session_factory factory;
	return factory;
}

session::session() :
	session(default_session_factory().create())
{}

session::session(const id_string& id, const id_string& started_at) :
	errorable(),
	_id{id},
	_started_at{started_at},
	_sequence{0}
{}

class session_factory::impl {
	public:
		// One generator per factory preserves same-millisecond ULID monotonicity.
		std::mutex lock;
		ulid_generator generator;
		bool initialized = false;
};

session_factory::session_factory() :
	_impl{std::make_shared<impl>()}
{}

session session_factory::create()
{
	// TODO let applications own and pass explicit session_factory instances.
	char ulid[27];
	// Skeeto's generator is stateful; protect it when sessions are created concurrently.
	std::lock_guard<std::mutex> guard(_impl->lock);
	if (!_impl->initialized) {
		ulid_generator_init(&_impl->generator, 0);
		_impl->initialized = true;
	}
	ulid_generate(&_impl->generator, ulid);
	return session(ulid, utc_now_ms());
}

session::session(const session& s) :
	errorable(s),
	_id{s._id},
	_started_at{s._started_at},
	_sequence{s._sequence.load()}
{}

session& session::operator=(const session& s) {
	if (this != &s) {
		errorable::operator=(s);
		_id = s._id;
		_started_at = s._started_at;
		_sequence.store(s._sequence.load());
	}
	return *this;
}

bool predicate::cmp_modifier_lt(const predicate& lhs, const predicate& rhs) {
	if(lhs.modifier.empty())
		return !rhs.modifier.empty();

	if(rhs.modifier.empty())
		return false;

	// returned when modifier is not compared
	bool lt_type_opr8r = (
		   (lhs.modifier_type < rhs.modifier_type)
		|| (lhs.modifier_type == rhs.modifier_type && lhs.opr8r < rhs.opr8r)
	);

	switch(lhs.modifier_type) {
		case TYPE_INTEGER:
			switch (rhs.modifier_type) {
				case TYPE_INTEGER:
					return lhs.opr8r < rhs.opr8r ||
						( lhs.opr8r == rhs.opr8r && (
							  std::strtoll(lhs.modifier.c_str(), nullptr, 10)
							< std::strtoll(rhs.modifier.c_str(), nullptr, 10) )
							// TODO check format, limits, set err code when adding tag predicates
						);
				case TYPE_FLOAT: // upgrade cmp to double
					return lhs.opr8r < rhs.opr8r ||
						( lhs.opr8r == rhs.opr8r && (
							  std::strtold(lhs.modifier.c_str(), nullptr)
							< std::strtold(rhs.modifier.c_str(), nullptr) )
						);
				case TYPE_STRING:
					return lt_type_opr8r;
				default:
					assert(0);
					return lt_type_opr8r;
			} break;

		case TYPE_FLOAT:
			switch (rhs.modifier_type) {
				case TYPE_INTEGER:
				case TYPE_FLOAT: // cmp both as double
					return lhs.opr8r < rhs.opr8r ||
						( lhs.opr8r == rhs.opr8r && (
							  std::strtold(lhs.modifier.c_str(), nullptr)
							< std::strtold(rhs.modifier.c_str(), nullptr) )
						);
				case TYPE_STRING:
					return lt_type_opr8r;
				default:
					assert(0);
					return lt_type_opr8r;
			} break;

		case TYPE_STRING:
			return (
			   lhs.modifier_type < rhs.modifier_type ||
				(
					lhs.modifier_type == rhs.modifier_type &&
					(
						lhs.opr8r < rhs.opr8r ||
						( lhs.opr8r == rhs.opr8r
						  && lhs.modifier < rhs.modifier )
					)
				)
			);

		default:
			assert(0); // modifier_type not implemented
			return lt_type_opr8r;
	}

	assert(0);
	return false;
}

// returns whether rhs == lhs
bool predicate::cmp_modifier_eq(const predicate& rhs, const predicate& lhs) {
	switch(lhs.modifier_type) {
		case TYPE_STRING:
			return (
				   lhs.modifier_type == rhs.modifier_type
				&& lhs.opr8r == rhs.opr8r
				&& lhs.modifier == rhs.modifier
			);

		case TYPE_INTEGER:
			switch (rhs.modifier_type) {
				case TYPE_STRING:
					return false;
				case TYPE_INTEGER:
					return lhs.opr8r == rhs.opr8r && (
						   std::strtoll(lhs.modifier.c_str(), nullptr, 10)
						== std::strtoll(rhs.modifier.c_str(), nullptr, 10)
						// TODO check format, limits, set err code when adding tag predicates
					);
				case TYPE_FLOAT:
					return lhs.opr8r == rhs.opr8r && (
						   std::strtold(lhs.modifier.c_str(), nullptr)
						== std::strtold(rhs.modifier.c_str(), nullptr)
					);
				default:
					assert(0);
					return false;
			} break;

		case TYPE_FLOAT:
			switch (rhs.modifier_type) {
				case TYPE_STRING:
					return false;
				case TYPE_INTEGER:
				case TYPE_FLOAT:
					return lhs.opr8r == rhs.opr8r && (
						   std::strtold(lhs.modifier.c_str(), nullptr)
						== std::strtold(rhs.modifier.c_str(), nullptr)
					);
				default:
					assert(0);
					return false;
			} break;


		default:
			assert(0); // modifier_type not implemented
			return false;
	}

	return false;
}


const char* predicate::op_c_str() const {
	switch (opr8r) {
		case OP_GT:		return ">";
		case OP_GT_EQ:	return ">=";
		case OP_EQ:		return "=";
		case OP_LT:		return "<";
		case OP_LT_EQ:	return "<=";
		default:
			assert(false);
			return "=";
	}
}

// TODO consider comparing modifier strings numerically using GMP <https://gmplib.org>, etc.
bool predicate::operator<(const predicate& p) const {
	return (
		relator < p.relator
		|| (
			(relator == p.relator && object < p.object)
			|| (
				object == p.object
				&& cmp_modifier_lt(*this, p)
				)
			)
		);
}

bool predicate::operator==(const predicate& p) const {
	return (
	  relator == p.relator
	  && object == p.object
	  && cmp_modifier_eq(*this, p)
	);
}

bool predicate::operator!=(const predicate& p) const {
	return (!(*this == p));
}

bool predicate::empty() const {
	return (
		relator.empty()
		&& object.empty()
		&& modifier.empty()
		&& opr8r == OP_EQ
		&& modifier_type == TYPE_STRING
	);
}

void insert_predicate(predicate_set& P,
	id_view relator, id_view object, id_view modifier ) {
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
		if ( a->id() == b->id() ) { // duplicate — merge relations
			// extract/mutate/reinsert: no copy of element, no iterator invalidation hazard.
			// hint is the predecessor so reinsert is O(1); fall back to end() when a is begin().
			auto hint = (a != A.begin()) ? std::prev(a) : A.end();
			auto node = A.extract(a);  // a is now invalid
			node.value().predicates(b->relations);
			a = A.insert(hint, std::move(node));
		}
	}
}

size_t merge_tags_erase_diffs(tag_set& A, const tag_set& B) {
	assert (B.size() != 0);

	if (A.size() == 0) {
		A.insert(B.begin(), B.end());
		return A.size();
	}

	tagd::tag_set T;  // accumulates matched, merged elements for reinsertion into A
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
			// extract/mutate/reinsert into T: moves node without copying element.
			auto next_a = std::next(a);
			auto node = A.extract(a);  // a invalidated; remaining A elements intact
			node.value().predicates(b->relations);
			it = T.insert(it, std::move(node));
			a = next_a;
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

bool tag_set_equal(const tag_set& A, const tag_set& B) {
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

void abstract_tag::swap(abstract_tag& rhs) noexcept {
	using std::swap;

	swap(_id, rhs._id);
	swap(_sub_relator, rhs._sub_relator);
	swap(_super_object, rhs._super_object);
	swap(_pos, rhs._pos);
	swap(_rank, rhs._rank);
	swap(_code, rhs._code);
	swap(relations, rhs.relations);
}

void swap(abstract_tag& lhs, abstract_tag& rhs) noexcept {
	lhs.swap(rhs);
}

abstract_tag& abstract_tag::operator=(const abstract_tag& rhs) {
	if (this == &rhs)
		return *this;

	// copy-then-swap for whole-object replacement via the same noexcept exchange seam
	abstract_tag copied(rhs);
	swap(copied);
	return *this;
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

tagd::code abstract_tag::relation(id_view relator, id_view object) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	id_view relator, id_view object, id_view modifier ) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object, modifier));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	id_view relator, id_view object, id_view modifier, operator_t op ) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	predicate_pair pr = relations.insert(predicate(relator, object, modifier, op));
	return (pr.second ? TAGD_OK : TAG_DUPLICATE);
}

tagd::code abstract_tag::relation(
	id_view relator, id_view object, id_view modifier, operator_t op, data_t d) {
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

tagd::code abstract_tag::not_relation(id_view relator, id_view object) {
	if (relator.empty() && object.empty())
		return TAG_ILLEGAL;

	size_t erased = relations.erase(predicate(relator, object));
	return (erased ? TAGD_OK : TAG_UNKNOWN);
}

void abstract_tag::predicates(const predicate_set &p) {
	this->relations.insert(p.begin(), p.end());
}

bool abstract_tag::has_relator(id_view r) const {
	if (r.empty())
		return false;

	for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == r) {
			return true;
		}
	}
   
	return false;
}

bool abstract_tag::has_relator(id_view r, predicate_set& P) const {
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

bool abstract_tag::related(id_view object) const {
	if (object.empty())
		return false;

	// WTF not sure if is the best way - propably more efficient methods
	for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->object == object)
			return true;
	}
   
	return false;
}

bool abstract_tag::related(id_view relator, id_view object) const {
	for (predicate_set::iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == relator && it->object == object)
			return true;
	}

	return false;
}

size_t abstract_tag::related(id_view object, predicate_set& how) const {
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
const id_string& referent::context() const {
	for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->relator == HARD_TAG_CONTEXT)
			return it->object;
	}

	return EMPTY_ID;
}

// tag output functions

// whether a label should be quoted
std::string util::esc_and_quote(id_string s) {
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

inline void print_quotable(std::ostream& os, const id_string& s) {
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
	if (t.id().find(EVURI_SCHEME) == 0 || t.id().find(ERRURI_SCHEME) == 0) {
		os << t.id();
	} else
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

	id_string last_relator;
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
thread_local static char CSPRINTF_BUF[CSPRINTF_MAX_SZ+1];
char* util::csprintf(const char *fmt, va_list& args) {
	int sz = vsnprintf(CSPRINTF_BUF, CSPRINTF_MAX_SZ, fmt, args);
	if (sz <= 0) {
		LOG_ERROR( "error vsnprintf failed: " << fmt << std::endl )
		return NULL;
	}

	if ((size_t)sz >= CSPRINTF_MAX_SZ) {
		CSPRINTF_BUF[CSPRINTF_MAX_SZ] = '\0';
		LOG_ERROR( "error truncated to " << CSPRINTF_MAX_SZ << "chars: " << CSPRINTF_BUF << std::endl )
		return NULL;
	}

	return CSPRINTF_BUF;
}

const id_string& error::message() const {
	for (predicate_set::const_iterator it = relations.begin(); it != relations.end(); ++it) {
		if (it->object == HARD_TAG_MESSAGE)
			return it->modifier;
	}

	return EMPTY_ID;
}

static session& default_error_session() {
	static session ssn(default_session_factory().create());
	return ssn;
}

error::error(const tagd::code c) :
	event(default_error_session(), "tagd", code_error_tag(c))
{
	if (_id.find(EVURI_SCHEME) == 0)
		_id.replace(0, EVURI_SCHEME.size(), ERRURI_SCHEME);
	_pos = POS_ERROR;
	_code = c;
}

error::error(const std::string& erruri) :
	event((erruri.find(ERRURI_SCHEME) == 0)
			? EVURI_SCHEME + erruri.substr(ERRURI_SCHEME.size())
			: erruri)
{
	if (erruri.find(ERRURI_SCHEME) != 0) {
		code(URI_ERR_SCHEME);
	}
	if (_id.find(EVURI_SCHEME) == 0)
		_id.replace(0, EVURI_SCHEME.size(), ERRURI_SCHEME);
	_pos = POS_ERROR;
}

error::error(const tagd::code c, const std::string& msg) :
	error(c)
{
	(void)this->relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, msg);
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

tagd::code errorable::last_error_relation(const predicate& p) {
	if (_errors == nullptr || _errors.get()->size() == 0)
		return tagd::TS_NOT_FOUND;

	return (*_errors.get())[_errors.get()->size()-1].relation(p);
}

tagd::code errorable::most_severe(tagd::code c) const {
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
	(void)err.relation(p);
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
		os << ">> " << *it << std::endl;
		++it;
	}

	for(; it != _errors.get()->end(); ++it)
		os << std::endl << ">> " << *it << std::endl;
}

// code strings
const char* code_str(tagd::code c) {
	switch (c) {
// case statement for each tagd::code generated by gen-codes-inc.pl
#include "tagd-codes.inc"
		default: assert(0); return "STR_EMPTY";
	}
}

const char* code_error_tag(tagd::code c) {
	switch (c) {
		case tagd::TAGD_ERR: return HARD_TAG_ERROR_TAGD_ERR.data();
		case tagd::TAG_UNKNOWN: return HARD_TAG_ERROR_TAG_UNKNOWN.data();
		case tagd::TAG_DUPLICATE: return HARD_TAG_ERROR_TAG_DUPLICATE.data();
		case tagd::TAG_ILLEGAL: return HARD_TAG_ERROR_TAG_ILLEGAL.data();
		case tagd::RANK_ERR: return HARD_TAG_ERROR_RANK_ERR.data();
		case tagd::RANK_EMPTY: return HARD_TAG_ERROR_RANK_EMPTY.data();
		case tagd::RANK_MAX_VALUE: return HARD_TAG_ERROR_RANK_MAX_VALUE.data();
		case tagd::RANK_MAX_LEN: return HARD_TAG_ERROR_RANK_MAX_LEN.data();
		case tagd::URI_ERR_SCHEME: return HARD_TAG_ERROR_URI_ERR_SCHEME.data();
		case tagd::URL_EMPTY: return HARD_TAG_ERROR_URL_EMPTY.data();
		case tagd::URL_MAX_LEN: return HARD_TAG_ERROR_URL_MAX_LEN.data();
		case tagd::URL_ERR_SCHEME: return HARD_TAG_ERROR_URL_ERR_SCHEME.data();
		case tagd::URL_ERR_HOST: return HARD_TAG_ERROR_URL_ERR_HOST.data();
		case tagd::URL_ERR_PORT: return HARD_TAG_ERROR_URL_ERR_PORT.data();
		case tagd::URL_ERR_PATH: return HARD_TAG_ERROR_URL_ERR_PATH.data();
		case tagd::URL_ERR_USER: return HARD_TAG_ERROR_URL_ERR_USER.data();
		case tagd::TS_NOT_FOUND: return HARD_TAG_ERROR_TS_NOT_FOUND.data();
		case tagd::TS_DUPLICATE: return HARD_TAG_ERROR_TS_DUPLICATE.data();
		case tagd::TS_SUB_UNK: return HARD_TAG_ERROR_TS_SUB_UNK.data();
		case tagd::TS_RELATOR_UNK: return HARD_TAG_ERROR_TS_RELATOR_UNK.data();
		case tagd::TS_OBJECT_UNK: return HARD_TAG_ERROR_TS_OBJECT_UNK.data();
		case tagd::TS_REFERS_TO_UNK: return HARD_TAG_ERROR_TS_REFERS_TO_UNK.data();
		case tagd::TS_CONTEXT_UNK: return HARD_TAG_ERROR_TS_CONTEXT_UNK.data();
		case tagd::TS_AMBIGUOUS: return HARD_TAG_ERROR_TS_AMBIGUOUS.data();
		case tagd::TS_RELATION_DEPENDENCY: return HARD_TAG_ERROR_TS_RELATION_DEPENDENCY.data();
		case tagd::TS_ERR_MAX_TAG_LEN: return HARD_TAG_ERROR_TS_ERR_MAX_TAG_LEN.data();
		case tagd::TS_ERR: return HARD_TAG_ERROR_TS_ERR.data();
		case tagd::TS_MISUSE: return HARD_TAG_ERROR_TS_MISUSE.data();
		case tagd::TS_INTERNAL_ERR: return HARD_TAG_ERROR_TS_INTERNAL_ERR.data();
		case tagd::TS_NOT_IMPLEMENTED: return HARD_TAG_ERROR_TS_NOT_IMPLEMENTED.data();
		case tagd::TAGL_ERR: return HARD_TAG_ERROR_TAGL_ERR.data();
		case tagd::HTTP_ERR: return HARD_TAG_ERROR_HTTP_ERR.data();
		default: return HARD_TAG_ERROR.data();
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
