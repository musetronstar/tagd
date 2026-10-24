## Hard Tags

*Hard tags*, being hard-coded in the software are the fundamental building
blocks by which we extend the TAGL language.  All user-defined tags are
subordinate to hard tags.

Hard tags *must* be used in TAGL, however, their string values *should not*
be hard-coded in C/C++ code, rather use the corresponding constants defined in
`tagd/include/tagd/hard-tags.h`.

For example, the `_entity` hard tag in TAGL corresponds to `HARD_TAG_ENTITY`
in the C/C++ code.

Since TAGL doesn't extensively rely on keywords for syntax (rather, using
hard tags), every hard tag (and every subsequent extended user-defined tag)
is assigned a `tagd::part_of_speech`.  Instead of keywords, TAGL is made up of
members of classes of TAGL parts of speech.

### URI Style Naming

Hard tags having a super object having the same name embedded or same type,
should use a URI ':' separator in the form: '_super:sub'

For example, `_error:ts_not_found` not `_ts_not_found_error`

### Adding Hard Tags

1. Add an entry to the `tagd::part_of_speech` enum in `tagd/include/tagd/codes.h`
   Don't forget to increment POS_END.

2. Add a constant definition to `tagd/include/tagd/hard-tags.h`.
   Give meaningful names to constants and hard tags that strongly correspond.

3. Add the `TERMINAL` symbol corresponding to the `tagd::part_of_speech`
   to TAGL grammar in `tagl/src/parser.y`.  The *lemon parser* will generate
   the `TOK_TERMINAL` definitions in the `tagl/include/parser.h` header used
   by our scanner.

4. Add an entry into the `switch` statment in the `driver::lookup_pos()`
   function in `/tagl/src/tagl.cc`. Lookups against the `tagspace` will return
   a `tagd::part_of_speech` which we will translate into a `TOK_TERMINAL`
   value and pass to the parser.

5. Make sure sqlite::term_pos_occurence() in `tagspace/tagdb/sqlite/src/sqlite.cc`
   accommodates the new `tagd::part_of_speech`. The first SQL branch reads
   the declared `tags.pos`, including `POS_TRUE` and `POS_FALSE`; the other
   branches collect positions where a tag is used. The foreign-key error
   switch describes those uses, so it needs no separate boolean cases.


### Boolean Hard Tags

`_true` (`HARD_TAG_TRUE`, `POS_TRUE`) and `_false`
(`HARD_TAG_FALSE`, `POS_FALSE`) are children of `_entity`.
User-defined descendants inherit the corresponding part of speech, including
indirect descendants such as `certainly <: yes <: _true`.

TAGL maps these parts of speech to boolean tokens. For example,
`%% _ignore_duplicates _true;` enables duplicate suppression, while
`%% _ignore_duplicates _false;` disables it. Numeric and unrelated values
are rejected; there is no numeric-to-boolean conversion.

### Filepile Vocabulary

`_link:filepile` is beneath `_link`, beneath `_entity`; links use ordinary
`POS_TAG`. `_from` and `_to` are relators. `_file:stored` is beneath `_file`
and stored files retain `POS_URL`. `_revision`, `_mtime`, `_stored_time`,
`_file_size`, and `_media_type` are metadata objects used with `_has`.
`_hash:SHA-256` is beneath `_hash`; its modifier is a lowercase hex string.

Generated link IDs may be up to 8192 bytes; source URLs retain the independent
2112-byte limit. Generated session/event and filepile timestamps now use UTC
with nine fractional digits. Existing explicit timestamp strings are unchanged.

`_error:hduri_err` identifies malformed HDURI component encoding. HDURI escapes
literal percent signs independently of URL escaping; this changes persisted URL
and filepile link identities containing percent sequences. See
[HDURI identity encoding](../docs/TAGL-spec.md#hduri-identity-encoding) for the
current format, limits, and pre-beta recreation requirements.
