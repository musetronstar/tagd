#pragma once

/*\
|*| hard coded tag ids in the sense that
|*| they are top-level TAGL identifiers
|*|
|*|	ALWAYS use HARD_TAG_* definitions instead of 
|*| embedding "_hard_tag" into your code!  If we
|*| ever need to change them, we want the compiler
|*| to fail instead of introducing silent logic errors.
|*| 
|*| Defines and comments are combined into gperf statements. 
|*| They should be in the form:
|*| #define HARD_TAG_TAGNAME "<_tagname>" //gperf <SUPER_RELATION>, tagd::part_of_speech
\*/

///// root _entity (only axiomatic entity that exists) /////
#define HARD_TAG_ENTITY		"_entity"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_MESSAGE	"_message"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG

///// relators - relates subject to object, superordinate to all relators /////
#define HARD_TAG_RELATOR	"_relator"	//gperf HARD_TAG_ENTITY, tagd::POS_RELATOR
#define HARD_TAG_HAS 		"_has"		//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR
#define HARD_TAG_CAN 		"_can"		//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR

///// super relators - relations between entities to their parent /////
#define HARD_TAG_SUPER		"_super"	//gperf HARD_TAG_ENTITY, tagd::POS_SUPER_RELATOR
#define HARD_TAG_IS_A		"_is_a"		//gperf HARD_TAG_SUPER, tagd::POS_SUPER_RELATOR
#define HARD_TAG_TYPE_OF	"_type_of"	//gperf HARD_TAG_SUPER, tagd::POS_SUPER_RELATOR

///// interrogators /////
// resolves objects of inquiry (queries, searches)
#define HARD_TAG_INTERROGATOR	"_interrogator"	//gperf HARD_TAG_ENTITY, tagd::POS_INTERROGATOR
#define HARD_TAG_WHAT			"_what"			//gperf HARD_TAG_INTERROGATOR, tagd::POS_INTERROGATOR
#define HARD_TAG_SEARCH			"_search"		//gperf HARD_TAG_INTERROGATOR, tagd::POS_INTERROGATOR
#define HARD_TAG_TERMS			"_terms"		//gperf HARD_TAG_MESSAGE, tagd::POS_TAG

///// referents /////
// super relation for token that refer to a tag in a context
#define HARD_TAG_REFERENT	"_referent"		//gperf HARD_TAG_SUPER, tagd::POS_REFERENT
// relation that refers a refers_to to a tag
#define HARD_TAG_REFERS		"_refers"		//gperf HARD_TAG_RELATOR, tagd::POS_REFERS
// relation that refers_to a tag
#define HARD_TAG_REFERS_TO	"_refers_to"	//gperf HARD_TAG_RELATOR, tagd::POS_REFERS_TO
// relation that defines the context of a _referent
#define HARD_TAG_CONTEXT	"_context"		//gperf HARD_TAG_RELATOR, tagd::POS_CONTEXT

///// flags /////
#define HARD_TAG_FLAG		"_flag"			//gperf HARD_TAG_ENTITY, tagd::POS_FLAG
// ignore TS_DUPLICATE errors
#define HARD_TAG_IGNORE_DUPLICATES	"_ignore_duplicates"	//gperf HARD_TAG_FLAG, tagd::POS_FLAG

///// errors /////
#define HARD_TAG_ERROR			"_error"		//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_CAUSED_BY		"_caused_by"	//gperf HARD_TAG_RELATOR, tagd::POS_RELATOR
#define HARD_TAG_UNKNOWN_TAG	"_unknown_tag"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_BAD_TOKEN		"_bad_token"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
#define HARD_TAG_LINE_NUMBER	"_line_number"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG

///// URIs /////
// TODO #define HARD_TAG_URI "_uri"   // resources located by URIs
// resources located by URLs
#define HARD_TAG_URL		"_url"		//gperf HARD_TAG_ENTITY, tagd::POS_TAG
// URL part hard tags
// the super tag of url part hard tags
#define HARD_TAG_URL_PART	"_url_part"	//gperf HARD_TAG_ENTITY, tagd::POS_TAG
// the naming convention is that of an hduri
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
#define HARD_TAG_HOST		"_host"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
// host split into its parts
// private label of a registered tld (i.e. the "hypermega" in hypermega.com)
#define HARD_TAG_PRIV_LABEL	"_private"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG

///// labels of a tld /////
#define HARD_TAG_PUB		"_pub"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
// labels of a subdomain
#define HARD_TAG_SUB		"_sub"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PATH		"_path"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_QUERY		"_query"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_FRAGMENT	"_fragment"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PORT		"_port"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_USER		"_user"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_PASS		"_pass"		//gperf HARD_TAG_URL_PART, tagd::POS_TAG
#define HARD_TAG_SCHEME		"_scheme"	//gperf HARD_TAG_URL_PART, tagd::POS_TAG
