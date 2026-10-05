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
* typeology via *type relations*
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
  A NULL value indicates a bare object with no modifier

* `quantifier`: a numeric subtype of a`modifier`
   (CURRENT: number stored as UTF-8 text)

* `object_list`: one or more `object`s attached to a `subject` with a `relator`

* `sub_relator`: a specific `object` in an *identity relation* that
  cannot be modified

* `super_object`: the `object` of a tag’s *identity relation* to which the
  `subject` is subordinate

* `predicate`: a relation composed of a `relator` connecting an `object_list`
  applied to a `subject`

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

predicate_list ::= relator object_list

object_list ::= object_list ',' object
object_list ::= object

object ::= TAG
object ::= TAG "=" INTEGER
object ::= TAG "=" FLOAT
object ::= TAG "=" STRING
```

#### 5.3 GET Grammar

```bnf
get_statement ::= "<<" subject
```

#### 5.4 DELETE Grammar

* Same as PUT
* MUST NOT include sub relation

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

* A tag MUST be introduced via a sub relation
* Sub relation object MUST exist

### 6.3 Predicates

* A subject MAY have multiple predicates
* Each predicate consists of:
  + a relator
  + an object_list

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

* Queries traverse sub relations
* `*` matches any predicate

## 10. Context and Referents

```tagl
%% _context example;
```

## 11. URLs

* URLs are valid TAGL tokens

## 12. Errors

* Errors are represented as tags

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
* contradiction handling
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
