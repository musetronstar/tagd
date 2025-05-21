%include
{

#include <cassert>
#include <iostream>
#include "tagd.h"
#include "tagl.h"  // includes parser.h
#include "tagdb.h"

#define DELETE(p) \
	if (p != nullptr) { \
		TAGL_LOG_TRACE( "DELETE(" << p << "): " << *p << std::endl ) \
		delete p; \
		p = nullptr; \
	} else { \
		TAGL_LOG_TRACE( "DELETE(NULL)" << std::endl ) \
	}

// TODO
// MDELETE marks occurences of manual calls to delete
// trace each call to MDELETE, test for edge case errors
// that might cause memory leaks
// Our goal is to fix all deallocation errors
// then do away with MDELETE
#define MDELETE(p) \
		TAGL_LOG_TRACE( "MDELETE\n" ); DELETE(p)

#define NEW_TAG(TAG_TYPE, TAG_ID)	\
	if (tagl->constrain_tag_id.empty() || (tagl->constrain_tag_id == TAG_ID)) {	\
		if (tagl->tag_ptr() != nullptr)	\
			tagl->delete_tag();	\
		tagl->tag_ptr(new TAG_TYPE(TAG_ID));	\
	} else {	\
		tagl->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", tagl->constrain_tag_id.c_str());	\
	}

#define NEW_TAG_DELETE(TAG_TYPE, TAG_ID_PTR) \
	NEW_TAG(TAG_TYPE, *TAG_ID_PTR); \
	MDELETE(TAG_ID_PTR);

#define NEW_REFERENT(REFERS, REFERS_TO, CONTEXT)	\
	if (tagl->constrain_tag_id.empty() || (tagl->constrain_tag_id == REFERS)) {	\
		if (tagl->tag_ptr() != nullptr)	\
			tagl->delete_tag();	\
		tagl->tag_ptr(new tagd::referent(REFERS, REFERS_TO, CONTEXT));	\
	} else {	\
		tagl->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", tagl->constrain_tag_id.c_str());	\
	}

void last_error_add_file_line_number(TAGL::driver *tagl) {
	if (!tagl->path().empty()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, "_file", tagl->path()) );
	}
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}
}

} // %include


%extra_context { TAGL::driver *tagl }
%token_type {std::string *}
%default_destructor { DELETE($$) }
%token_prefix	TOK_

%parse_accept
{
	if (!tagl->has_errors()) {
		tagl->code(tagd::TAGD_OK);
	}
}

%stack_overflow {
	tagl->error(tagd::TAGL_ERR, "stack overflow");
	last_error_add_file_line_number(tagl);
}

%syntax_error
{
	switch(yymajor) { // token that caused error
		case TOK_UNKNOWN:
			if (TOKEN != nullptr)
				tagl->error(tagd::TS_NOT_FOUND, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *TOKEN));
			else
				tagl->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
			break;
		default:
			if (TOKEN != nullptr)
				tagl->ferror(tagd::TAGL_ERR, "parse error near: %s", TOKEN->c_str());
			else
				tagl->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
	}
	last_error_add_file_line_number(tagl);

/*
	if (TAGL_TRACE_ON) {
		fprintf(stderr, "syntax_error stack:\n");
		fprintf(stderr,"yymajor: %s\n",yyTokenName[yymajor]);
		auto p = yypParser->yystack;
		while(p++ < yypParser->yytos) {
			fprintf(stderr,"\t%s: %s\n",
				yyTokenName[p->major],
				(p->minor.yy0 ? p->minor.yy0->c_str() : "NULL")
			);
		}
	}
*/
	/* TODO
	 * we shouldn't have to manually delete the stack but
	 * %default_destructor doesn't get called on "_refers_to" in this situation:
		>> doggy _refers_to dog
		syntax_error stack:
		yymajor: TERMINATOR
			CMD_PUT: NULL
			refers: doggy
			REFERS_TO: _refers_to
			refers_to: dog
		DELETE(0x55f590735dc0): dog
		DELETE(0x55f590736660): doggy
	 */
	 /*
	for (int i=0; i<yypParser->yyidx; i++) {
	//while(p++ < yypParser->yytos) {
		auto p = &yypParser->yystack[i];
		if (p->minor.yy0 != nullptr) {
			MDELETE(p->minor.yy0);
		}
	}

	if (TOKEN != nullptr) {
		DELETE(TOKEN)
	}
	*/

	tagl->do_callback();
}

start ::= statement_list .

statement_list ::= statement_list statement .
statement_list ::= statement .

statement ::= set_statement TERMINATOR .
{
	tagl->cmd(TOK_CMD_SET);
	if (tagl->has_errors()) {
		tagl->do_callback(); // callback::cmd_error() will be called
	} else {
		tagl->code(tagd::TAGD_OK);
	}
}
statement ::= get_statement TERMINATOR .
{
	tagl->cmd(TOK_CMD_GET);
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= put_statement TERMINATOR .
{
	tagl->cmd(TOK_CMD_PUT);
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= del_statement TERMINATOR .
{
	tagl->cmd(TOK_CMD_DEL);
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= query_statement TERMINATOR .
{
	tagl->cmd(TOK_CMD_QUERY);
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= TERMINATOR .

set_statement ::= CMD_SET set_context .
set_statement ::= CMD_SET set_flag .
set_statement ::= CMD_SET set_include .

%type boolean_value { bool }
%destructor boolean_value { /* NOOP */ }
set_flag ::= FLAG(F) boolean_value(b) .
{
	// TODO hard tag flags will need to have a flag_value relation holding
	// the value of the tagdb::flag_t
	if (*F == HARD_TAG_IGNORE_DUPLICATES) {
		if (b) {
			tagl->flags |= tagdb::F_IGNORE_DUPLICATES;
		} else {
			tagl->flags &= ~(tagdb::F_IGNORE_DUPLICATES);
		}
	} else {
		tagl->ferror(tagd::TAGL_ERR, "bad flag: %s", F->c_str());
	}
	MDELETE(F)
}
boolean_value(b) ::= QUANTIFIER(Q) .
{
	b = (*Q != "0");
	MDELETE(Q)
}

set_context ::= new_context context_list .
set_context ::= new_context empty_context_list .

new_context ::= context .
{
	/* every context operation is a `set operation`
	 * so that previous contexts are overwritten
	 * TODO: determent whether set operations
	 * should be more like variable assignments
	 * and and contexts can be:
	 * - emptied
	 * - overwritten
	 * - appended
	 */
	tagl->clear_context_levels();
}
empty_context_list ::= EMPTY_STR .
{
	// NOOP already clear by previous production
}
context_list ::= context_list COMMA push_context .
context_list ::= push_context .

push_context ::= context_object(c) .
{
	tagl->push_context(*c);
	DELETE(c)
}

context(c) ::= CONTEXT(C) .
{ c = C; }

set_include ::= include tagl_file(f) .
{
	tagl->include_file(*f);
	MDELETE(f)
}
tagl_file(f) ::= TAGL_FILE(F) .
{
	f = F;
}
tagl_file(f) ::= QUOTED_STR(S) .
{
	f = S;
}
include(i) ::= INCLUDE(I) .
{ i = I; }

get_statement ::= CMD_GET subject .
get_statement ::= CMD_GET unknown .
/* not_found_context_dichotomy
// We can't set TS_NOT_FOUND as an error here
// because lookup_pos will return pos:UNKNOWN for
// out of context referents, whereas tagdb::get()
// will return tagd_code:TS_AMBIGUOUS, so we have to set
// the tag so it makes it to tagdb::get via the callback
// TODO have lookup_pos return REFERENT for out of context referents
get_statement ::= CMD_GET UNKNOWN(U) .
{
	tagl->error(tagd::TS_NOT_FOUND,
		tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *U));
	MDELETE(U)
}
*/
get_statement ::= CMD_GET REFERS(R) .
{
	NEW_TAG_DELETE(tagd::abstract_tag, R)
}

put_statement ::= CMD_PUT subject_sub_relation relations .
put_statement ::= CMD_PUT subject_sub_relation .
put_statement ::= CMD_PUT subject relations .
put_statement ::= CMD_PUT referent_relation .

del_statement ::= CMD_DEL subject .
get_statement ::= CMD_DEL UNKNOWN(U) .
{
	tagl->error(tagd::TS_NOT_FOUND,
		tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *U));
	last_error_add_file_line_number(tagl);
	MDELETE(U)
}
del_statement ::= CMD_DEL del_subject_sub_err .
{
	tagl->ferror(tagd::TS_MISUSE,
		"sub must not be specified when deleting tag: %s", tagl->tag_ptr()->id().c_str());
	last_error_add_file_line_number(tagl);
}
del_statement ::= CMD_DEL subject relations .
del_statement ::= CMD_DEL referent_relation .

del_subject_sub_err ::= subject_sub_relation .
del_subject_sub_err ::= subject_sub_relation relations .

query_statement ::= interrogator_query .
query_statement ::= search_query .

interrogator_query ::= CMD_QUERY interrogator_sub_relation relations .
interrogator_query ::= CMD_QUERY interrogator_sub_relation .
interrogator_query ::= CMD_QUERY interrogator relations .
interrogator_query ::= CMD_QUERY interrogator sub_relator REFERENT(R) query_referent_relations .
{
	tagl->tag_ptr()->super_object(*R);
	MDELETE(R)
}
interrogator_query ::= CMD_QUERY interrogator query_referent_relations .
{
	tagl->tag_ptr()->super_object(HARD_TAG_REFERENT);
}

search_query ::= CMD_QUERY search_query_list .

search_query_list ::= search_query_list COMMA search_query_quoted_str .
search_query_list ::= search_query_quoted_str .
search_query_list ::= .

search_query_quoted_str ::= quoted_str(s) .
{
	NEW_TAG(tagd::interrogator, HARD_TAG_SEARCH)
	tagl->tag_ptr()->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *s);
	MDELETE(s)
}

quoted_str(s) ::= QUOTED_STR(S) .
{ s = S; }

interrogator_sub_relation ::= interrogator sub_relator super_object .

interrogator ::= INTERROGATOR(I) .
{
	NEW_TAG(tagd::interrogator, *I)
}

interrogator ::= .
{
	NEW_TAG(tagd::interrogator, HARD_TAG_INTERROGATOR)
}

subject_sub_relation ::= subject sub_relator super_object .
subject_sub_relation ::= unknown sub_relator super_object .

subject ::= TAG(T) .
{
	NEW_TAG(tagd::tag, *T);
}
subject ::= SUB_RELATOR(S) .
{
	NEW_TAG(tagd::tag, *S);
}
subject ::= RELATOR(R) .
{
	NEW_TAG(tagd::relator, *R);
}
subject ::= INTERROGATOR(I) .
{
	NEW_TAG(tagd::interrogator, *I);
}
subject ::= URL(U) .
{
	tagl->new_url(*U);
}
subject ::= HDURI(U) .
{
	tagl->new_url(*U);
}
subject ::= REFERENT(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
}
subject ::= REFERS_TO(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
}
subject ::= CONTEXT(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
}
subject ::= FLAG(F) .
{
	NEW_TAG(tagd::abstract_tag, *F);
}

unknown ::= UNKNOWN(U) .
{
	NEW_TAG(tagd::abstract_tag, *U);
}


referent_relation ::= refers_subject(r) refers_to(rt) refers_to_object(rto) context(c) context_object(co) .
{
	// WTF not sure why gcc freaks calling *c a pointer type and not the others
	NEW_REFERENT(*r, *rto, (*co))
	DELETE(r)
	DELETE(rt)
	DELETE(rto)
	DELETE(c)
	DELETE(co)
}

refers_to(r) ::= REFERS_TO(R) .
{ r = R; }

query_referent_relations ::= query_referent_relations query_referent_relation .
query_referent_relations ::= query_referent_relation .

query_referent_relation ::= REFERS(R) refers_subject(r) .
{
	tagl->tag_ptr()->relation(HARD_TAG_REFERS, *r);
	MDELETE(R)
	MDELETE(r)
}
query_referent_relation ::= refers_to(rt) refers_to_object(rto) .
{
	tagl->tag_ptr()->relation(HARD_TAG_REFERS_TO, *rto);
	MDELETE(rt)
	MDELETE(rto)
}
query_referent_relation ::= context(c) context_object(co) .
{
	tagl->tag_ptr()->relation(HARD_TAG_CONTEXT, *co);
	MDELETE(c)
	MDELETE(co)
}

refers_subject(r) ::= TAG(T) .
{
	r = T;
}
refers_subject(r) ::= SUB_RELATOR(S) .
{
	r = S;
}
refers_subject(r) ::= RELATOR(R) .
{
	r = R;
}
refers_subject(r) ::= REFERS_TO(R) .
{
	r = R;
}
refers_subject(r) ::= INTERROGATOR(I) .
{
	r = I;
}
refers_subject(r) ::= UNKNOWN(U) .
{
	r = U;
}
refers_subject(r) ::= QUOTED_STR(Q) .
{
	r = Q;
}


refers_to_object(rt) ::= TAG(T) .
{
	rt = T;
}
refers_to_object(rt) ::= SUB_RELATOR(S) .
{
	rt = S;
}
refers_to_object(rt) ::= RELATOR(R) .
{
	rt = R;
}
refers_to_object(rt) ::= INTERROGATOR(I) .
{
	rt = I;
}
refers_to_object(rt) ::= REFERS(R) .
{
	rt = R;
}
refers_to_object(rt) ::= REFERS_TO(RT) .
{
	rt = RT;
}
refers_to_object(rt) ::= CONTEXT(C) .
{
	rt = C;
}

// TAG seems the only sensible context, but we might allow others if they make sense
context_object(c) ::= TAG(C) .
{
	c = C;
}

sub_relator ::= SUB_RELATOR(S) .
{
	tagl->tag_ptr()->sub_relator(*S);
}

super_object ::=  TAG(T) .
{
	tagl->tag_ptr()->super_object(*T);
}
super_object ::=  SUB_RELATOR(S) .
{
	tagl->tag_ptr()->super_object(*S);
}
super_object ::=  RELATOR(R) .
{
	tagl->tag_ptr()->super_object(*R);
}
super_object ::=  INTERROGATOR(I) .
{
	tagl->tag_ptr()->super_object(*I);
}
super_object ::=  REFERENT(R) .
{
	tagl->tag_ptr()->super_object(*R);
}

/*
	URL are concretes.
	They can't be a sub, unless we devise a way for
	urls to be subordinate to domains or suburls
*/

relations ::= relations predicate_list .
relations ::= predicate_list .

predicate_list ::= relator object_list .
{
	// clear the relator so we can tell between a
	// modifier following an object vs the next relator
	tagl->relator.clear();
}

relator ::= RELATOR(R) .
{
	tagl->relator = *R;
}
relator ::= WILDCARD .
{
	tagl->relator.clear();
}

object_list ::= object_list COMMA object .
object_list ::= object .

object ::= modified_object .
object ::= bare_object .

%type op { tagd::operator_t }
%destructor op { /* NOOP */ }
modified_object ::= lhs_object(l) op(o) rhs_object(r) .
{
	tagl->tag_ptr()->relation(tagl->relator, *l, *r, o);
	DELETE(l)
	DELETE(r)
}

lhs_object(o) ::= TAG(T) . 
{ o = T; }

rhs_object(o) ::= QUANTIFIER(Q) .
{ o = Q; }
rhs_object(o) ::= MODIFIER(M) . 
{ o = M; }
rhs_object(o) ::= QUOTED_STR(Q) . 
{ o = Q; }
rhs_object(o) ::= URL(U) . 
{ o = U; }
rhs_object(o) ::= HDURI(U) . 
{ o = U; }

bare_object ::= TAG(T) .
{
	tagl->tag_ptr()->relation(tagl->relator, *T);
}
bare_object ::= URL(U) .
{
	tagd::url u(*U);
	if (u.code() == tagd::TAGD_OK) {
		tagl->tag_ptr()->relation(tagl->relator, u.hduri());
	} else {
		tagl->ferror(u.code(), "bad url: %s", U->c_str());
	}
}
bare_object ::= HDURI(U) .
{
	tagd::HDURI u(*U);
	if (u.code() == tagd::TAGD_OK) {
		tagl->tag_ptr()->relation(tagl->relator, u.hduri());
	} else {
		tagl->ferror(u.code(), "bad hduri: %s", U->c_str());
	}
}

bare_object ::= REFERENT(R) .
{
	tagl->tag_ptr()->relation(tagl->relator, *R);
}


op(o) ::= EQ .
{
	o = tagd::OP_EQ;
}
op(o) ::= GT .
{
	o = tagd::OP_GT;
}
op(o) ::= GT_EQ .
{
	o = tagd::OP_GT_EQ;
}
op(o) ::= LT .
{
	o = tagd::OP_LT;
}
op(o) ::= LT_EQ .
{
	o = tagd::OP_LT_EQ;
}

%code {

/*
 * TODO when available in lemon, replace %extra_argument with %extra_context
 * add extra argument to ParseAlloc() and remove from Parse()
 */

bool TAGL_TRACE_ON = false;

void TAGL_SET_TRACE_ON() {
	TAGL_TRACE_ON = true;

#ifndef NDEBUG
	ParseTrace(stderr, (char *)"tagl_trace: ");
#endif
}

void TAGL_SET_TRACE_OFF() {
	TAGL_TRACE_ON = false;

#ifndef NDEBUG
	ParseTrace(NULL, NULL);
#endif
}

namespace TAGL {

// sets up scanner and parser, wont init if already setup
void driver::init() {
	// set _code for new parse, _errors will still contain prev errors
	if (_code != tagd::TAGD_OK)
		_code = tagd::TAGD_OK;

	if (_parser != nullptr)
		return;

    // set up parser
    _parser = ParseAlloc(::operator new, this);
    // this also works: _parser = ParseAlloc(malloc, this);
}

void driver::free_parser() {
	if (_parser != nullptr) {
		if (_token != TOK_TERMINATOR && !this->has_errors())
			Parse(_parser, TOK_TERMINATOR, NULL);
		if (_token > 0)
			Parse(_parser, 0, NULL);
		ParseFree(_parser, ::operator delete);
		// this also works: ParseFree(_parser, free);
		_parser = nullptr;
	}
}

void driver::parse_tok(int tok, std::string *s) {
		_token = tok;
		TAGL_LOG_TRACE( "line " << _scanner->_line_number
				<< ", token " << token_str(_token) << ": " << (s == nullptr ? "NULL" : *s)
				<< std::endl )

		Parse(_parser, _token, s);
}

/* parses an entire string, replace end of input with a newline
 * init() should be called before calls to parseln and
 * finish() should be called afterwards
 * empty line will result in passing a TOK_TERMINATOR token to the parser
 */
tagd::code driver::parseln(const std::string& line) {
	this->init();

	// end of input
	if (line.empty()) {
		Parse(_parser, TOK_TERMINATOR, NULL);
		_token = 0;
		Parse(_parser, _token, NULL);
		return this->code();
	}

	_scanner->scan(line);

	return this->code();
}

} // namespace TAGL
} // %include
