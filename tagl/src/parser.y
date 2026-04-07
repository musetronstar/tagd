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

#define NEW_TAG(TAG_TYPE, TAG_ID)	\
	if (tagl->constrain_tag_id.empty() || (tagl->constrain_tag_id == TAG_ID)) {	\
		if (tagl->tag_ptr() != nullptr)	\
			tagl->delete_tag();	\
		tagl->tag_ptr(new TAG_TYPE(TAG_ID));	\
	} else {	\
		tagl->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", tagl->constrain_tag_id.c_str());	\
	}

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

void scan_tagdurl(TAGL::driver *tagl, const std::string &tagdurl ) {
	// Use a separate driver+parser so the tagdurl scanner can call parse_tok
	// without re-entering the outer parser mid-reduction.
	TAGL::driver inner(tagl->tdb(), tagl->session_ptr());
	inner.scan_tagdurl(TOK_CMD_GET, tagdurl);
	inner.finish();  // flush reductions before inspecting tag_ptr
	if (inner.has_errors()) {
		tagl->copy_errors(inner);
	} else if (inner.tag_ptr() != nullptr) {
		tagl->delete_tag();
		tagl->tag_ptr(inner.tag_ptr());
		inner.tag_ptr(nullptr);  // transfer ownership to outer driver
	}
}

} // %include


%extra_context { TAGL::driver *tagl }
%token_type {TAGL::TokenText}
%token_destructor { /* NOOP */ }
/*
 * Most TAGL nonterminals are parser-control helpers with side effects, not
 * semantic values that own memory. Use safe no-value defaults, then opt into
 * explicit types only for real data carriers such as pointers, scalars, and
 * TokenText slices.
 */
%default_type { TAGL::NoValue }
%default_destructor { /* NOOP */ }
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
			if (!TOKEN.empty())
				tagl->error(tagd::TS_NOT_FOUND, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, TOKEN.str()));
			else
				tagl->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
			break;
		default:
			if (!TOKEN.empty()) {
				const auto token = TOKEN.str();
				tagl->ferror(tagd::TAGL_ERR, "parse error near: %s", token.c_str());
			}
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
	/* TODO syntax-error cleanup still deserves a focused follow-up. The parser
	 * no longer relies on manual stack deletion here, but this path remains the
	 * place to investigate if a future destructor regression reappears.
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
%type context_object { TAGL::TokenText }
%type tagl_file { TAGL::TokenText }
%type quoted_str { TAGL::TokenText }
%type refers_subject { TAGL::TokenText }
%type refers_to_object { TAGL::TokenText }
%type sub_relator_symbol { TAGL::TokenText }
%type relator_symbol { TAGL::TokenText }
%type lhs_object { TAGL::TokenText }
%type rhs_object { TAGL::TokenText }

set_flag ::= FLAG(F) boolean_value(b) .
{
	const auto flag = F.str();
	// TODO hard tag flags will need to have a flag_value relation holding
	// the value of the tagdb::flag_t
	if (flag == HARD_TAG_IGNORE_DUPLICATES) {
		if (b) {
			tagl->flags |= tagdb::F_IGNORE_DUPLICATES;
		} else {
			tagl->flags &= ~(tagdb::F_IGNORE_DUPLICATES);
		}
	} else {
		tagl->ferror(tagd::TAGL_ERR, "bad flag: %s", flag.c_str());
	}
}
boolean_value(b) ::= QUANTIFIER(Q) .
{
	b = (Q.str() != "0");
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
	tagl->push_context(c.str());
}

context ::= CONTEXT .

set_include ::= include tagl_file(f) .
{
	tagl->include_file(f.str());
}
tagl_file(f) ::= TAGL_FILE(F) .
{
	f = F;
}
tagl_file(f) ::= QUOTED_STR(S) .
{
	f = S;
}
include ::= INCLUDE .

get_statement ::= CMD_GET subject .
get_statement ::= CMD_GET unknown .
/* not_found_context_dichotomy
// We can't set TS_NOT_FOUND as an error here
// because lookup_pos will return pos:UNKNOWN for
// out of context referents, whereas tagdb::get()
// will return tagd_code:TS_AMBIGUOUS, so we have to set
// the tag so it makes it to tagdb::get via the callback
// TODO have lookup_pos return REFERENT for out of context referents
*/
get_statement ::= CMD_GET REFERS(R) .
{
	const auto r = R.str();
	NEW_TAG(tagd::abstract_tag, r)
}
get_statement ::= CMD_GET TAGDURL(U) .
{
	scan_tagdurl(tagl, U.str());
}

put_statement ::= CMD_PUT subject_sub_relation relations .
put_statement ::= CMD_PUT subject_sub_relation .
put_statement ::= CMD_PUT subject relations .
put_statement ::= CMD_PUT referent_relation .
put_statement ::= CMD_PUT TAGDURL(U) .
{
	scan_tagdurl(tagl, U.str());
}

del_statement ::= CMD_DEL subject .
get_statement ::= CMD_DEL UNKNOWN(U) .
{
	tagl->error(tagd::TS_NOT_FOUND,
		tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, U.str()));
	last_error_add_file_line_number(tagl);
}
del_statement ::= CMD_DEL del_subject_sub_err .
{
	tagl->ferror(tagd::TS_MISUSE,
		"sub must not be specified when deleting tag: %s", tagl->tag_ptr()->id().c_str());
	last_error_add_file_line_number(tagl);
}
del_statement ::= CMD_DEL subject relations .
del_statement ::= CMD_DEL referent_relation .
del_statement ::= CMD_DEL TAGDURL(U) .
{
	scan_tagdurl(tagl, U.str());
}

del_subject_sub_err ::= subject_sub_relation .
del_subject_sub_err ::= subject_sub_relation relations .

query_statement ::= interrogator_query .
query_statement ::= search_query .
query_statement ::= tagdurl_query .

interrogator_query ::= CMD_QUERY interrogator_sub_relation relations .
interrogator_query ::= CMD_QUERY interrogator_sub_relation .
interrogator_query ::= CMD_QUERY interrogator relations .
interrogator_query ::= CMD_QUERY interrogator sub_relator REFERENT(R) query_referent_relations .
{
	tagl->tag_ptr()->super_object(R.str());
}
interrogator_query ::= CMD_QUERY interrogator query_referent_relations .
{
	tagl->tag_ptr()->super_object(HARD_TAG_REFERENT);
}

search_query ::= CMD_QUERY search_query_list .
{
}

tagdurl_query ::= CMD_QUERY TAGDURL(U) .
{
	scan_tagdurl(tagl, U.str());
}

search_query_list ::= search_query_list COMMA search_query_quoted_str .
search_query_list ::= search_query_quoted_str .
search_query_list ::= .

search_query_quoted_str ::= quoted_str(s) .
{
	NEW_TAG(tagd::interrogator, HARD_TAG_SEARCH)
	(void)tagl->tag_ptr()->relation(HARD_TAG_HAS, HARD_TAG_TERMS, s.str());
}

quoted_str(s) ::= QUOTED_STR(S) .
{ s = S; }

interrogator_sub_relation ::= interrogator sub_relator super_object .
{
}

interrogator ::= INTERROGATOR(I) .
{
	const auto i = I.str();
	NEW_TAG(tagd::interrogator, i)
}

interrogator ::= .
{
	NEW_TAG(tagd::interrogator, HARD_TAG_INTERROGATOR)
}

subject_sub_relation ::= subject sub_relator super_object .
subject_sub_relation ::= unknown sub_relator super_object .

subject ::= TAG(T) .
{
	const auto t = T.str();
	NEW_TAG(tagd::tag, t);
}
subject ::= SUB_RELATOR(S) .
{
	const auto s = S.str();
	NEW_TAG(tagd::tag, s);
}
subject ::= RELATOR(R) .
{
	const auto r = R.str();
	NEW_TAG(tagd::relator, r);
}
subject ::= INTERROGATOR(I) .
{
	const auto i = I.str();
	NEW_TAG(tagd::interrogator, i);
}
subject ::= URL(U) .
{
	tagl->new_url(U.str());
}
subject ::= HDURI(U) .
{
	tagl->new_url(U.str());
}
subject ::= REFERENT(R) .
{
	const auto r = R.str();
	NEW_TAG(tagd::abstract_tag, r);
}
subject ::= REFERS_TO(R) .
{
	const auto r = R.str();
	NEW_TAG(tagd::abstract_tag, r);
}
subject ::= CONTEXT(R) .
{
	const auto r = R.str();
	NEW_TAG(tagd::abstract_tag, r);
}
subject ::= FLAG(F) .
{
	const auto f = F.str();
	NEW_TAG(tagd::abstract_tag, f);
}
unknown ::= UNKNOWN(U) .
{
	const auto u = U.str();
	NEW_TAG(tagd::abstract_tag, u);
}


referent_relation ::= refers_subject(r) refers_to refers_to_object(rto) context context_object(co) .
{
	// WTF not sure why gcc freaks calling *c a pointer type and not the others
	NEW_REFERENT(r.str(), rto.str(), co.str())
}

refers_to ::= REFERS_TO .

query_referent_relations ::= query_referent_relations query_referent_relation .
query_referent_relations ::= query_referent_relation .

query_referent_relation ::= REFERS refers_subject(r) .
{
	(void)tagl->tag_ptr()->relation(HARD_TAG_REFERS, r.str());
}
query_referent_relation ::= refers_to refers_to_object(rto) .
{
	(void)tagl->tag_ptr()->relation(HARD_TAG_REFERS_TO, rto.str());
}
query_referent_relation ::= context context_object(co) .
{
	(void)tagl->tag_ptr()->relation(HARD_TAG_CONTEXT, co.str());
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

sub_relator ::= sub_relator_symbol(S) .
{
	tagl->tag_ptr()->sub_relator(S.str());
}

sub_relator_symbol(s) ::= SUB_RELATOR(S) .
{
	s = S; // actual sub relator tag
}
sub_relator_symbol(s) ::= SUB_RELATOR_SYMBOL(S) .
{
	s = S; // hard tag substituted for  `-^` symbol
}

super_object ::= TAG(T) .
{
	tagl->tag_ptr()->super_object(T.str());
}
super_object ::= SUB_RELATOR(S) .
{
	tagl->tag_ptr()->super_object(S.str());
}
super_object ::= RELATOR(R) .
{
	tagl->tag_ptr()->super_object(R.str());
}
super_object ::= INTERROGATOR(I) .
{
	tagl->tag_ptr()->super_object(I.str());
}
super_object ::= REFERENT(R) .
{
	tagl->tag_ptr()->super_object(R.str());
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

relator ::= relator_symbol(R) .
{
	tagl->relator = R.str();
}
relator ::= WILDCARD .
{
	tagl->relator.clear();
}

relator_symbol(r) ::= RELATOR(R) .
{
	r = R;
}
relator_symbol(r) ::= RELATOR_SYMBOL(R) .
{
	r = R;
}

object_list ::= object_list COMMA object .
object_list ::= object .

object ::= modified_object .
object ::= bare_object .

%type op { tagd::operator_t }
modified_object ::= lhs_object(l) op(o) rhs_object(r) .
{
	(void)tagl->tag_ptr()->relation(tagl->relator, l.str(), r.str(), o);
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
	(void)tagl->tag_ptr()->relation(tagl->relator, T.str());
}
bare_object ::= URL(U) .
{
	tagd::url url(U.str());
	if (url.code() == tagd::TAGD_OK) {
		(void)tagl->tag_ptr()->relation(tagl->relator, url.hduri());
	} else {
		const auto text = U.str();
		tagl->ferror(url.code(), "bad url: %s", text.c_str());
	}
}
bare_object ::= HDURI(U) .
{
	tagd::HDURI hduri(U.str());
	if (hduri.code() == tagd::TAGD_OK) {
		(void)tagl->tag_ptr()->relation(tagl->relator, hduri.hduri());
	} else {
		const auto text = U.str();
		tagl->ferror(hduri.code(), "bad hduri: %s", text.c_str());
	}
}
bare_object ::= REFERENT(R) .
{
	(void)tagl->tag_ptr()->relation(tagl->relator, R.str());
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
} // %include
