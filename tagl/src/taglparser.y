%include
{
	#include <cassert>
	#include <iostream>
	#include "tagd.h"
	#include "tagl.h"  // includes taglparser.h

}

%extra_argument { TAGL::driver *tagl }
%token_type {std::string *}
%token_destructor { delete $$; }

%parse_accept
{
	//std::cerr << "parse_accept" << std::endl;
	if (tagl->code() != TAGL_ERR) {
		tagl->code(TAGL_OK);
		tagl->msg("parse success");
	}
}

%syntax_error
{
	if (tagl->code() != TAGL_ERR) {
		switch(yymajor) { // token that caused error
			case UNKNOWN:
				tagl->error("unknown tag", TOKEN);
				break;
			default:
				tagl->error("parse error near", TOKEN);
		}
	}
/*
	std::cerr << "syntax_error stack: " << std::endl;
	fprintf(stderr,"yymajor: %s\n",yyTokenName[yymajor]);
	if( yypParser->yyidx>0 ){
		int i;
		for(i=1; i<=yypParser->yyidx; i++)
			fprintf(stderr," %s",yyTokenName[yypParser->yystack[i].major]);
		fprintf(stderr,"\n");
	}
*/
}

start ::= statement_list .

statement_list ::= statement_list statement .
statement_list ::= statement .

// statement ::= pragma TERMINATOR .
statement ::= get_statement TERMINATOR .
{
	tagl->_cmd = CMD_GET;
	if (tagl->code() != TAGL_ERR) {
		//std::cerr << "statement_success(" << pos_str(tagl->_tag->pos()) << "): " << std::endl;
		if (tagl->_tag != NULL) {
			//std::cerr << *(tagl->_tag) << std::endl;
			tagl->code(TAGL_OK);
			tagl->msg("statement success");
			tagl->statement_end();
			delete tagl->_tag;
			tagl->_tag = NULL;
		}
		// std::cerr << std::endl;
	}
}
statement ::= put_statement TERMINATOR .
{
	tagl->_cmd = CMD_PUT;
	if (tagl->code() != TAGL_ERR) {
		//std::cerr << "statement_success(" << pos_str(tagl->_tag->pos()) << "): " << std::endl;
		if (tagl->_tag != NULL) {
			//std::cerr << *(tagl->_tag) << std::endl;
			tagl->code(TAGL_OK);
			tagl->msg("statement success");
			tagl->statement_end();
			delete tagl->_tag;
			tagl->_tag = NULL;
		}
		// std::cerr << std::endl;
	}
}
statement ::= query_statement TERMINATOR .
{
	tagl->_cmd = CMD_QUERY;
	if (tagl->code() != TAGL_ERR) {
		//std::cerr << "statement_success(" << pos_str(tagl->_tag->pos()) << "): " << std::endl;
		if (tagl->_tag != NULL) {
			//std::cerr << *(tagl->_tag) << std::endl;
			tagl->code(TAGL_OK);
			tagl->msg("statement success");
			tagl->statement_end();
			delete tagl->_tag;
			tagl->_tag = NULL;
		}
		// std::cerr << std::endl;
	}
}

statement ::= TERMINATOR .

/*
%type cmd { int }
tagl_cmd ::= cmd(c) .
{
	tagl->_cmd = c;
}
tagl_cmd ::= .
{
	tagl->_cmd = tagl->_default_cmd;
}

pragma ::= CMD ASSIGNMENT cmd(c) .
{
	tagl->_default_cmd = c;
}

cmd(c) ::= CMD_GET .
{
	c = CMD_GET;
}
cmd(c) ::= CMD_PUT .
{
	c = CMD_PUT;
}
cmd(c) ::= CMD_TEST .
{
	c = CMD_TEST;
}
*/

get_statement ::= CMD_GET subject .

put_statement ::= CMD_PUT subject_super_relation relations .
put_statement ::= CMD_PUT subject_super_relation .
put_statement ::= CMD_PUT subject relations .

query_statement ::= CMD_QUERY interrogator_super_relation relations .
query_statement ::= CMD_QUERY interrogator_super_relation .
query_statement ::= CMD_QUERY interrogator relations .

interrogator_super_relation ::= interrogator SUPER super_object .

interrogator ::= INTERROGATOR(I) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::interrogator(*I);
}

subject_super_relation ::= subject SUPER super_object .
subject_super_relation ::= unknown SUPER super_object .

subject ::= TAG(T) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::tag(*T);
}
subject ::= SUPER(S) .
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

unknown ::= UNKNOWN(U) .
{
	if (tagl->_tag != NULL)
		delete tagl->_tag;
	tagl->_tag = new tagd::abstract_tag(*U);
}

super_object ::=  TAG(T) .
{
	tagl->_tag->super(*T);
}
super_object ::=  SUPER(S) .
{
	tagl->_tag->super(*S);
}
super_object ::=  RELATOR(R) .
{
	tagl->_tag->super(*R);
}
super_object ::=  INTERROGATOR(I) .
{
	tagl->_tag->super(*I);
}
/* URL are concretes.
   They can't be a super, unless we devise a way for 
   urls to be subordinate to domains or suburls */

relations ::= relations predicate_list .
relations ::= predicate_list .

predicate_list ::= relator object_list .
{
	// clear the relator so we can tell between a
	// quantifier following an object vs the next relator
	tagl->_relator.clear();
}

relator ::= RELATOR(R) .
{
	tagl->_relator = *R;
}

object_list ::= object_list SEPARATOR object .
object_list ::= object .

object ::= TAG(T) QUANTIFIER(Q) .
{
	tagl->_tag->relation(tagl->_relator, *T, *Q);
}
object ::= TAG(T) .
{
	tagl->_tag->relation(tagl->_relator, *T);
}
