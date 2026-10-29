# TAGL Specification

## 1. Introduction

### 1.1 Purpose

TAGL is the language of the *tagd semantic-relational system* for defining
and querying knowledge as a tagspace, a hierarchical tree structure of tags
connected by *relations*.

It a very general sense, TAGL asserts answers to the question,
*"what is it, and how is it related?"*

> To be, is to be related.

TAGL expresses *semantic meaning* using relations in a *three-layer structure*:
* heirachical positional identity via *subordinate relations*
* typology via *type relations*
* horizontal relational attributes (or properties) via *predicates*

### 1.2 Scope

This specification defines:

* lexical structure (tokens)
* grammar (syntax)
* semantic model
* evaluation rules
* statements

### 1.3 Non-Goals

This specification does NOT define storage, interfaces or applications of TAGL
nor does it describe the tagd system at large.

## 2. Conventions and Terminology

tagd code terms are surrounded in markdown with backticks as `inline code`.

### 2.1 Normative Keywords

* MUST
* MUST NOT
* SHOULD
* MAY

### 2.2 Definitions

* tag: a semantic-relational entity identified nominally by a globally unique
  UTF-8 text label (ID) and structurally by its subordinate relation within a
  tagspace

* globally unique: a unique tag ID within a tagspace - different tagspaces
  can share the same ID without conflict, though they might not share meaning

* Universally unique: a unique tag ID to all known tagspace sets

* hard tag: a "hard coded" *immutable* tag common in all tagspaces

* `hard_tagspace`: the superordinate tree of *hard tags* from which all
  user defined tags are derived - `_entity` is the root of the tree

* `tagspace`: a tree of tags defined using **subordinate relations**
  (aka identity relations).

* `tagspace_set`: a set of one or more tagspaces

* `subject`: the tag for which *one and only one* *subordinate relation*
  defines identity and to which *zero or more* *type relations* and
  *predicates* are applied.

* `relator`: a tag that relates a subject to one or more objects

* `object`: a tag that is related to a subject by a `relator` which MAY bind to a
  optional `modifier` using an operator.

* `modifier`: a UTF-8 byte string value bound to an `object`.
  A NULL value indicates a bare object with no modifier.
  Current modifier types are literals, not tags: their values never undergo
  contextual referent encoding/decoding, even when they match a referent or tag ID.
  PUT, GET, QUERY, and DELETE use these literal values. Previously stored bytes
  remain authoritative; older substitutions cannot recover the original spelling.

* `quantifier`: a numeric subtype of a`modifier`
   (CURRENT: number stored as UTF-8 text)

* `object_list`: one or more `object`s attached to a `subject` with a `relator`

* `sub_relator`: a specific `object` in an *identity relation* that
  cannot be modified

* `super_object`: the `object` of a tag’s *identity relation* to which the
  `subject` is subordinate

* `predicate`: a relation composed of a `relator` connecting an `object_list`
  applied to a `subject`

* **type relation** (also **type predicate**): a modifier-free
  `[type_relator + type_object]` declaration that adds an inheritance source.
  Type relations and subordinate relations are distinct kinds; both provide
  inheritance. Ordinary predicate objects do not provide inheritance.

* **explicit relation** (or **explicit declaration**): a relation asserted directly
  on a subject. An **explicit predicate** is a directly stored ordinary predicate.

* **effective relations**: relations available after recursive inheritance.
  **Effective predicates** are the combined ordinary predicates after compatible
  normalization. An **effective description** preserves identity and explicit type
  relations while returning effective predicates. “Inherited” describes a source;
  “effective” describes the combined result.

* `predicate_list`: a list of one or more `predicate`s grouped by `relator`s

* relation: a `[sub_relator + super_object]`, a `[type_relator + type_object]`, or
   a `[relator + object]`applied to a `subject`

* identity relation (aka *subordinate relation*,  *sub relation*): a relation
  formed by a `[sub_relator + super_object]` that determines the `rank` of a
  `tag`, directly under the `super_object`, which provides *structural identity*

* nominal identity: the identity of a tag defined by its globally unique ID
  which is a UTF-8 label stored in a `tag`s `_id` field.

* structural identity: the identity of a tag defined by its subordinate
  relation (rank)

* `tagd_pos`: token types in the TAGL grammar determined by their
  subordinate relation (inherited by their parent)

### Notes:

A `predicate` is formed when a relator is followed by an `object_list`.  
A `relator` alone is not a `predicate`.

### Metaphors

Certain English parts of speech can serve as a metaphor for TAGL parts of speech:

* noun-like: subjects and objects
* verb-like (Copula): relators
* prepositions: relational operators (`<`, `<=`, `=`, `>`, `>=`)
* adjective-like: string modifiers
* quantifiers: quantifiers (a numeric modifer subtype)

However, TAGL is *formal language* as an abstraction of *natural language*
which *elicits* the *latent semantic-relational* structure in natural language
and makes it explicit through statements which operate at a *meta-level*
above certain classes of *declaritive statements* and *imperative statements*
in natural language.

## 3. tagd Core Model

All tokens in TAGL are UTF-8 labels.

### 3.1 Tag

First class objects in TAGL - the atoms of meaning.

### 3.2 Identity (Sub Relations)

A tag’s identity is defined positionally by a sub relation.

```tagl
>> dog _is_a mammal
```

The TAGL parts of speech in this example are:
* `subject`: `dog`
* `sub_relator`: `_is_a`
* `object`: `mammal`

In this example, `dog` would be placed subordinate to its parent `mammal`.
As a visualization of the ranks as a dotted string
(the actual rank is composed of a sequence of UTF-8 codepoints), given that
the rank of `mammal` was `1.3.4.2`, and dog was the first child, it might be
`1.3.4.2.1` - notice that the child tag has the parent as its prefix.
This is how the tree is ordered.

So, the semantic meaning is derived from the positional rank `1.3.4.2.1`,
not the nominal ID `"dog"`.

Rules:

* A `subject` MUST have a globally unique ID to be defined in a `tagspace`
* The `sub_relator` MUST exist in a tagspace
* The `object` MUST exist in a tagspace

### 3.3 Predicates

Predicates define horizontal relations about a subject (aka attributes or properties).

```tagl
>> dog _has legs = 4;
```

The `relator` `_has` alone is not a `predicate`, in this case, it is composed of
`relator + object + operator + quantifier`: `_has legs = 4`.

Note:
The `object = quantifer` (`legs = 4`) operates similarly to what is seen in a
*key-value NoSQL database*, however the object being a tag also *relational*.

Rules:

* A `predicate` consists of a `relator` followed by an `object_list`.
* A `relator` MUST be a tag having a `tagd_pos` of `POS_RELATOR`.
subordinate to `_rel` ().

### 3.4 Modifier Types

(CURRENT) Modifier values have types identified *syntactically* by the scanner
and stored as UTF-8 bytes.  They are not tags in of themselves.

* `QUOTED_STRING` (e.g. "53cr37P@55w0rD-LOL")
* `quantifier` (modifer subtype)
  * `INTEGER` number literal (e.g., `0`, `1`, `55`, `-23`)
  * `FLOAT` rational number literal (e.g., `3.14`, `-1.0`, `0.0`)

Empty text (`= ""`) and an absent modifier currently share the empty-modifier
representation. Explicit NULL operands are unsupported. Numeric operations use
the existing integer and finite native floating-point representation; exact
cross-type numeric ordering, signed-zero identity, and shared primitive values
are specified separately.

#### Semantic numeric tags (FUTURE MAYBE)

If we needed to make semantic statements *about* numeric values,
we might define something like this (conceptual example):
```tagl
_integer _sub _number
_float   _sub _number
_number  _sub _entity
```

Rules:

* `_integer` and `_float` are distinct types. No implicit coercion between them.
* Both types are always signed. A leading `-` is part of the literal token,
  not a unary operator. `"-23"` MUST be emitted as a single `INTEGER` token.
* Scientific notation is DEFERRED (not currently supported).

### 3.5 Type Relations

`_type_of` is a `POS_TYPE_RELATOR` hard tag directly beneath `_entity`. Its
structural descendants are custom type relators. Each type object MUST already
exist locally; ordinary local URL tags are allowed. Foreign type references are
unsupported and return `TS_MISUSE`.

```tagl
>> animal _sub _entity;
>> mobile _sub _entity;
>> dog _is_a animal _type_of mobile;
>> classifies _sub _type_of;
>> dog classifies mobile;
```

A type relation has no modifier, operator, or modifier type. Adding one leaves
its subject's structural parent, rank, and POS unchanged. A new subject still
requires a subordinate relation; an existing subject can add types directly.

### 3.6 Tagspace

A tagspace is a hierarchical tree structure defined by subordinate relations.

An *Alexadrov topoloogy* is induced by the tagspace ranks.

* Root tag: `_entity`
* `_entity _sub _entity` is axiomatic and the only tag to allow a
*self-reflexivity*, which we must have to terminate an *infinite regress*
of subordinate relations.

Tags derive their structural identity through their position in the
tree hierarchy given by an *ordinal rank*.

## 4. Lexical Structure

### 4.1 Commands

* `>>` PUT
* `<<` GET
* `!!` DELETE
* `??` QUERY
* `%%` SET

### 4.2 Operators

* `<:` maps to `_sub`
* `->` maps to `_rel`
* `=` assignment
* `,` separator
* `*` wildcard

### 4.3 Literals

* TAG: UTF-8 label
* INTEGER: signed whole number (e.g., `0`, `1`, `55`, `-23`)
* FLOAT: signed decimal number (e.g., `3.14`, `-1.0`, `0.0`)
* STRING: quoted

### 4.4 Comments

```tagl
-- line comment
-* block comment *-
```

## 5. Syntax

### Statements

#### Statement Termination

A statement MUST be terminated by a terminator.

A terminator MUST be one of:

* a semicolon (`;`)
* a double newline (`"\n\n"`)

The scanner MUST emit a terminator token for either form.

Canonical form:

The `tagspace` `dump()` method outputs the TAGL text of a tagspace in canonical form.

* A statement MUST be terminated by a double newline
* A semicolon MUST NOT be used when the following line is empty
* A semicolon MUST be used when statements are written on consecutive non-empty lines

#### 5.1 Statement Types

* PUT
* GET
* DELETE
* QUERY
* SET

#### 5.2 PUT Grammar

```bnf
put_statement ::= ">>" subject_sub_relation predicates
put_statement ::= ">>" subject_sub_relation
put_statement ::= ">>" subject predicates

subject_sub_relation ::= subject sub_symbol TAG

subject ::= TAG

predicates ::= predicates predicate_list
predicates ::= predicate_list
predicates ::= predicates type_predicate_list
predicates ::= type_predicate_list

type_predicate_list ::= TYPE_RELATOR type_object_list
type_object_list ::= type_object_list ',' TAG
type_object_list ::= TAG

predicate_list ::= relator object_list

object_list ::= object_list ',' object
object_list ::= object

object ::= TAG
object ::= TAG "=" INTEGER
object ::= TAG "=" FLOAT
object ::= TAG "=" STRING
```

For stored relations, `(subject, relator, object)` identifies one optional
modifier. PUT with a supplied new modifier replaces the existing modifier; it
does not add a second value for that relation. A bare object asserts the
predicate's existence and preserves any existing modifier. If the predicate is
absent, a bare PUT inserts it without a modifier.

Returned tags own their values and resolved ordering. Canonical output follows
these rules:

| Aspect | Contract |
| --- | --- |
| Predicate order | Ascending relator rank, object rank, then literal modifier UTF-8 bytes; absent modifier first. Thus `10` precedes `2`, including numeric modifiers. Operator and modifier type break otherwise equal keys. |
| Rank order | Compare binary path bytes. Referent spelling does not change order. Context-free values use immutable hard-tag ranks, then identifier order for unresolved labels. |
| Type relations | Order by relator rank, then object rank. |
| Typed membership | Compare numeric values within their stored type. Compatible integer and float values remain distinct members; updates retain operator/type metadata. |
| Snapshots | Later moves, deletion, cache eviction, or closing the tagspace do not alter earlier results. New reads resolve current ranks; unresolved components fail through the error interface. |
| Identifiers and strings | Quote non-URL identifiers when syntax requires it. Quote strings starting with a digit, `+`, `-`, or `.` to preserve type, e.g. `legs = "4"` or `_config:bind_port = "7070"`. Valid TAGL quote/backslash escapes survive repeated dump/load. |
| Floats | Append `.0` when the stored spelling has neither a decimal point nor an exponent. |
| Dumps | Emit explicit declarations in dependency-safe stages and subject order, with predicates ordered as above. Reads/dumps never store inherited assertions. |

A valid PUT that changes stored state returns `TAGD_OK`. Reasserting an existing
identity and relations without changing any values, including modifiers, returns
`TS_DUPLICATE`, unless `_ignore_duplicates` is enabled. Mixing unchanged
predicates with an insertion, modifier update, or subject move is a successful
change. Other validation errors retain their existing behavior.

#### 5.3 GET Grammar

```bnf
get_statement ::= "<<" subject
```

GET returns **explicit declarations** by default: identity, directly stored type
relations, and explicit predicates. Enable the effective description with
`%% _include_inherited_predicates _true;` (`F_INCLUDE_INHERITED_PREDICATES` in
C++), and disable it with `_false`.

Both structural ancestors and recursively reached type objects contribute
predicates and type assignments (§6.4). Validation uses all contributions,
independently of the display flag.

```tagl
>> parent _sub _entity _has _message = inherited;
>> child _sub parent _has _message;
%% _include_inherited_predicates _true;
<< child;
-- Returns: child _sub parent _has _message = inherited
```

In C++, the flag fills `abstract_tag::relations` with effective predicates;
`type_relations` still contains direct declarations. TAGL prints those predicates
as ordinary relations on the returned subject. Compatible integer and float
contributions remain distinct members. Returned tags own their data and resolved
ordering, so later mutations or closing the tagspace do not alter them.

Reads never persist inherited assertions. Explicitly PUTting an expanded result
can assert them locally, as with any other PUT. Some effective combinations
cannot be stored as one direct relation with one modifier. Use explicit-only
dumps for declaration-preserving reconstruction, including required dependencies.

The flag also expands each result of `get_children()` and each returned QUERY
description (§9). URL descriptions follow the same rules. The private
configuration profile store retains its structural override policy and rejects
type declarations; ordinary tagspaces always use the assertion semantics in §6.

#### 5.4 DELETE Grammar

DELETE uses PUT's relation grammar but MUST NOT include a subordinate relation.
Without relations, it deletes the subject subject to reference checks. Removing
a local type relation leaves all independently reachable sources available.

| Predicate DELETE | Local state | Result |
| --- | --- | --- |
| Bare `!! s r o` | Local predicate | Remove the local predicate. |
| Valued `!! s r o = v` | Same type, operator, and value | Clear its modifier; retain the bare predicate. |
| Valued | Different local value | `TS_NOT_FOUND`. |
| Either | Matching assertion is inherited only | `TS_MISUSE`; the source is unchanged. |
| Either | Matching assertion is absent | `TS_NOT_FOUND`. |

Deleting local assertions never masks inherited contributions. A failed
multi-predicate DELETE leaves the tagspace unchanged.

```tagl
>> dog _sub _entity _has _message = 4;
>> dog _has _message;       -- TS_DUPLICATE; 4 remains
!! dog _has _message = 4;   -- retains: dog _has _message
!! dog _has _message;       -- removes the predicate
```

#### 5.5 QUERY Grammar

```bnf
query_statement ::= "??" interrogator predicates
query_statement ::= "??" "<search terms>"
```

#### 5.6 Numeric Literal Examples

##### Modifier Context

```tagl
-- integer modifiers
>> dog _is_a mammal
    _has legs = 4, weight = 23;

>> rectangle _is_a shape
    _has width = 10, height = 5;

>> temperature _is_a measurement
    _has value = -23, threshold = 0;

-- float modifiers
>> circle _is_a shape
    _has radius = 3.14;

>> water _is_a substance
    _has boiling_point = 100.0, freezing_point = 0.0;
```

##### Literal Expression Statements

Bare numeric literals terminated by `;` or double newline.
In tagsh these echo the value. In a file context the value is legal but silent.

```tagl
1;
-23;
0;
3.14;
-1.0;
0.0;
```

##### Notes

* a bare literal expression statement requires a new grammar rule in `parser.y`
* `TOK_FLOAT` rule MUST precede `TOK_INTEGER` in the scanner
* scientific notation is DEFERRED

## 6. Semantic Model

### 6.1 Evaluation

TAGL statements are evaluated against a tagspace.

* PUT
  + define predicates
  + mutate predicates
* GET
  + retrieve definition
* DELETE
  - remove predicates
  - remove tags
* QUERY
  + retrieve matching subjects

### 6.2 Identity Constraints

* A new non-URL subject MUST be introduced with one subordinate relation.
* Its subordinate relator and parent MUST exist in the tagspace.
* Structural position determines grammatical classification. Insertion derives
  POS from the destination family when it is more specific than `POS_TAG`.
* Reparenting recomputes POS for the moved subtree. Existing subordinate/type-relator uses must remain
  valid, and type relators cannot occupy ordinary predicate positions. Moves
  violating these requirements are rejected atomically. URL identity retains its
  scheme-derived subordinate relation; filepile classification uses `_type_of`.

### 6.3 Local Assertions

Each `(subject, relator, object)` has one optional local modifier. A supplied PUT
replaces it; the old and replacement values are not simultaneous constraints.
Bare PUT preserves a supplied value. Numeric `4` and `4.0` are compatible but
have distinct typed identities; changing one to the other updates storage.
String `"4"` is not coerced to a number.

| Assertion | Result |
| --- | --- |
| Unchanged typed assertion already local | `TS_DUPLICATE`, or `TAGD_OK` with `_ignore_duplicates`. |
| Assertion present only through inheritance | Store it locally, subject to consistency checks. |
| Self-typing or typing as `_entity` | After reference/POS validation, `TAGD_OK` no-op without storage. |
| Direct type relation implied by current paths | Store it; subsequent reassertions follow the duplicate rule. |
| Repeated paths or diamonds | Contribute once per source; no duplicate error. |

Self/root no-ops neither count as new declarations nor suppress duplicate checks
for other assertions in the operation. No ignore flag suppresses consistency checks.

### 6.4 Recursive Inheritance

In a finite tagspace state, dependencies run from each subject to its structural
parent and explicit type objects. Omit `_entity`'s axiomatic self-parent edge.
Write `x ≼ s` when s reaches x, including zero steps. The combined dependency
graph MUST be acyclic, so ≼ is a partial order extending structural containment.
Nontrivial structural, type, and mixed cycles return `TS_MISUSE`. This includes
reparenting a tag beneath any of its own descendants.

Every reached source contributes its explicit predicates and access to its own
parents and types. Subjects inherit types from structural ancestors even without
an explicit assignment. Type membership includes reachable objects and their
structural ancestors. Resolution is independent of source and traversal order.

For each exact resolved relator–object pair, intersect all supplied value
constraints. They MUST admit a representable common value; an empty intersection
is `TS_CONTRADICTION`. Bare predicates leave the modifier unspecified. Local
assertions never mask inherited constraints, and unequal pairs remain independent.

Validate every contribution before display normalization. Merge identical typed
contributions, preserving compatible integer and float contributions separately.
A bare general predicate may be absorbed when another predicate specializes both
its relator and object; crossed directions do not specialize. Normalization never
changes source declarations.

### 6.5 Atomic Mutation Validation

Validate reference/POS integrity, dependency cycles, direct duplicates, then
effective consistency. Source edits, type additions, and reparenting check all
affected inheritors, including subjects outside the changed structural subtree.
Multi-subject PUT checks the final combined state. Removing constraints relaxes
the description but still obeys reference checks.

Failure preserves declarations, ranks, references, and search state. Diagnostics
identify the affected subject and conflicting source assertions through the
existing error interface. Sequential TAGL scripts retain their per-operation
transaction behavior.

### 7. Functions

Functions in TAGL are first-class tags that carry executable behavior. Defining a function is performed with the `>>` (CMD_PUT) command.

#### 7.1 Named Functions

```tagl
>> name($param1, $param2) _when guard ->
    body ;

>> name($param1, $param2) ->
    body.
```

#### 7.2 Anonymous Functions (Lambdas)

```tagl
$var = ($param1, $param2) ->
    body ;
```

#### 7.3 Variables

- Variables are prefixed with `$`.
- Variables are single-assignment (immutable after binding).
- Assignment uses `=`.

```tagl
$request = get_current_request($S);
$response = handle_http_request($request, $S);
```

#### 7.4 Guards

Guards are introduced with `_when` and support modal and topological expressions:

- `□` → necessity (`_necessarily`)
- `◇` → possibility (`_possibly`)
- `<:` → subordinate relation (`_sub`)

#### 7.5 Symbolic Aliases

| Symbol | Maps to        | Mathematical Meaning                  |
|--------|----------------|---------------------------------------|
| `<:`   | `_sub`         | specialization / containership        |
| `~>`   | `_rel`         | relator / horizontal relation         |
| `□`    | `_necessarily` | necessity (true in all accessible worlds) |
| `◇`    | `_possibly`    | possibility (true in some world)      |

#### 7.6 Recursion and Control Flow

TAGL has no looping constructs. All iteration is expressed via recursion. The runtime SHOULD optimize tail calls.

Closures are supported: lambdas may capture `$variables` from the lexical scope in which they are defined.

## 8. Hard Tags

Core:
* `_entity`
* `_sub`
* `_rel`

## 9. Query Semantics

Structural selectors use rank containment. Ordinary predicates and type
conditions match the integrated effective description, including conjunctions
satisfied by different sources. A bare predicate matches a relation whose
relator and object respectively specialize its components. Both components must
agree in direction. A valued predicate constrains only its exact resolved pair.

Canonical `_type_of` conditions test the reflexive integrated order, including
structural ancestry even without a stored type edge. Conditions using a custom
type relator require a matching declared relator on a reachable source and match
the assigned object's integrated ancestors. Returned descriptions list only the
subject's explicitly declared type relations.

For a valued QUERY, intersect the contributing constraints to obtain the admitted
set A. Match only when A is nonempty and entails the query constraint. For example,
`legs = 4` entails `legs > 2`, while `legs > 2` together with `legs < 5` does not
entail `legs = 3`. Missing and bare-only pairs do not prove valued queries.
Strings and numbers are distinct domains. Effective positive propositions persist
along ≼ within a fixed valid state. Full-text search retains its structural
containment behavior; it is not a predicate-entailment proposition.

QUERY membership is independent of `_include_inherited_predicates`. When the
API returns tag descriptions, each match contains its whole explicit declaration
by default, including predicates unrelated to the query. Enabling the flag
returns its effective predicates instead. Declared type relations remain explicit
in both modes. ID-only shell/HTTP results retain their existing shape.

## 10. Context and Referents

```tagl
%% _context example;
```


### Tagspace SET and UNSET

An application owns one volatile `_config` and a stable anonymous memory
instance `_default`. User tagspaces have description tags beneath `_tagspace`
in `_config`. Descriptions and alias referents survive closing; open state is
runtime ownership, not a stored predicate.

```tagl
%% _use _config;
>> file:///work/animals.db;
>> zoology _sub _tagspace _from file:///work/animals.db;
>> japanese _sub _entity;
>> 動物園 _refers_to zoology _context japanese;
%% _tagspace /work/animals.db = z;
%% _use z;
>> dog _sub _entity;
%% _use _default;
%% !! _tagspace zoology = z;
%% !! _tagspace zoology;
%% _use _config;
!! zoology; -- ordinary dependencies can still prevent deletion
```

Grammar (the alias modifier is optional):

```text
SET          := %% (_tagspace | _use) target [= alias] ;
UNSET        := %% !! _tagspace target [= alias] ;
CLEAR_CONTEXT := %% !! _context ;
```

`_tagspace` opens without itself selecting; `_use` opens then selects.
Repeated opening or the same alias binding succeeds. Qualified UNSET removes
only the `_config`-context alias, even while active. Unqualified UNSET closes
without deleting descriptions, aliases, or source files. Absent bindings and
already-closed instances succeed; conflicting bindings fail. Borrowed/active
instances and reserved targets cannot close. Creating databases remains the
CLI `--create` operation, not a TAGL command.

Targets are IDs/referents or locations beginning `/`, `./`, `../`, `~/`, or
`file://`. Quote spaces and TAGL delimiters. Path shorthand is confined to these
SET/UNSET arguments; tagdurls use the addressing contract below.
Relative locations use the containing file's directory or interactive working
directory. `~/` expands the OS user's home, independently of tagd home. Only
file URLs decode percent escapes; a literal `%20` in a filesystem path stays
literal. No shell expansion or command substitution is performed.

Equivalent paths and symlinks reuse the source's existing description. If none
exists, the filename stem supplies its ID. An alias never renames that ID.
Conflicting stems require an explicit alternate description. IDs and aliases
share a unique namespace across all contexts and ordinary tags in `_config`;
user tagspaces retain independent namespaces. Names beginning `_` are reserved.

Each description has at most one local `_from` file URL, with no modifier.
Absent `_from`, its source is `<home>/tagspaces/<id>/<id>.db`. Child descriptions
do not inherit sources. `.db` opens existing persistent storage; `.tagl` executes
into a memory instance. Suffixes are case-sensitive. Declaration does not open
or require the file, but source uniqueness is checked again before opening.
Close before editing an effective source; edits never move files. Retained
identity allows closing after the source disappears; reopening resolves its
current location again.

TAGL sources execute sequentially, including SET commands and includes. A new
description/alias is published only after successful execution. A failed load
discards its candidate memory instance but preserves earlier effects elsewhere.
If the script remains in the candidate, `_tagspace` restores the caller's
selection; explicit switches away survive. Successful `_use` finally selects
the loaded instance. Errors never select a discarded instance. This is not a
whole-script transaction; scopes and coordinated rollback remain future work.

Switching retains the session context stack. Tagspace aliases resolve through
that stack in `_config`, with context `_config` as fallback; ordinary operations
then resolve referents in the selected tagspace. Missing lookup contexts provide
no match. Explicitly pushing a missing context returns `TS_NOT_FOUND` without
changing the stack. `%% !! _context;` clears the stack; `%% _context "";` remains
a compatibility spelling.

In `tagsh`, `.tagspaces` lists reserved targets, registered/open and
registered/closed descriptions, aliases, and shallow disk-only/unverified home
entries. `*` marks the active target. Listing never opens or registers sources.
HTTP requests start with the fixed server default and have independent selection.
They may select `_config` (directly or through an alias) only when it was that
startup default; otherwise selection or opening returns `TS_MISUSE`/403 before
reading or adding an alias. Ordinary invalid/conflicting arguments remain client
errors, not policy denials;
see [configuration](configuration.md) for request policy and startup profiles.

## 11. URLs

URLs are valid TAGL tokens and carry their own subordinate relation. An explicit
subordinate relation must match that syntax-derived identity; the tagspace layer
validates this for both TAGL and C++ callers. Bare PUT can declare a URL without
artificial predicates, in any tagspace:

```tagl
>> file:///work/animals.db;
```

An HDURI spelling may be used for the same URL. This general declaration rule
also supplies the file URL objects used by tagspace `_from` predicates.

### HDURI identity encoding

HDURI retains the `hd:` scheme and ten fields, separated by nine `!` delimiters:

```text
hd:rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
```

The public suffix and subdomain labels are reversed to group related hosts.
If domain decomposition would lose host bytes, the complete host occupies
`priv_label` and the other two host fields are empty. The original host is
preserved, including leading/trailing dots and hosts exceeding domain lookup
limits.

Each component escapes literal `%` as `%25` and literal `!` as `%21`.
Decoding removes exactly this one layer; it does not decode URL percent escapes.
For example:

| URL | HDURI |
| --- | --- |
| `http://example.com/a!b` | `hd:com!example!!/a%21b!!!!!!http` |
| `http://example.com/a%21b` | `hd:com!example!!/a%2521b!!!!!!http` |

Identity is the ID produced by ordinary URL parsing, including its existing
scheme/host case handling. HDURI does not introduce further normalization:
percent-escape spelling, explicit ports, UTF-8 bytes, and empty versus absent
query/fragment components remain distinct. The `mailto:` form reconstructs
without `//`; `mailto://` is rejected because the layout does not encode both
forms separately. The `hd:` scheme is reserved for HDURI input.
Explicit empty ports (a trailing `:` in the authority) are rejected with
`URL_ERR_PORT`; the component layout represents absent ports and nonempty ports.

Only canonical fields, as produced by `hduri()`, are accepted. Different host
groupings or noncanonical scheme/host case are rejected. General URL parsing
accepts the `hd:` prefix case-insensitively and normalizes it to `hd:`;
the HDURI-specific constructor requires the literal `hd:` prefix.

An empty password field means no password. The complete password field `%00`
means an explicitly empty password; literal password text `%00` is `%2500`.
The sentinel is invalid in other fields. Invalid field counts, escapes, or
component boundaries return `HDURI_ERR` (`_error:hduri_err`). Empty payloads,
missing schemes, and invalid reconstructed URLs retain their ordinary URI/URL
error codes.

The decoded URL limit remains 2112 bytes. HDURI input has a separate conservative
bound of 6351 bytes: three times the URL limit, plus the `hd:` prefix, nine
separators, and the empty-password marker. Decoding independently checks the
URL limit. This also leaves room for filepile's `link:` prefix and `#999` suffix
within the 8192-byte tag-ID limit.

**Pre-beta encoding break:** escaping `%` changes HDURIs and generated filepile
link IDs containing percent sequences. Recreate affected persisted identities
from authoritative original URLs before using them with this encoding. There is
no version marker, legacy detection, or automatic migration. Old collisions
cannot be reversed reliably; saved old strings that happen to satisfy the new
grammar cannot be identified as old input. No user data is rewritten or deleted
automatically.
Dump reports an invalid stored HDURI, including its stored ID, as an error rather
than emitting a blank URL subject. A failed dump may contain earlier rows and
must not be used as a complete export.

## 12. Errors

Errors are represented as tags. Relevant mutation errors include:

| Error | Meaning | HTTP status |
| --- | --- | --- |
| `TS_DUPLICATE` | Unchanged assertion already stored locally. | 409 |
| `TS_CONTRADICTION` | Joint effective constraints have no common value. | 409 |
| `TS_MISUSE` | Invalid relation role, unsupported foreign type, or structural, type, or mixed cycle. | 400 |
| `TS_NOT_FOUND` | Missing subject, relation, or matching typed modifier. | 404 |

A rejected mutation leaves the tagspace unchanged.

## 13. Examples

```tagl
>> substance _sub _entity;
>> dog _is_a mammal;
```

## 14. Implementation Notes (Non-Normative)

* Scanner: `re2c`
* Parser: `lemon`

## 15. Future Work

* formal EBNF grammar
* revision model

### Kripke Semantics

Possible Operators:
```
<> []

<> [_]

<_> [_]

```

## 16. References

* `README.md`
* `hard-tags.h`
* `scanner.h`

### Configuration-file profile selection

`%% _config_id ID;` is permitted only while loading application configuration.
The last SET across includes selects a profile, resolved after loading using
that SET's context. CLI `--config-id` may override a valid file selection.
See [configuration](configuration.md) for inference, defaults, and diagnostics.

## Tagdurl addressing

The first path segment selects a registered tagspace ID or alias. `_` selects
the active tagspace; `_default` and `_config` are explicit reserved targets.
Decode the selector once and validate it with the shared tagspace name rules.
Malformed escapes, invalid UTF-8, separators, dot segments, NULs, and repeated
slashes are errors. Unknown names return `TS_NOT_FOUND`; no storage is created.

```tagl
%% _use zoology;
/petting_zoo/donkey;  -- GET in petting_zoo for this statement only
/zoology/mammal/;    -- QUERY descendants of mammal in zoology
/_/donkey;          -- GET in the still-active zoology
/petting_zoo;       -- GET its description in _config
/petting_zoo/;      -- QUERY its description's descendants in _config
```

The remaining path preserves existing predicate and search semantics, for
example `/zoology/animal/legs,tail` or `/zoology/*/?q=can+bark`. Context `c`
applies by spelling to both tagspace-alias and target-tag lookups. A context
missing in either tagspace produces no match there, not a validation error.
Explicit SET context commands still require a known context.

Autoload uses the selected profile's `_config:tagdurl_autoload` setting, default
`_false`. Registered descriptions can reopen their databases. With autoload
enabled, available home-layout databases can be discovered; description-only
operations register them without opening storage. Embedded operations restore
the previous selection on success and failure.

HTTP uses the same parser. Each request starts from the server startup default;
selections never change that default. Description GET/HEAD requires `_config`
as the server default, otherwise HTTP 403 is returned before discovery. Data
requests to `_config`, including its aliases, have the same restriction and
remain read-only. HEAD has GET status and authorization without a response body.

`POST /` remains the general TAGL endpoint with request-local switching.
`POST /animals` and `/animals/` accept complete statements but fix the target:
`_use` of the same resolved target is idempotent, while another target or a
cross-target embedded tagdurl is HTTP 403. A rejected combined selection and
alias command creates no alias. `POST /animals/donkey` retains an implicit PUT
subject and predicate body. PUT and DELETE require a tag after the selector;
tagspace-only paths return HTTP 400. HTTP never executes local includes or
closed `.tagl` sources. Open `.tagl` instances remain usable.

`/` (welcome/general POST), `/_file/...`, and `/favicon.ico` remain special routes
and take precedence. A tagspace named `favicon.ico` is masked even if the icon
is missing. Generated links retain the supplied selector and applicable context.
Errors use existing TAGL `_error` objects with HTTP status. Caching, permissions,
redirect policy, scopes, and foreign references remain future work.

### Migration

Old active-relative URLs must acquire a selector: `/dog` becomes `/_/dog`, and
`/mammal/` becomes `/_/mammal/`. Bare `/dog` now addresses the description of a
tagspace named dog; it never falls back to reading dog from the active target.
Update external scripts and stored URL values explicitly; databases are not
automatically rewritten. Root searches can use `/_/*/?q=...`. The HTTP client
still accepts arbitrary complete URLs without rewriting them.

### Stored-file URL classification

A stored filepile URL retains its syntax-derived `_sub _file` identity and
receives the system-maintained type relation `_type_of _file:stored`. Dumps
include that relation and all association/content metadata. Ordinary PUT cannot
create this classification, even indirectly through a type or subordinate
object beneath `_file:stored` in the integrated order. Direct PUT of a managed
filepile URL is `TS_MISUSE`. Explicit restore is a separate pending feature;
ordinary loading cannot reconstruct managed filepile records, and legacy
`_is_a _file:stored` declarations are incompatible.

### STORE (`>>+`)

STORE uses the PUT grammar, with a `POS_URL` subject. Supplied predicates and
type relations describe source A. The shared tagspace operation acquires bytes
and atomically commits the supplied PUT with stored URL B and association L.
URL identity and source-eligibility rules still apply. Local file URLs are
supported; unsupported acquisition schemes return `TS_NOT_IMPLEMENTED`.

```tagl
>>+ file:///photos/a.jpg _has _message = "Holiday photo";
```

A duplicate means the whole command changes nothing: both the declaration and
latest bytes are unchanged. It returns `TS_DUPLICATE`, or `TAGD_OK` when
`_ignore_duplicates` is enabled. Changes to either part succeed regardless of
that setting; unchanged bytes retain L and its revision. Other failures commit
neither part. Complete published bytes can remain unreferenced after a semantic
commit failure; this is not a cross-filesystem transaction.

The command is available in every local input mode and prints L's ID on success.
The shell's `--store FILE_OR_URL...` option queues this command for each operand;
`--with 'PREDICATES'` attaches predicates to the immediately preceding store group.
Path conversion is CLI convenience; TAGL itself still requires a URL subject.
All filepile invariants are enforced by the tagspace layer. HTTP invocation,
network acquisition, restore mode, roles, and concurrent operations are separate
work. See [Filepile storage](filepile.md) for metadata, diagnostics, and limits.

### Managed DELETE (`!!+`)

`!!+` shares ordinary DELETE grammar. Without predicates, the subject must be a
URL. A source URL deletes A and its retained links, plus unshared B records and
bytes. A stored B URL deletes B and all links to it, preserving the sources.
Both roles on one URL produce `TS_MISUSE`; a URL with no managed storage follows
ordinary DELETE. Original source resources and existing audit tags survive.

```tagl
!!+ file:///photos/a.jpg _has _message = "Holiday photo";  -- clear the message modifier
!!+ file:///photos/a.jpg;  -- the source tag and its managed storage
```

Predicate and type-relation forms retain the subject and stored bytes, even
when removing the final predicate. Subordinate relations are rejected. Ordinary
`!!` cannot remove A with filepile links, L, or B: these return `TS_DEPENDENCY`.
Generated URL, content and association facts cannot be edited through ordinary
PUT/DELETE or predicate-bearing `!!+`; user descriptions retain normal semantics.

Managed deletion validates unrelated dependencies, atomically deletes selected
records, then cleans up eligible files. Shared local content survives. Unknown
commit outcomes prohibit cleanup; post-commit cleanup errors report partial
completion without rolling back the deletion. Missing stored bytes warn without
failing an otherwise successful deletion. Required database absence or
unavailability remains fatal. The initiating `_event:filepile_delete` logs the
final result through `_has _message` without automatic event persistence.

The initial contract assumes one participating persistent tagspace and sequential
operations. Deleted revisions may be reused: allocate the highest retained
revision plus one, or `001` when none remain. See [Filepile storage](filepile.md).
