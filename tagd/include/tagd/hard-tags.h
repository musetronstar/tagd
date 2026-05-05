#pragma once

#include "tagd/codes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tagd {

/*\
|*| This file contains hard-coded tag ids (HARD_TAGs)
|*|
|*| ALWAYS use HARD_TAG_* definitions instead of
|*| embedding "_hard_tag" into your code!  If we
|*| ever need to change them, we want the compiler
|*| to fail instead of introducing silent logic errors.
|*|
|*| Hard tag ids and their hierarchy metadata are declared here for the
|*| generated hard-tag lookup tables.
|*| They should be in the form:
|*| inline constexpr std::string_view HARD_TAG_TAGNAME{"<_tagname>"}; /// <SUB_RELATION> <SUPER_OBJECT> <tagd::part_of_speech>
|*|
|*| The generator consumes lines in the form:
|*| inline constexpr std::string_view HARD_TAG_TAGNAME{"<_tagname>"}; /// <SUB_RELATION> <SUPER_OBJECT> <tagd::part_of_speech>
\*/

// root _entity (only axiomatic, self-referencing entity that exists)
inline constexpr std::string_view HARD_TAG_ENTITY{"_entity"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG

// sub relators - relates tag ids to their super_object (aka. superordinate, parent, hypernym...)
inline constexpr std::string_view HARD_TAG_SUB{"_sub"};		/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_SUB_RELATOR
// TODO HARD_TAG_IS_A
// inline constexpr std::string_view HARD_TAG_IS_A{"_is_a"};		/// HARD_TAG_SUB HARD_TAG_SUB tagd::POS_SUB_RELATOR
// TODO HARD_TAG_TYPE_OF
// inline constexpr std::string_view HARD_TAG_TYPE_OF{"_type_of"};	/// HARD_TAG_SUB HARD_TAG_SUB tagd::POS_SUB_RELATOR

// relators - relates subject to object, subordinate to all relators
inline constexpr std::string_view HARD_TAG_RELATOR{"_rel"};		/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_RELATOR
inline constexpr std::string_view HARD_TAG_HAS{"_has"};		/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_RELATOR
inline constexpr std::string_view HARD_TAG_CAN{"_can"};		/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_RELATOR

/***** primitive types *****/
// TODO: gperf does not support the sub_relator field for these entries;
// the sub_relation is set to HARD_TAG_SUB and must be handled at runtime.
inline constexpr std::string_view HARD_TAG_NUMBER{"_number"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_INTEGER{"_integer"};	/// HARD_TAG_SUB HARD_TAG_NUMBER tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_FLOAT{"_float"};	/// HARD_TAG_SUB HARD_TAG_NUMBER tagd::POS_TAG

/***** interrogators *****/
// resolves objects of inquiry (queries, searches)
inline constexpr std::string_view HARD_TAG_INTERROGATOR{"_interrogator"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_INTERROGATOR
inline constexpr std::string_view HARD_TAG_WHAT{"_what"};		/// HARD_TAG_SUB HARD_TAG_INTERROGATOR tagd::POS_INTERROGATOR
inline constexpr std::string_view HARD_TAG_SEARCH{"_search"};	/// HARD_TAG_SUB HARD_TAG_INTERROGATOR tagd::POS_INTERROGATOR

/***** referents *****/
// super_object for token that refers to a tag in a context
inline constexpr std::string_view HARD_TAG_REFERENT{"_referent"};	/// HARD_TAG_SUB HARD_TAG_SUB tagd::POS_REFERENT
// token that refers (i.e. "doggy" in the statement "doggy _refers_to dog"
inline constexpr std::string_view HARD_TAG_REFERS{"_refers"};	/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_REFERS
// token that is referred to (i.e. "dog" in the statement "doggy _refers_to dog"
inline constexpr std::string_view HARD_TAG_REFERS_TO{"_refers_to"};	/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_REFERS_TO
// relation that defines the context of a _referent
inline constexpr std::string_view HARD_TAG_CONTEXT{"_context"};	/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_CONTEXT

/***** messages *****/
inline constexpr std::string_view HARD_TAG_MESSAGE{"_message"};		/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TERMS{"_terms"};		/// HARD_TAG_SUB HARD_TAG_MESSAGE tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_EVENT{"_event"};		/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ACTION{"_action"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_LOG_EVENT{"_event:log"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_COMMAND_EVENT{"_event:command"};	/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TAGDB_EVENT{"_event:tagdb"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TAGDB_PUT_EVENT{"_event:tagdb_put"};	/// HARD_TAG_SUB HARD_TAG_TAGDB_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TAGDB_DEL_EVENT{"_event:tagdb_del"};	/// HARD_TAG_SUB HARD_TAG_TAGDB_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TAGDB_GET_EVENT{"_event:tagdb_get"};	/// HARD_TAG_SUB HARD_TAG_TAGDB_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_TAGDB_QUERY_EVENT{"_event:tagdb_query"};	/// HARD_TAG_SUB HARD_TAG_TAGDB_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_EVENT{"_event:http"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
// HTTP request/response event hierarchy
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_EVENT{"_event:http_request"};		/// HARD_TAG_SUB HARD_TAG_HTTP_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_RESPONSE_EVENT{"_event:http_response"};	/// HARD_TAG_SUB HARD_TAG_HTTP_EVENT tagd::POS_TAG
// request method events
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_GET_EVENT{"_event:http_request_get"};		/// HARD_TAG_SUB HARD_TAG_HTTP_REQUEST_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_HEAD_EVENT{"_event:http_request_head"};	/// HARD_TAG_SUB HARD_TAG_HTTP_REQUEST_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_PUT_EVENT{"_event:http_request_put"};		/// HARD_TAG_SUB HARD_TAG_HTTP_REQUEST_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_POST_EVENT{"_event:http_request_post"};	/// HARD_TAG_SUB HARD_TAG_HTTP_REQUEST_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_REQUEST_DELETE_EVENT{"_event:http_request_delete"};	/// HARD_TAG_SUB HARD_TAG_HTTP_REQUEST_EVENT tagd::POS_TAG
// response status events
inline constexpr std::string_view HARD_TAG_HTTP_RESPONSE_OK_EVENT{"_event:http_response_ok"};		/// HARD_TAG_SUB HARD_TAG_HTTP_RESPONSE_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HTTP_RESPONSE_NOT_FOUND_EVENT{"_event:http_response_not_found"};	/// HARD_TAG_SUB HARD_TAG_HTTP_RESPONSE_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_PARSE_EVENT{"_event:parse"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_SCAN_EVENT{"_event:scan"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG

/***** roles *****/
inline constexpr std::string_view HARD_TAG_ROLE{"_role"};			/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_SYSTEM{"_role:system"};		/// HARD_TAG_SUB HARD_TAG_ROLE tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_SCANNER{"_role:scanner"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_PARSER{"_role:parser"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_DRIVER{"_role:driver"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_TAGDB{"_role:tagdb"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_TAGSH{"_role:tagsh"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_HTTAGD{"_role:httagd"};		/// HARD_TAG_SUB HARD_TAG_ROLE_SYSTEM tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ROLE_SECURITY{"_role:security"};	/// HARD_TAG_SUB HARD_TAG_ROLE tagd::POS_TAG

/***** flags *****/
inline constexpr std::string_view HARD_TAG_FLAG{"_flag"};			/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_FLAG
// ignore TS_DUPLICATE errors
inline constexpr std::string_view HARD_TAG_IGNORE_DUPLICATES{"_ignore_duplicates"};	/// HARD_TAG_SUB HARD_TAG_FLAG tagd::POS_FLAG

/***** include *****/
inline constexpr std::string_view HARD_TAG_INCLUDE{"_include"};		/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_INCLUDE

/***** errors *****/
inline constexpr std::string_view HARD_TAG_ERROR{"_error"};		/// HARD_TAG_SUB HARD_TAG_EVENT tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TAGD_ERR{"_error:tagd_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TAG_UNKNOWN{"_error:tag_unknown"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TAG_DUPLICATE{"_error:tag_duplicate"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TAG_ILLEGAL{"_error:tag_illegal"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_RANK_ERR{"_error:rank_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_RANK_EMPTY{"_error:rank_empty"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_RANK_MAX_VALUE{"_error:rank_max_value"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_RANK_MAX_LEN{"_error:rank_max_len"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URI_ERR_SCHEME{"_error:uri_err_scheme"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_EMPTY{"_error:url_empty"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_MAX_LEN{"_error:url_max_len"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_ERR_SCHEME{"_error:url_err_scheme"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_ERR_HOST{"_error:url_err_host"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_ERR_PORT{"_error:url_err_port"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_ERR_PATH{"_error:url_err_path"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_URL_ERR_USER{"_error:url_err_user"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_NOT_FOUND{"_error:ts_not_found"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_DUPLICATE{"_error:ts_duplicate"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_SUB_UNK{"_error:ts_sub_unk"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_RELATOR_UNK{"_error:ts_relator_unk"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_OBJECT_UNK{"_error:ts_object_unk"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_REFERS_TO_UNK{"_error:ts_refers_to_unk"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_CONTEXT_UNK{"_error:ts_context_unk"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_AMBIGUOUS{"_error:ts_ambiguous"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_RELATION_DEPENDENCY{"_error:ts_relation_dependency"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_ERR_MAX_TAG_LEN{"_error:ts_err_max_tag_len"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_ERR{"_error:ts_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_MISUSE{"_error:ts_misuse"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_INTERNAL_ERR{"_error:ts_internal_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TS_NOT_IMPLEMENTED{"_error:ts_not_implemented"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_TAGL_ERR{"_error:tagl_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_ERROR_HTTP_ERR{"_error:http_err"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_BAD_TOKEN{"_bad_token"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_UNKNOWN_TAG{"_unknown_tag"};	/// HARD_TAG_SUB HARD_TAG_ERROR tagd::POS_TAG

/***** used in errors, but not necessarily (could be used in other contexts) *****/
inline constexpr std::string_view HARD_TAG_CAUSED_BY{"_caused_by"};	/// HARD_TAG_SUB HARD_TAG_RELATOR tagd::POS_RELATOR
inline constexpr std::string_view HARD_TAG_LINE_NUMBER{"_line_number"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_EMPTY{"_empty"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG

/***** URIs *****/
// TODO #define HARD_TAG_URI "_uri"   // resources located by URIs
// resources located by URLs
inline constexpr std::string_view HARD_TAG_URL{"_url"};		/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
/*** URL part hard tags ***/
// the sub object of url part hard tags
inline constexpr std::string_view HARD_TAG_URL_PART{"_url_part"};	/// HARD_TAG_SUB HARD_TAG_ENTITY tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_HOST{"_host"};		/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
// private label of a registered tld (i.e. the "hypermega" in hypermega.com)
inline constexpr std::string_view HARD_TAG_PRIV_LABEL{"_private"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_PUBLIC{"_public"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_SUBDOMAIN{"_subdomain"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_PATH{"_path"};		/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_QUERY{"_query"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_FRAGMENT{"_fragment"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_PORT{"_port"};		/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_USER{"_user"};		/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_PASS{"_pass"};		/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG
inline constexpr std::string_view HARD_TAG_SCHEME{"_scheme"};	/// HARD_TAG_SUB HARD_TAG_URL_PART tagd::POS_TAG

// A hard_tag_axiom is one point in the invariant hard-tag subspace.
struct hard_tag_axiom {
    std::string_view id;
    std::string_view sub_relator;
    std::string_view super_object;
    tagd::part_of_speech pos;
    uint64_t packed_rank;
    uint8_t rank_size;
};

struct hard_tag_id_index {
    std::string_view id;
    size_t axiom_index;
};

} // namespace tagd

#include "../hard-tag-lookups.inc.h"

// Preserve unqualified use sites while the canonical definitions live in namespace tagd.
using tagd::HARD_TAG_ENTITY;
using tagd::HARD_TAG_SUB;
using tagd::HARD_TAG_RELATOR;
using tagd::HARD_TAG_HAS;
using tagd::HARD_TAG_CAN;
using tagd::HARD_TAG_NUMBER;
using tagd::HARD_TAG_INTEGER;
using tagd::HARD_TAG_FLOAT;
using tagd::HARD_TAG_INTERROGATOR;
using tagd::HARD_TAG_WHAT;
using tagd::HARD_TAG_SEARCH;
using tagd::HARD_TAG_REFERENT;
using tagd::HARD_TAG_REFERS;
using tagd::HARD_TAG_REFERS_TO;
using tagd::HARD_TAG_CONTEXT;
using tagd::HARD_TAG_MESSAGE;
using tagd::HARD_TAG_TERMS;
using tagd::HARD_TAG_EVENT;
using tagd::HARD_TAG_ACTION;
using tagd::HARD_TAG_LOG_EVENT;
using tagd::HARD_TAG_COMMAND_EVENT;
using tagd::HARD_TAG_TAGDB_EVENT;
using tagd::HARD_TAG_TAGDB_PUT_EVENT;
using tagd::HARD_TAG_TAGDB_DEL_EVENT;
using tagd::HARD_TAG_TAGDB_GET_EVENT;
using tagd::HARD_TAG_TAGDB_QUERY_EVENT;
using tagd::HARD_TAG_HTTP_EVENT;
using tagd::HARD_TAG_HTTP_REQUEST_EVENT;
using tagd::HARD_TAG_HTTP_RESPONSE_EVENT;
// Bring all HTTP event hard tags into unqualified scope so tests and generated
// code continue to see them without tagd:: qualification, matching convention.
using tagd::HARD_TAG_HTTP_REQUEST_GET_EVENT;
using tagd::HARD_TAG_HTTP_REQUEST_HEAD_EVENT;
using tagd::HARD_TAG_HTTP_REQUEST_PUT_EVENT;
using tagd::HARD_TAG_HTTP_REQUEST_POST_EVENT;
using tagd::HARD_TAG_HTTP_REQUEST_DELETE_EVENT;
using tagd::HARD_TAG_HTTP_RESPONSE_OK_EVENT;
using tagd::HARD_TAG_HTTP_RESPONSE_NOT_FOUND_EVENT;
using tagd::HARD_TAG_PARSE_EVENT;
using tagd::HARD_TAG_SCAN_EVENT;
using tagd::HARD_TAG_ROLE;
using tagd::HARD_TAG_ROLE_SYSTEM;
using tagd::HARD_TAG_ROLE_SCANNER;
using tagd::HARD_TAG_ROLE_PARSER;
using tagd::HARD_TAG_ROLE_DRIVER;
using tagd::HARD_TAG_ROLE_TAGDB;
using tagd::HARD_TAG_ROLE_TAGSH;
using tagd::HARD_TAG_ROLE_HTTAGD;
using tagd::HARD_TAG_ROLE_SECURITY;
using tagd::HARD_TAG_FLAG;
using tagd::HARD_TAG_IGNORE_DUPLICATES;
using tagd::HARD_TAG_INCLUDE;
using tagd::HARD_TAG_ERROR;
using tagd::HARD_TAG_ERROR_TAGD_ERR;
using tagd::HARD_TAG_ERROR_TAG_UNKNOWN;
using tagd::HARD_TAG_ERROR_TAG_DUPLICATE;
using tagd::HARD_TAG_ERROR_TAG_ILLEGAL;
using tagd::HARD_TAG_ERROR_RANK_ERR;
using tagd::HARD_TAG_ERROR_RANK_EMPTY;
using tagd::HARD_TAG_ERROR_RANK_MAX_VALUE;
using tagd::HARD_TAG_ERROR_RANK_MAX_LEN;
using tagd::HARD_TAG_ERROR_URI_ERR_SCHEME;
using tagd::HARD_TAG_ERROR_URL_EMPTY;
using tagd::HARD_TAG_ERROR_URL_MAX_LEN;
using tagd::HARD_TAG_ERROR_URL_ERR_SCHEME;
using tagd::HARD_TAG_ERROR_URL_ERR_HOST;
using tagd::HARD_TAG_ERROR_URL_ERR_PORT;
using tagd::HARD_TAG_ERROR_URL_ERR_PATH;
using tagd::HARD_TAG_ERROR_URL_ERR_USER;
using tagd::HARD_TAG_ERROR_TS_NOT_FOUND;
using tagd::HARD_TAG_ERROR_TS_DUPLICATE;
using tagd::HARD_TAG_ERROR_TS_SUB_UNK;
using tagd::HARD_TAG_ERROR_TS_RELATOR_UNK;
using tagd::HARD_TAG_ERROR_TS_OBJECT_UNK;
using tagd::HARD_TAG_ERROR_TS_REFERS_TO_UNK;
using tagd::HARD_TAG_ERROR_TS_CONTEXT_UNK;
using tagd::HARD_TAG_ERROR_TS_AMBIGUOUS;
using tagd::HARD_TAG_ERROR_TS_RELATION_DEPENDENCY;
using tagd::HARD_TAG_ERROR_TS_ERR_MAX_TAG_LEN;
using tagd::HARD_TAG_ERROR_TS_ERR;
using tagd::HARD_TAG_ERROR_TS_MISUSE;
using tagd::HARD_TAG_ERROR_TS_INTERNAL_ERR;
using tagd::HARD_TAG_ERROR_TS_NOT_IMPLEMENTED;
using tagd::HARD_TAG_ERROR_TAGL_ERR;
using tagd::HARD_TAG_ERROR_HTTP_ERR;
using tagd::HARD_TAG_BAD_TOKEN;
using tagd::HARD_TAG_UNKNOWN_TAG;
using tagd::HARD_TAG_CAUSED_BY;
using tagd::HARD_TAG_LINE_NUMBER;
using tagd::HARD_TAG_EMPTY;
using tagd::HARD_TAG_URL;
using tagd::HARD_TAG_URL_PART;
using tagd::HARD_TAG_HOST;
using tagd::HARD_TAG_PRIV_LABEL;
using tagd::HARD_TAG_PUBLIC;
using tagd::HARD_TAG_SUBDOMAIN;
using tagd::HARD_TAG_PATH;
using tagd::HARD_TAG_QUERY;
using tagd::HARD_TAG_FRAGMENT;
using tagd::HARD_TAG_PORT;
using tagd::HARD_TAG_USER;
using tagd::HARD_TAG_PASS;
using tagd::HARD_TAG_SCHEME;
