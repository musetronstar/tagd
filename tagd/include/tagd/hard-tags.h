#pragma once

/*\
|*| This file contains hard-coded tag ids (HARD_TAGs)
|*|
|*| ALWAYS use HARD_TAG_* definitions instead of
|*| embedding "_hard_tag" into your code!  If we
|*| ever need to change them, we want the compiler
|*| to fail instead of introducing silent logic errors.
|*|
|*| Defines and comments are combined into a gperf statements by gen-hard-tags.gperf.pl.
|*| They should be in the form:
|*| #define HARD_TAG_TAGNAME "<_tagname>" //gperf <SUB_RELATION>, <tagd::part_of_speech>
\*/

// root _entity (only axiomatic, self-referencing entity that exists)
#define HARD_TAG_ENTITY		"_entity"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG

// sub relators - relates tag ids to their super_object (aka. superordinate, parent, hypernym...)
#define HARD_TAG_SUB		"_sub"	//gperf HARD_TAG_ENTITY, tagd::POS_SUB_RELATOR
#define HARD_TAG_IS_A		"_is_a"		//gperf HARD_TAG_SUB, tagd::POS_SUB_RELATOR
#define HARD_TAG_TYPE_OF	"_type_of"	//gperf HARD_TAG_SUB, tagd::POS_SUB_RELATOR

// relators - relates subject to object, subordinate to all relators
#define HARD_TAG_RELATOR	"_rel"		//gperf HARD_TAG_ENTITY, tagd::POS_RELATOR
#define HARD_TAG_HAS 		"_has"		//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR
#define HARD_TAG_CAN 		"_can"		//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR

/***** interrogators *****/
// resolves objects of inquiry (queries, searches)
#define HARD_TAG_INTERROGATOR	"_interrogator"	//gperf HARD_TAG_ENTITY, tagd::POS_INTERROGATOR
#define HARD_TAG_WHAT			"_what"			//gperf HARD_TAG_INTERROGATOR, tagd::POS_INTERROGATOR
#define HARD_TAG_SEARCH			"_search"		//gperf HARD_TAG_INTERROGATOR, tagd::POS_INTERROGATOR

/***** referents *****/
// super_object for token that refers to a tag in a context
#define HARD_TAG_REFERENT	"_referent"		//gperf HARD_TAG_SUB, tagd::POS_REFERENT
// token that refers (i.e. "doggy" in the statement "doggy _refers_to dog"
#define HARD_TAG_REFERS		"_refers"		//gperf HARD_TAG_RELATOR, tagd::POS_REFERS
// token that is referred to (i.e. "dog" in the statement "doggy _refers_to dog"
#define HARD_TAG_REFERS_TO	"_refers_to"	//gperf HARD_TAG_RELATOR, tagd::POS_REFERS_TO
// relation that defines the context of a _referent
#define HARD_TAG_CONTEXT	"_context"		//gperf HARD_TAG_RELATOR, tagd::POS_CONTEXT

/***** messages *****/
#define HARD_TAG_MESSAGE	"_message"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_TERMS		"_terms"	//gperf HARD_TAG_MESSAGE, tagd::POS_TAG

/***** flags *****/
#define HARD_TAG_FLAG		"_flag"		//gperf HARD_TAG_ENTITY, tagd::POS_FLAG
// ignore TS_DUPLICATE errors
#define HARD_TAG_IGNORE_DUPLICATES	"_ignore_duplicates"	//gperf HARD_TAG_FLAG, tagd::POS_FLAG

/***** flags *****/
#define HARD_TAG_INCLUDE	"_include"		//gperf HARD_TAG_RELATOR, tagd::POS_INCLUDE

/***** errors *****/
#define HARD_TAG_ERROR			"_error"		//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_BAD_TOKEN		"_bad_token"	//gperf HARD_TAG_ERROR, tagd::POS_TAG
#define HARD_TAG_UNKNOWN_TAG	"_unknown_tag"	//gperf HARD_TAG_ERROR, tagd::POS_TAG

/***** used in errors, but not necessarily (could be used in other contexts) *****/
#define HARD_TAG_CAUSED_BY		"_caused_by"	//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR
#define HARD_TAG_LINE_NUMBER	"_line_number"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_EMPTY			"_empty"		//gperf HARD_TAG_ENTITY, tagd::POS_TAG

/***** URIs *****/
// TODO #define HARD_TAG_URI "_uri"   // resources located by URIs
// resources located by URLs
#define HARD_TAG_URL		"_url"		//gperf HARD_TAG_ENTITY, tagd::POS_TAG
/*** URL part hard tags ***/
// the sub object of url part hard tags
#define HARD_TAG_URL_PART	"_url_part"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_HOST		"_host"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
// private label of a registered tld (i.e. the "hypermega" in hypermega.com)
#define HARD_TAG_PRIV_LABEL	"_private"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PUBLIC		"_public"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_SUBDOMAIN	"_subdomain" //gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PATH		"_path"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_QUERY		"_query"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_FRAGMENT	"_fragment"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PORT		"_port"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_USER		"_user"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PASS		"_pass"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_SCHEME		"_scheme"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
