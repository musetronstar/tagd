#pragma once

/*
    hard coded tag ids in the sense that
    they are top-level TAGL identifiers

	ALWAYS use HARD_TAG_* definitions instead of 
	embedding "_hard_tag" into your code!  If we
	ever need to change them, we want the compiler
	to fail instead of introducing silent logic errors.

IMPORTANT: updates here must be reflected in tagspace::init_hard_tags()
*/
/*** superordinate relation semantics ***/
// hard coded tags based on how they function

#define HARD_TAG_ENTITY "_entity"   // root _entity (only axiomatic entity that exists)

// topmost identifiers (as in the property of identity)
#define HARD_TAG_SUPER "_super"     // relation between entities to their parent
#define HARD_TAG_IS_A "_is_a"       // relation between child things to their parent
#define HARD_TAG_TYPE_OF "_type_of" // relation between child non-things (verbs) to their parent

// relators
#define HARD_TAG_RELATOR "_relator" // relates subject to object, superordinate to all relators
#define HARD_TAG_HAS "_has"
#define HARD_TAG_CAN "_can"

// interrogators
#define HARD_TAG_INTERROGATOR "_interrogator"  // resolves objects of inquiry (queries, searches)

// referents
#define HARD_TAG_REFERENT  "_referent"	// super relation for token that refer to a tag in a context
#define HARD_TAG_REFERS    "_refers"	// relation that refers a refers_to to a tag
#define HARD_TAG_REFERS_TO "_refers_to"	// relation that refers_to a tag
#define HARD_TAG_CONTEXT   "_context"	// relation that defines the context of a _referent

// errors
#define HARD_TAG_ERROR "_error"
#define HARD_TAG_MESSAGE "_message"
#define HARD_TAG_CAUSED_BY "_caused_by"
#define HARD_TAG_UNKNOWN_TAG "_unknown_tag"
#define HARD_TAG_BAD_TOKEN "_bad_token"

// URIs
// TODO #define HARD_TAG_URL "_uri"   // resources located by URIs
#define HARD_TAG_URL "_url"         // resources located by URLs

// URL part hard tags
#define HARD_TAG_URL_PART "_url_part"	// the super tag of url part hard tags
// the naming convention is that of an hduri
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
#define HARD_TAG_HOST "_host"
// host split into its parts
#define HARD_TAG_PRIV_LABEL "_private"  // private label of a registered tld (i.e. the "hypermega" in hypermega.com)

#define HARD_TAG_PUB "_pub"				// labels of a tld
#define HARD_TAG_SUB "_sub"				// labels of a subdomain
// TODO - not sure if these will be needed, as we will do reverse label matches on the hduri, not its parts (subject to change)
// #define HARD_TAG_RPUB "_rpub"			// reversed labels of a tld
// #define HARD_TAG_RSUB "_rsub"			// reversed labels of a subdomain

#define HARD_TAG_PATH "_path"
#define HARD_TAG_QUERY "_query"
#define HARD_TAG_FRAGMENT "_fragment"
#define HARD_TAG_PORT "_port"
#define HARD_TAG_USER "_user"
#define HARD_TAG_PASS "_pass"
#define HARD_TAG_SCHEME "_scheme"
// -- URL hard tags
