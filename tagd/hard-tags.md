## Hard Tags

*Hard tags*, being hard-coded in the software are the fundamental building
blocks by which we extend the TAGL language.  All user-defined tags are
subordinate to hard tags.

Hard tags *must* be used in TAGL, however, their string values *should not*
be hard-coded in C/C++ code, rather use the corresponding macros defined in
`tagd/include/tagd/hard-tags.h`.

For example, the `_entity` hard tag in TAGL corresponds to `HARD_TAG_ENTITY`
in the C/C++ code.

Since TAGL doesn't extensively rely on keywords for syntax (rather, using
hard tags), every hard tag (and every subsequent extended user-defined tag)
is assigned a `tagd::part_of_speech`.  Instead of keywords, TAGL is made up of
members of classes of TAGL parts of speech.

### Adding Hard Tags

1. Add an entry to the `tagd::part_of_speech` enum in `tagd/include/tagd.h`
   Don't forget to increment POS_END.

2. Add a macro definition to `tagd/include/tagd/hard-tags.h`.
   Give meaningful names to defines and hard tags that strongly correspond.

3. Add the `TERMINAL` symbol corresponding to the `tagd::part_of_speech`
   to TAGL grammar in `tagl/src/parser.y`.  The *lemon parser* will generate
   the `TOK_TERMINAL` definitions in the `tagl/include/parser.h` header used
   by our scanner.

3. Add an entry into the `switch` statment in the `driver::lookup_pos()`
   function in `/tagl/src/tagl.cc`. Lookups against the `tagdb` will return
   a `tagd::part_of_speech` which we will translate into a `TOK_TERMINAL`
   value and pass to the parser.

4. Make sure sqlite::term_pos_occurence() in `tagdb/sqlite/src/sqlite.cc`
   accomodates the new tagd::part_of_speech.
