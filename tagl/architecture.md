TAGL: Scanner -> Parser -> Callback Architecture
------------------------------------------------
####  push model  ####


            +-------------------------------+
            |  push tokens  |    create    5|
            |       to      >    parse      |
    +---+   |4    parser    |    tree       |
    | t |   |-------^--------------v--------|
    | a |-->| lookup tokens |  populate    6|
    | g |   | trans tag POS |  tag object   |
    | d |<--|3  to token ID |  from tree    |   +---+
    | b |   |-------^--------------v--------|   | t |
    +---+   |     scan/     |   callback   7|-->| a |
            |      lex      |  tagdb CRUD   |   | g |
            |2    tokens    | set results   |<--| d |
            |-------^--------------v--------|   | b |
            |      TAGL driver manages     8|   +---+
            |   scanner, parser, callback,  |
            |1    tagdb and I/O resources   |
            +-------------------------------+
                    ^              | 
                    |     push/    | 
                    |   callback   |
                    |              v
               +---------+    +----------+
               |  INPUT  |    |  OUTPUT  |
               +---------+    +----------+


### Steps
1. Caller creates TAGL `driver` passing its `callback`, `tagdb`,
   and (optionally) `session` objects, where
   * a `session` manages tagdb `contexts` and
   * the `driver` creates `scanner` and `parser` objects
   * The caller calls the `driver::execute()` method
     passing in TAGL statements.
     (Ultimately, each TAGL statement will represent a tag object)
   * The execute method initializes scanner and parser instances then
     calls the `scanner::scan()` method
2. The scanner scans/lexes input into token id/values and calls the
   `driver::parse_tok()` method determining that:
   * Some tokens are known *syntatically*, such as
     - commands: get `<<`, put `>>`, del `!!` and set `%%`
     - operators: `<`, `<=`, `==`, `>=`, `>`
	 - resources: `URL`, `HDURI` `file`
	 - terminator: `;`
     - etc.
   * Other tokens are known *semantically* by
     "looking up their tagd part of speech" given a `tag id`, where:
3. The scanner calls the `driver::lookup_pos()` method which
   given the `tag id`:
   * calls the `tagdb::pos()` method (using its context if given)
   * returning a `tag pos`
   * that is translated into a parser token `tok_id`
4. Token id/values are passed to the `parser` via the
   `driver::parse_tok()` method which:
5. Pushes tokens on the parser stack to
   * create the `parse tree`, which
   * when a statement is terminated (`;`):
6. Creates a tag object that is either
   * an `error` object,
   * or a type of `tag` object which gets populated
     (that `driver` currently holds a reference to - see *TODO 1*)
7. The `driver::do_callback()` method is called (see *TODO 1.1*)
   and results are written to I/O resource (stdout/stderr, evbuffer, etc.)
8. Control is returned to the caller (see *TODO 2*) which
   * calls the driver `finish()` method which
   * tears down the parser and other allocated resources

### TODO
1. Fix token destructor or scan from rotating input buffers

   The scanner currently creates a new string objects on the heap
   for every lexxed token, so the The Parse() function accepts
   string pointers as input tokens. This might be fine (SQLite does this),
   but we need to get a handle on why lemon is not destroying these
   tokens as advertised (surely a usage error).
   See `test_search()` in `tagl/tests/Tester.h`

   However, it might be cleaner and faster to not dynamically allocate
   each token on the heap, but rather scan TAGL into one of two
   rotating buffers:

                     +---------------------+
                     |                     |
                     |      +--> buf_a     |
    +---------+      |     /     ^         |      +----------+
    |  INPUT  |----->| SCAN    rotate      |----->|  OUTPUT  |
    |  TAGL   |      |     \     v         |      |  TOKENS  |
    +---------+      |      +--> buf_b     |      +----------+
                     |                     |
                     +---------------------+

So that in input operation will:
* read from an evbuffer
* copy into either `buf_a` or `buf_b`, rotating between each read

Instead of
    `%token_type {std::string *}`
use
    `%token_type {token}`

Where token is:

    struct token {
		small buf_i;	// buf index
		char *tok;		// ptr into buf_a or buf_b
		size_t sz;		// size of tok
		tagd:pos pos;	// TAGL part of speech
    }

But this would not be high priority if we can fix the bugs
we are having with try to let lemon destroy our tokens.

2. When we are truly doing an *asyncronous callback*, the caller will
   not call `driver::finish()`, rather,
   * the callback object will finish
   * and the the caller will be *notified via message passing*
   * the driver should not hold a reference to `_cmd` and `_tag` members.
   Instead:
   * Rename `do_callback()` to `setup(cmd, tag)` which is called by the parser
       * the parser will create `cmd` and `tag` variables/objects
       * no need for the `driver` to hold them
   * Add an overloaded `()` method to callback, making it a functor
       * the callback as a functor object is called instead of `do_callback()`

