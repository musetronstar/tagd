#pragma once
#include <string>

namespace tagd {

// TAGD_CODES_START  // marks automated processing
typedef enum {
	TAGD_OK = 0,

	TS_INIT,

	// all codes >= TAGD_ERR are considered errors
	TAGD_ERR,
	TAG_UNKNOWN,
	TAG_DUPLICATE,
	TAG_ILLEGAL,

	// rank errors
	RANK_ERR,
	RANK_EMPTY,
	RANK_MAX_VALUE,  // max value per rank level
	RANK_MAX_LEN = 255, // max bytes in _data string

	// URI errors
	URI_ERR_SCHEME,

	// URL errors
	URL_EMPTY,
	URL_MAX_LEN = 2112, /* We are the Priests of the Temples of Syrinx */
	URL_ERR_SCHEME,
	URL_ERR_HOST,
	URL_ERR_PORT,
	URL_ERR_PATH,
	URL_ERR_USER,

	// tagspace
	TS_NOT_FOUND,
	TS_DUPLICATE,
	TS_SUB_UNK,
	// TS_SUBJECT_UNK, // reserved; no current code path distinguishes subject unknown from tagspace not found
	TS_RELATOR_UNK,
	TS_OBJECT_UNK,

	// TS_REFERS_UNK, // reserved; current referent errors report refers_to/context causes instead
	TS_REFERS_TO_UNK,
	TS_CONTEXT_UNK,
	TS_AMBIGUOUS,
	TS_RELATION_DEPENDENCY,

	TS_ERR_MAX_TAG_LEN,
	TS_ERR,				// general error due to caller input
	TS_MISUSE,			// caller misusing class interface
	TS_INTERNAL_ERR,	// error not due to caller input
	TS_NOT_IMPLEMENTED,	// functionality not yet implemented

	TAGL_ERR,

	HTTP_ERR
} code;
// TAGD_CODES_END

/*\
|*| order of operations for tagd::predicates
|*| for operators and data types
\*/
typedef enum {
	TYPE_INTEGER,
	TYPE_FLOAT,
	TYPE_STRING
} data_t;

typedef enum {
	CMD_GET,
	CMD_PUT,
	CMD_DEL,
	CMD_QUERY
} command_t;

// when comparing two values L and R
typedef enum {
	OP_LT,     // L < R
	OP_LT_EQ,  // L <= R
	OP_EQ,     // L == R
	OP_GT_EQ,  // L >= R
	OP_GT      // L > R
} operator_t;

/* TODO
typedef enum {
	PRINT_PRETTY,		// use whitespace for maximum readability
	PRINT_DELIM_NL,		// use newlines when delimitting lists
	PRINT_ID_ONLY,		// only print the tag id
	PRINT_ID_SUB_ONLY	// only print the tag id and the sub relation
} print_options_t;
*/


// TAGL part of speech
typedef enum {	// PART_OF_SPEECH_START  // marks automated processing
	POS_UNKNOWN       = 0,
	POS_TRUE          = 1 << 0,
	POS_FALSE         = 1 << 1,
	POS_TAG           = 1 << 2,
	POS_SUB_RELATOR   = 1 << 3,
	POS_SUB_OBJECT    = 1 << 4,
// relator is used as a "Copula" to link subjects to predicates
// (even more general than a "linking verb")
	POS_RELATOR       = 1 << 5,
	POS_INTERROGATOR  = 1 << 6,
	POS_URL           = 1 << 7,
	// POS_URI // TODO
	POS_ERROR         = 1 << 8,
	POS_SUBJECT       = 1 << 9,
	// a relator at use in a relation
	POS_RELATED       = 1 << 10,
	POS_OBJECT        = 1 << 11,
	POS_MODIFIER      = 1 << 12,
	POS_REFERENT      = 1 << 13,
	POS_REFERS        = 1 << 14,
	POS_REFERS_TO     = 1 << 15,
	POS_CONTEXT       = 1 << 16,
	POS_FLAG          = 1 << 17,
	POS_INCLUDE       = 1 << 18
} part_of_speech;  // PART_OF_SPEECH_END
const int POS_END     = 1 << 19;

/*\
|*|      TODO
|*|
|*| tagd_pos rank tree
|*| ------------------
|*| Each tagd_pos is a tagd::rank type
|*| that holds the HARD_TAG_POS rank + tadg_pos subtree rank
|*|
|*| Encoding the tad_pos into the tagspace rank tree help us
|*|   * eliminate t_id <=> id_t conversion in tagspace
|*|     The rank now fills the role of:
|*|     * each rank uniquely identifies rank == old t_id
|*|     * rank == t_id
|*|       relation     tad rank     offset
|*|       --------     --------     ------
|*|       id           == tagd::rank + POS_ID (0)
|*|       sub_relator  == tagd::rank + POS_SUB
|*|       super_object == tagd::rank + POS_SUB_OBJECT
|*|               pos  == tagd::rank + POS_POS
|*|     * rank == (rank + pos) to deduce value of other each pos in a tag
|*|     * makes a tagd rank an iterable set over a buffer of data
|*|       from start == rank == id to end
|*|     * allow to deduce the end of a tag in a buffer of data given any rank,
|*|		* allows us to copy an entire tagspace (by value)
|*|		  using only a buffer of utf8 and list of ranks
|*|       VERY PORTABLE!
|*| So that each tagd_id utf8 string value
|*| tagspace indexes will optimize reads of
|*| an entire tag as all the data will be

|*| Also this allows us to make tagd_pos searchable and extensible in TAGL
|*| And allow to seek directly within a rank index
\*/

// string literal of each tagd::code
const char* code_str(tagd::code);
const char* code_error_tag(tagd::code);

// string literal of each part_of_speech
const char* pos_str(tagd::part_of_speech);

// string list corresponding to each bit set in part_of_speech value
std::string pos_list_str(tagd::part_of_speech);

} // namespace tagd
