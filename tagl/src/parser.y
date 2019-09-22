%include
{
	#include <cassert>
	#include <iostream>
	#include "tagd.h"
	#include "tagl.h"  // includes parser.h
	#include "tagdb.h"

#define DELETE(P) delete P;

#define NEW_TAG(TAG_TYPE, TAG_ID)	\
	if (tagl->_constrain_tag_id.empty() || (tagl->_constrain_tag_id == TAG_ID)) {	\
		if (tagl->_tag != nullptr)	\
			delete tagl->_tag;	\
		tagl->_tag = new TAG_TYPE(TAG_ID);	\
	} else {	\
		tagl->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", tagl->_constrain_tag_id.c_str());	\
	}

#define NEW_REFERENT(REFERS, REFERS_TO, CONTEXT)	\
	if (tagl->_constrain_tag_id.empty() || (tagl->_constrain_tag_id == REFERS)) {	\
		if (tagl->_tag != nullptr)	\
			delete tagl->_tag;	\
		tagl->_tag = new tagd::referent(REFERS, REFERS_TO, CONTEXT);	\
	} else {	\
		tagl->ferror(tagd::TAGL_ERR, "tag id constrained as: %s", tagl->_constrain_tag_id.c_str());	\
	}
}


%extra_argument { TAGL::driver *tagl }
%token_type {std::string *}
%token_prefix	TOK_

%parse_accept
{
	if (!tagl->has_errors()) {
		tagl->code(tagd::TAGD_OK);
	}
}

%syntax_error
{
	switch(yymajor) { // token that caused error
		case TOK_UNKNOWN:
			if (TOKEN != NULL)
				tagl->error(tagd::TS_NOT_FOUND, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *TOKEN));
			else
				tagl->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
			break;
		default:
			if (TOKEN != NULL)
				tagl->ferror(tagd::TAGL_ERR, "parse error near: %s", TOKEN->c_str());
			else
				tagl->error(tagd::TAGL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
	}
	if (!tagl->path().empty()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, "_file", tagl->path()) );
	}
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}

	if (tagl->is_trace_on()) {
		std::cerr << "syntax_error stack: " << std::endl;
		fprintf(stderr,"yymajor: %s\n",yyTokenName[yymajor]);
		if( yypParser->yytos > yypParser->yystack ) {
			// loop from start of stack to top of stack
			auto p = yypParser->yystack;
			while(p++ <= yypParser->yytos)
				fprintf(stderr," %s",yyTokenName[p->major]);
			fprintf(stderr,"\n");
		}
	}

	if (TOKEN != NULL)
		DELETE(TOKEN)
	tagl->do_callback();
}

start ::= statement_list .

statement_list ::= statement_list statement .
statement_list ::= statement .

statement ::= set_statement TERMINATOR .
{
	tagl->_cmd = TOK_CMD_SET;
	if (tagl->has_errors()) {
		tagl->do_callback(); // callback::cmd_error() will be called
	} else {
		tagl->code(tagd::TAGD_OK);
	}
}
statement ::= get_statement TERMINATOR .
{
	tagl->_cmd = TOK_CMD_GET;
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= put_statement TERMINATOR .
{
	tagl->_cmd = TOK_CMD_PUT;
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= del_statement TERMINATOR .
{
	tagl->_cmd = TOK_CMD_DEL;
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= query_statement TERMINATOR .
{
	tagl->_cmd = TOK_CMD_QUERY;
	if (!tagl->has_errors())
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= TERMINATOR .


set_statement ::= CMD_SET set_context .
set_statement ::= CMD_SET set_flag .
set_statement ::= CMD_SET set_include .

%type boolean_value { bool }
set_flag ::= FLAG(F) boolean_value(b) .
{
	// TODO hard tag flags will need to have a flag_value relation holding
	// the value of the tagdb::flag_t
	if (*F == HARD_TAG_IGNORE_DUPLICATES) {
		if (b) {
			tagl->_flags |= tagdb::F_IGNORE_DUPLICATES;
		} else {
			tagl->_flags &= ~(tagdb::F_IGNORE_DUPLICATES);
		}
	} else {
		tagl->ferror(tagd::TAGL_ERR, "bad flag: %s", F->c_str());
	}
	DELETE(F)
}
boolean_value(b) ::= QUANTIFIER(Q) .
{
	b = (*Q != "0");
	DELETE(Q)
}

set_context ::= set_context context_list .
set_context ::= set_context empty_context_list .
set_context ::= CONTEXT(C) .
{
	/* ever context operation is a `set operation`
	 * so that previous contexts are overwritten
	 * TODO: determent whether set operations
	 * should be more like variable assignments
	 * and and contexts can be:
	 * - emptied
	 * - overwritten
	 * - appended
	 */
	tagl->clear_context_levels();
	DELETE(C)
}
empty_context_list ::= EMPTY_STR .
{
	// NOOP already clear by previous production
}
context_list ::= context_list COMMA push_context .
context_list ::= push_context .
push_context ::= context(c) .
{
	if (tagl->_session == nullptr)
		tagl->own_session(tagl->_tdb->new_session());
	tagl->_session->push_context(*c);
	tagl->_context_level++;
	DELETE(c)
}

set_include ::= INCLUDE(I) tagl_file(f) .
{
	tagl->include_file(*f);
	DELETE(I)
	DELETE(f)
}
tagl_file(f) ::= TAGL_FILE(F) .
{
	f = F;
}
tagl_file(f) ::= QUOTED_STR(S) .
{
	f = S;
}

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
	DELETE(U)
}
*/
get_statement ::= CMD_GET REFERS(R) .
{
	NEW_TAG(tagd::abstract_tag, *R)
	DELETE(R)
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
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}
	DELETE(U)
}
del_statement ::= CMD_DEL del_subject_sub_err .
{
	tagl->ferror(tagd::TS_MISUSE,
		"sub must not be specified when deleting tag: %s", tagl->_tag->id().c_str());
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}
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
	tagl->_tag->super_object(*R);
	DELETE(R)
}
interrogator_query ::= CMD_QUERY interrogator query_referent_relations .
{
	tagl->_tag->super_object(HARD_TAG_REFERENT);
}

search_query ::= CMD_QUERY QUOTED_STR(S) search_query_list .
{
	// use quoted str in production so we can reduce here
	NEW_TAG(tagd::interrogator, HARD_TAG_SEARCH)
	tagl->_tag->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *S);
	DELETE(S)
}

search_query_list ::= search_query_list COMMA search_query_quoted_str .
search_query_list ::= search_query_quoted_str .
search_query_list ::= .

search_query_quoted_str ::= QUOTED_STR(S) .
{
	tagl->_tag->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *S);
	DELETE(S)
}

interrogator_sub_relation ::= interrogator sub_relator super_object .

interrogator ::= INTERROGATOR(I) .
{
	NEW_TAG(tagd::interrogator, *I)
	DELETE(I)
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
	DELETE(T)
}
subject ::= SUB_RELATOR(S) .
{
	NEW_TAG(tagd::tag, *S);
	DELETE(S)
}
subject ::= RELATOR(R) .
{
	NEW_TAG(tagd::relator, *R);
	DELETE(R)
}
subject ::= INTERROGATOR(I) .
{
	NEW_TAG(tagd::interrogator, *I);
	DELETE(I)
}
subject ::= URL(U) .
{
	NEW_TAG(tagd::url, *U);
	DELETE(U)
}
subject ::= HDURI(U) .
{
	NEW_TAG(tagd::HDURI, *U);
	DELETE(U)
}
subject ::= REFERENT(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
	DELETE(R)
}
subject ::= REFERS_TO(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
	DELETE(R)
}
subject ::= CONTEXT(R) .
{
	NEW_TAG(tagd::abstract_tag, *R);
	DELETE(R)
}
subject ::= FLAG(F) .
{
	NEW_TAG(tagd::abstract_tag, *F);
	DELETE(F)
}

unknown ::= UNKNOWN(U) .
{
	NEW_TAG(tagd::abstract_tag, *U);
	DELETE(U)
}


referent_relation ::= refers(r) REFERS_TO(RT) refers_to(rt) CONTEXT(C) context(c) .
{
	// WTF not sure why gcc freaks calling *c a pointer type and not the others
	NEW_REFERENT(*r, *rt, (*c))
	DELETE(r)
	DELETE(RT)
	DELETE(rt)
	DELETE(C)
	DELETE(c)
}

query_referent_relations ::= query_referent_relations query_referent_relation .
query_referent_relations ::= query_referent_relation .

query_referent_relation ::= REFERS(R) refers(r) .
{
	tagl->_tag->relation(HARD_TAG_REFERS, *r);
	DELETE(R)
	DELETE(r)
}
query_referent_relation ::= REFERS_TO(RT) refers_to(rt) .
{
	tagl->_tag->relation(HARD_TAG_REFERS_TO, *rt);
	DELETE(RT)
	DELETE(rt)
}
query_referent_relation ::= CONTEXT(C) context(c) .
{
	tagl->_tag->relation(HARD_TAG_CONTEXT, *c);
	DELETE(C)
	DELETE(c)
}

refers(r) ::= TAG(T) .
{
	r = T;
}
refers(r) ::= SUB_RELATOR(S) .
{
	r = S;
}
refers(r) ::= RELATOR(R) .
{
	r = R;
}
refers(r) ::= REFERS_TO(R) .
{
	r = R;
}
refers(r) ::= INTERROGATOR(I) .
{
	r = I;
}
refers(r) ::= UNKNOWN(U) .
{
	r = U;
}
refers(r) ::= QUOTED_STR(Q) .
{
	r = Q;
}


refers_to(rt) ::= TAG(T) .
{
	rt = T;
}
refers_to(rt) ::= SUB_RELATOR(S) .
{
	rt = S;
}
refers_to(rt) ::= RELATOR(R) .
{
	rt = R;
}
refers_to(rt) ::= INTERROGATOR(I) .
{
	rt = I;
}
refers_to(rt) ::= REFERS(R) .
{
	rt = R;
}
refers_to(rt) ::= REFERS_TO(RT) .
{
	rt = RT;
}
refers_to(rt) ::= CONTEXT(C) .
{
	rt = C;
}

// TAG seems the only sensible context, but we might allow others if they make sense
context(c) ::= TAG(C) .
{
	c = C;
}

sub_relator ::= SUB_RELATOR(S) .
{
	tagl->_tag->sub_relator(*S);
	DELETE(S)
}

super_object ::=  TAG(T) .
{
	tagl->_tag->super_object(*T);
	DELETE(T)
}
super_object ::=  SUB_RELATOR(S) .
{
	tagl->_tag->super_object(*S);
	DELETE(S)
}
super_object ::=  RELATOR(R) .
{
	tagl->_tag->super_object(*R);
	DELETE(R)
}
super_object ::=  INTERROGATOR(I) .
{
	tagl->_tag->super_object(*I);
	DELETE(I)
}
super_object ::=  REFERENT(R) .
{
	tagl->_tag->super_object(*R);
	DELETE(R)
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
	tagl->_relator.clear();
}

relator ::= RELATOR(R) .
{
	tagl->_relator = *R;
	DELETE(R)
}

relator ::= WILDCARD .
{
	tagl->_relator.clear();
}

object_list ::= object_list COMMA object .
object_list ::= object .

%type op { tagd::operator_t }
object ::= TAG(T) op(o) QUANTIFIER(Q) .
{
	tagl->_tag->relation(tagl->_relator, *T, *Q, o);
	DELETE(T)
	DELETE(Q)
}
object ::= TAG(T) op(o) MODIFIER(M) .
{
	tagl->_tag->relation(tagl->_relator, *T, *M, o);
	DELETE(T)
	DELETE(M)
}
object ::= TAG(T) op(o) QUOTED_STR(Q) .
{
	tagl->_tag->relation(tagl->_relator, *T, *Q, o);
	DELETE(T)
	DELETE(Q)
}
object ::= TAG(T) op(o) URL(U) .
{
	tagl->_tag->relation(tagl->_relator, *T, *U, o);
	DELETE(T)
	DELETE(U)
}
object ::= TAG(T) op(o) HDURI(U) .
{
	tagl->_tag->relation(tagl->_relator, *T, *U, o);
	DELETE(T)
	DELETE(U)
}
object ::= TAG(T) .
{
	tagl->_tag->relation(tagl->_relator, *T);
	DELETE(T)
}
object ::= URL(U) .
{
	tagd::url u(*U);
	if (u.code() == tagd::TAGD_OK) {
		tagl->_tag->relation(tagl->_relator, u.hduri());
	} else {
		tagl->ferror(u.code(), "bad url: %s", U->c_str());
	}
	DELETE(U)
}
object ::= HDURI(U) .
{
	tagd::HDURI u(*U);
	if (u.code() == tagd::TAGD_OK) {
		tagl->_tag->relation(tagl->_relator, u.hduri());
	} else {
		tagl->ferror(u.code(), "bad hduri: %s", U->c_str());
	}
	DELETE(U)
}

object ::= REFERENT(R) .
{
	tagl->_tag->relation(tagl->_relator, *R);
	DELETE(R)
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


