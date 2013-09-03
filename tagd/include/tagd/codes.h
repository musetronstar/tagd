#pragma once

namespace tagd {

typedef enum {	// TAGD_CODES_START  // marks automated processing
    TAGD_OK,

    TAG_INSERTED,
    TAG_UNKNOWN,
    TAG_DUPLICATE,
    TAG_ILLEGAL,

    // urls
    // URL_OK uses TAGD_OK instead
    URL_EMPTY,
    URL_MAX_LEN = 2112, /* We are the Priests of the Temples of Syrinx */

    // url errors
    URL_ERR_SCHEME,
    URL_ERR_HOST,
    URL_ERR_PORT,
    URL_ERR_PATH,
    URL_ERR_USER,

	// tagspace
    TS_INIT,		// initial state (not initialized)
    // TS_OK uses TAGD_OK instead /* 200 */
    TS_NOT_FOUND,   /* 404 */
    TS_DUPLICATE,
    TS_SUPER_UNK,  /* 400 */
    TS_SUBJECT_UNK,  /* 400 */
    TS_RELATOR_UNK,  /* 400 */
    TS_OBJECT_UNK,  /* 400 */
    TS_ERR_MAX_TAG_LEN, /* 400 */
    TS_ERR,         /* general error due to caller input */
    TS_MISUSE,      /* caller misusing class interface */
    TS_INTERNAL_ERR, /* error not due to caller input  */ /* 500 */

    TAGL_INIT,
    TAGL_ERR,

	HTTP_ERR
} code;  // TAGD_CODES_END

} // namespace tagd

// string for each tagd::code
const char* tagd_code_str(tagd::code);

#define tagd_code  tagd::code
