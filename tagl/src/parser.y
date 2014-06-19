%include
{
	#include <cassert>
	#include <iostream>
	#include "tagd.h"
	#include "tagl.h"  // includes parser.h
}

%extra_argument { TAGL::driver *tagl }
%token_type {std::string *}
%token_destructor { delete $$; }

%parse_accept
{
	if (!tagl->has_error()) {
		tagl->code(tagd::TAGD_OK);
	}
}

%syntax_error
{
	switch(yymajor) { // token that caused error
		case UNKNOWN:
			if (TOKEN != NULL)
				tagl->error(tagd::TS_NOT_FOUND, tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *TOKEN));
			else
				tagl->error(tagd::TAGL_ERR, tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
			break;
		default:
			if (TOKEN != NULL)
				tagl->ferror(tagd::TAGL_ERR, "parse error near: %s", TOKEN->c_str());
			else
				tagl->error(tagd::TAGL_ERR, tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, yyTokenName[yymajor]));
	}
	if (!tagl->filename().empty()) {
		tagl->last_error_relation(
			tagd::make_predicate(HARD_TAG_CAUSED_BY, "_file", tagl->filename()) );
	}
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}

	if (tagl->is_trace_on()) {
		std::cerr << "syntax_error stack: " << std::endl;
		fprintf(stderr,"yymajor: %s\n",yyTokenName[yymajor]);
		if( yypParser->yyidx>0 ){
			int i;
			for(i=1; i<=yypParser->yyidx; i++)
				fprintf(stderr," %s",yyTokenName[yypParser->yystack[i].major]);
			fprintf(stderr,"\n");
		}
	}

	tagl->do_callback();
}

start ::= statement_list .

statement_list ::= statement_list statement .
statement_list ::= statement .

statement ::= set_statement TERMINATOR .
{
	tagl->_cmd = CMD_SET;
	if (!tagl->has_error()) {
		tagl->code(tagd::TAGD_OK);
	}
}
statement ::= get_statement TERMINATOR .
{
	tagl->_cmd = CMD_GET;
	if (!tagl->has_error()) 
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= put_statement TERMINATOR .
{
	tagl->_cmd = CMD_PUT;
	if (!tagl->has_error()) 
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= del_statement TERMINATOR .
{
	tagl->_cmd = CMD_DEL;
	if (!tagl->has_error()) 
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= query_statement TERMINATOR .
{
	tagl->_cmd = CMD_QUERY;
	if (!tagl->has_error()) 
		tagl->code(tagd::TAGD_OK);
	tagl->do_callback();
}
statement ::= TERMINATOR .

%type context { std::string * }
set_statement ::= CMD_SET set_context .
set_statement ::= CMD_SET set_flag .
%type boolean_value { bool }
set_flag ::= FLAG(F) boolean_value(b) .
{
	// TODO hard tag flags will need to have a flag_value relation holding
	// the value of the tagspace::flag_t
	if (*F == HARD_TAG_IGNORE_DUPLICATES) {
		if (b) {
			tagl->_flags |= tagspace::F_IGNORE_DUPLICATES;
		} else {
			tagl->_flags &= ~(tagspace::F_IGNORE_DUPLICATES);
		}
	} else {
		tagl->ferror(tagd::TAGL_ERR, "bad flag: %s", F->c_str());
	}
}
boolean_value(b) ::= QUANTIFIER(Q) .
{
	b = (*Q != "0");
}

set_context ::= set_context context_list .
set_context ::= set_context EMPTY_STR .
set_context ::= CONTEXT .
{
	// clear context before pushing, or errors occur if not empty
	tagl->_TS->clear_context();
}
context_list ::= context_list COMMA push_context .
context_list ::= push_context .
push_context ::= context(c) .
{
	tagl->_TS->push_context(*c);
}

get_statement ::= CMD_GET subject .
get_statement ::= CMD_GET unknown .
/*
// We can't set TS_NOT_FOUND as an error here
// because lookup_pos will return UNKNOWN for
// out of context referents, whereas tagspace::get()
// will return TS_AMBIGUOUS, so we have to set
// the tag so it makes it to tagspace::get via the callback
// TODO have lookup_pos return REFERENT for out of context referents
get_statement ::= CMD_GET UNKNOWN(U) .
{
	tagl->error(tagd::TS_NOT_FOUND,
		tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *U));
}
*/
get_statement ::= CMD_GET REFERS(R) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*R);
}

put_statement ::= CMD_PUT subject_super_relation relations .
put_statement ::= CMD_PUT subject_super_relation .
put_statement ::= CMD_PUT subject relations .
put_statement ::= CMD_PUT referent_relation .

del_statement ::= CMD_DEL subject .
get_statement ::= CMD_DEL UNKNOWN(U) .
{
	tagl->error(tagd::TS_NOT_FOUND,
		tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, *U));
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}
}
del_statement ::= CMD_DEL del_subject_super_err .
{
	tagl->ferror(tagd::TS_MISUSE,
		"super must not be specified when deleting tag: %s", tagl->_tag->id().c_str());
	if (tagl->line_number()) {
		tagl->last_error_relation(
			tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, std::to_string(tagl->line_number())) );
	}
}
del_statement ::= CMD_DEL subject relations .
del_statement ::= CMD_DEL referent_relation .

del_subject_super_err ::= subject_super_relation .
del_subject_super_err ::= subject_super_relation relations .

query_statement ::= interrogator_query .
query_statement ::= search_query .

interrogator_query ::= CMD_QUERY interrogator_super_relation relations .
interrogator_query ::= CMD_QUERY interrogator_super_relation .
interrogator_query ::= CMD_QUERY interrogator relations .
interrogator_query ::= CMD_QUERY interrogator super_relator REFERENT(R) query_referent_relations .
{
	tagl->_tag->super_object(*R);
}
interrogator_query ::= CMD_QUERY interrogator query_referent_relations .
{
	tagl->_tag->super_object(HARD_TAG_REFERENT);
}

search_query ::= CMD_QUERY QUOTED_STR(S) search_query_list .
{
	// use quoted str in production so we can reduce here
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::interrogator(HARD_TAG_SEARCH);
	tagl->_tag->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *S);
}

search_query_list ::= search_query_list COMMA QUOTED_STR(S) .
{
	tagl->_tag->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *S);
}
search_query_list ::= QUOTED_STR(S) .
{
	tagl->_tag->relation(HARD_TAG_HAS, HARD_TAG_TERMS, *S);
}
search_query_list ::= .


interrogator_super_relation ::= interrogator super_relator super_object .

interrogator ::= INTERROGATOR(I) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::interrogator(*I);
}

subject_super_relation ::= subject super_relator super_object .
subject_super_relation ::= unknown super_relator super_object .

subject ::= TAG(T) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::tag(*T);
}
subject ::= SUPER_RELATOR(S) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::tag(*S);
}
subject ::= RELATOR(R) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::relator(*R);
}
subject ::= INTERROGATOR(I) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::interrogator(*I);
}
subject ::= URL(U) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::url(*U);
}
subject ::= REFERENT(R) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*R);
}
subject ::= REFERS_TO(R) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*R);
}
subject ::= CONTEXT(R) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*R);
}
subject ::= FLAG(F) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*F);
}

unknown ::= UNKNOWN(U) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*U);
}


%type refers { std::string * }
%type refers_to { std::string * }
referent_relation ::= refers(r) REFERS_TO refers_to(rt) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::referent(*r, *rt);
}
referent_relation ::= refers(r) REFERS_TO refers_to(rt) CONTEXT context(c) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::referent(*r, *rt, *c);
}

query_referent_relations ::= query_referent_relations query_referent_relation .
query_referent_relations ::= query_referent_relation .

query_referent_relation ::= REFERS refers(r) .
{
	tagl->_tag->relation(HARD_TAG_REFERS, *r);
}
query_referent_relation ::= REFERS_TO refers_to(rt) .
{
	tagl->_tag->relation(HARD_TAG_REFERS_TO, *rt);
}
query_referent_relation ::= CONTEXT context(c) .
{
	tagl->_tag->relation(HARD_TAG_CONTEXT, *c);
}

refers(r) ::= TAG(T) .
{
	r = T;
}
refers(r) ::= SUPER_RELATOR(S) .
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
// not sure about urls as referents
// refers(r) ::= URL(U) .
// {
// 	r = U;
// }
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
refers_to(rt) ::= SUPER_RELATOR(S) .
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

// refers_to(rt) ::= URL(U) .
// {
// 	rt = U;
// }

// TAG seems the only sensible context, but we might allow others if they make sense
context(c) ::= TAG(C) .
{
	c = C;
}

super_relator ::= SUPER_RELATOR(S) .
{
	tagl->_tag->super_relator(*S);
}

super_object ::=  TAG(T) .
{
	tagl->_tag->super_object(*T);
}
super_object ::=  SUPER_RELATOR(S) .
{
	tagl->_tag->super_object(*S);
}
super_object ::=  RELATOR(R) .
{
	tagl->_tag->super_object(*R);
}
super_object ::=  INTERROGATOR(I) .
{
	tagl->_tag->super_object(*I);
}
super_object ::=  REFERENT(R) .
{
	tagl->_tag->super_object(*R);
}



/* URL are concretes.
   They can't be a super, unless we devise a way for 
   urls to be subordinate to domains or suburls */

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
}
object ::= TAG(T) op(o) MODIFIER(M) .
{
	tagl->_tag->relation(tagl->_relator, *T, *M, o);
}
object ::= TAG(T) op(o) QUOTED_STR(Q) .
{
	tagl->_tag->relation(tagl->_relator, *T, *Q, o);
}
object ::= TAG(T) op(o) URL(U) .
{
	tagl->_tag->relation(tagl->_relator, *T, *U, o);
}
object ::= TAG(T) .
{
	tagl->_tag->relation(tagl->_relator, *T);
}
object ::= URL(U) .
{
	tagd::url u(*U);
	if (u.code() == tagd::TAGD_OK) {
		tagl->_tag->relation(tagl->_relator, u.hduri());
	} else {
		tagl->ferror(u.code(), "bad url: %s", U->c_str());
	}
}
object ::= REFERENT(R) .
{
	tagl->_tag->relation(tagl->_relator, *R);
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


