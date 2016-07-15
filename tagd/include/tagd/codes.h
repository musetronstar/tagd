#pragma once

namespace tagd {

typedef enum {	// TAGD_CODES_START  // marks automated processing
    TAGD_OK = 0,

    TS_INIT,
    TAGL_INIT,

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

    // url errors
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
    TS_SUPER_UNK,
    TS_SUBJECT_UNK,
    TS_RELATOR_UNK,
    TS_OBJECT_UNK,

    TS_REFERS_UNK, 
    TS_REFERS_TO_UNK,
    TS_CONTEXT_UNK,
	TS_AMBIGUOUS,
	TS_FOREIGN_KEY,

    TS_ERR_MAX_TAG_LEN,
    TS_ERR,				// general error due to caller input
    TS_MISUSE,			// caller misusing class interface
    TS_INTERNAL_ERR,	// error not due to caller input

    TAGL_ERR,

	HTTP_ERR
} code;  // TAGD_CODES_END

} // namespace tagd

// string for each tagd::code
const char* tagd_code_str(tagd::code);
