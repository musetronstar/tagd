#pragma once

/*
    hard coded tag ids in the sense that
    they are top-level TAGL identifiers

	ALWAYS use HARD_TAG_* definitions instead of 
	embedding "_hard_tag" into your code!  If we
	ever need to change them, we want the compiler
	to fail instead of introducing silent logic errors.
*/
/*** superordinate relation semantics ***/
// topmost identifiers (as in the property of identity)
#define HARD_TAG_IS_A "_is_a"       // relation between child things to their parent
#define HARD_TAG_TYPE_OF "_type_of" // relation between child non-things (verbs) to their parent

// topmost tags based on how they function
#define HARD_TAG_ENTITY "_entity"   // root _entity (only axiomatic entity that exists)
#define HARD_TAG_RELATOR "_relator" // relates subject to object, superordinate to all relators
#define HARD_TAG_URL "_url"         // resources located by urls
#define HARD_TAG_INTERROGATOR "_interrogator"  // resolves objects of inquiry (queries, searches)

// URL part hard tags
// the naming convention is that of an hduri
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
#define HARD_TAG_HOST "_host"
// host split into its parts
#define HARD_TAG_PRIV_LABEL "_priv_label"	// private label of a registered tld (i.e. the "hypermega" in hypermega.com)

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


// relators
#define HARD_TAG_HAS "_has"

