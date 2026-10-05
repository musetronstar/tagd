# tagd Design Thesis

This is a design of the **tagd** *semantic-relational* system, **tagspace**s and the **TAGL** language
as we wish it to be, not as it actually is now - we should mark features as DONE or some level of TODO.

This is a work in progress and subject to change...

## Description
TAGL will be a Turing complete functional-declarative language, so things like BITWISE operations will not be off limits. I want to use a "best of" from different languages as inspiration.  I need to put this altogether in a design document, where we can design the language TODOs even though its not yet implemented.

## Inspirations
But in short - the language features appear and  behave as:
* Erlang as default and fallback when unsure.
  + recursive functions (effectively using tail recursion) - no loops unless needed later
  + variables are constant
  + selection done through pattern matching - no `if` statements
  + but `tags` are the atoms of the system - whereas Erlang atoms are but labels, tags are labels that also represent hierarchy and relation - not OOP, but adds a dimension of hierarchy and membership - tags can extend what Erlang has in atoms, by baking in "is a" and "has a" relations in a tag.
  + asynchronous functions are truly lightweight threads - there can be tens of thousands easily. They act like processes where parameters are not "passed in" on the stack, but as TAGL through buffers. They are isolated in that they cannot do I/O except through buffers provided to them. Values are not returned, rather TAGL is written to the output buffer. They can be polled or instructed through signals. They can be local or remote - it doesn't matter. Upon error, they emit their error TAGL to an error buffer and are killed and dropped from the internal/virtual process table.
* Set Theory and Mathematics
* C style operators for: logical, comparison, bitwise, mathematical, operations
* C++ scoping, closure, lambda and namespaces. `using` keyword
* Python style `**` exponentiation, sequence generality, slicing, comprehension, "dot method" chaining
* Perl style automatic `/regex/` patterns, capturing and binding
* JavaScript style single threaded non-blocking asynchronous worker thread model.

That's not complete, but I want to remain consistent with the meaning/behavior/usage of symbols in TAGL as they are in the languages it was inspired from.

## Workflow

We will follow the Joe Armstrong Erlang thesis: "Making reliable distributed systems in the presence of software errors"
https://erlang.org/download/armstrong_thesis_2003.pdf

Use his style, rigor and methodical approach to building the case for the language step by step, all supporting his thesis.

Moving, paragraph by paragraph, chapter by chapter, detailing the TAGL language in parallel to the Armstrong thesis.

## Thesis Outline

I will provide the background and introduction of TAGL later, so lets jump forward to the Erlang Thesis section `1.2 Thesis outline`

TASK: Provide a sparse/high level TAGL Thesis Outline - we can fill in detail later.

## Identity

The tagd system addresses two fundamental questions:

* What is it?
* How is it related?

These questions are resolved through a unified model:

* identity is defined by one hierarchical subordinate relation
* relations are defined by horizontal predicate relations

In tagd, meaning is derived structurally:

* a tag's position in the hierarchy (given by its rank) establishes its identity within the tagspace
* predicate relations describe how the tag relates to other tags

TAGL provides a formal language for acting upon these structures. The canonical form of a tagspace is its dump - the UTF-8 TAGL code that represents the tagspace (and can be loaded).

Imagine canonical tagspace dump on a large canvas where the TAGL text is printed in canonical form. TAGL operations can be imagined as mechanical operations on paper:
* CMD_PUT: cut and insert a section
* CMD_GET: select and extract a section
* CMD_QUERY: scan for then select and extract matching sections
* CMD_DELETE: cut out a section

The tagd system provides a computing environment in which these structures can be:

* defined
* accessed
* queried
* compared
* serialized in canonical form (tagspace dump)

This approach enables knowledge representation that is:

* explicit
* deterministic
* inspectable

All semantics are encoded in the tagspace itself. No external schema or hidden interpretation layer is required.

### Leibniz’s Identity of Indiscernibles (TODO)

The tagspace obeys (and will increasingly enforce) **Leibniz’s Identity of Indiscernibles** (PII):

> Two entities are identical if and only if they share all properties.

In tagd terms: two tags are the *same* entity if and only if they occupy the same structural position (identical rank + subordinate relation + predicate fiber).

**Current reality:** Nominal `id` is still a distinguishing property, so two tags with different ids but otherwise identical structure (same rank, same relations) are currently permitted. This is a pragmatic allowance to avoid breaking existing bootstrap and user data.

**Future direction:** We will introduce tagspace-level flags (or hard constraints) requiring full Leibniz discernibility: no two tags may share identical rank + relations. At that point, differing only by name while occupying the same structural position becomes a hard error (`TS_DUPLICATE` / `TS_MISUSE`).

This aligns the system with both mathematical identity and the Physics of Meaning.

---

### Computation and Functions

TAGL unifies knowledge representation and computation by treating functions as **tags** within the tagspace.

- Defining a function is a `>>` PUT operation that binds executable behavior to a tag identifier.
- Functions are first-class: they can be assigned to variables, passed as arguments, returned from other functions, and stored in the tagspace.
- Variables (`$name`) are distinct from tags and follow single-assignment semantics.
- Anonymous functions (lambdas) use the form `($params) -> body ;`.
- Recursion is the fundamental control structure; there are no loops.
- Guards may leverage the full expressive power of the Alexandrov topology and Kripke-style modal operators (`□`, `◇`).

This design draws from Erlang's declarative and reliable style while embedding computation directly inside the mathematical structure of the tagspace. Functions become inspectable, versionable, and queryable entities — just like any other knowledge in the system.

---

### The Physics of Meaning

TAGL rests on the recognition that language and knowledge are fundamentally **relational phenomena**. We therefore approach semantics not merely as a linguistic problem, but as a **mathematical physics of meaning**.

From Kenneth Pike we take the powerful tripartite analogy:
- **Particle** — the discrete tag (nominal identity)
- **Wave** — sequential structure and predicate flow
- **Field** — the contextual system in which meaning emerges

From Lucien Tesnière we adopt the relational core of language:
- The **relator** as the structural center (verb-centeredness)
- **Valency** — the defined relational capacity of a relator (how many arguments it expects)
- The distinction between **actants** (core participants) and **circumstants** (optional modifiers)

tagd mathematics supplies the rigorous substrate that unifies these traditions:

- The subordinate relation (`<:` / `_sub`) induces an **Alexandrov topology**, giving us a well-defined geometric **field**.
- Ranks encode **structural identity** (position within the topology).
- Horizontal `_rel` predicates express dynamic **valency** and interaction.
- Modal operators (`□` necessity, `◇` possibility) allow us to reason (using Kripke Semantics) about what must hold or may hold across the field.

In this model, **"To be, is to be related"** is no longer philosophical poetry. It is the foundational axiom of a constructive system in which identity, structure, and meaning are mathematically inseparable.

We are therefore not merely designing a programming language or a knowledge representation system. We are building a **relational physics of semantics** — where tags are particles, relations are forces, and the tagspace is the field in which meaning coheres.

---

### Proofs as Programs, Types as Propositions

The theoretical foundations already present in TAGL — Alexandrov topology, Kripke semantics, constructive identity, and modal operators — converge on a single unifying correspondence in the foundations of mathematics and computer science: the **Curry-Howard-Lambek correspondence**.

#### The Correspondence Trilogy

| Logic | Type Theory | Category Theory |
|---|---|---|
| Proposition | Type | Object |
| Proof | Program / Term | Morphism |
| Implication | Function type | Exponential |
| Conjunction | Product type | Product |
| Truth | Unit type | Terminal object |

- **Curry-Howard** — proofs are programs; propositions are types (logic ↔ computation)
- **Lambek** — adds the categorical dimension; morphisms are computations (computation ↔ geometry)
- Together: logic, computation, and space are the *same thing* viewed from three different angles

This correspondence was observed independently by Haskell Curry and William Alvin Howard (whose 1969 manuscript was circulated for a decade before publication in 1980), and extended to category theory by Joachim Lambek. Its deepest expression is found in Per Martin-Löf's **Intuitionistic Type Theory** (1984), where dependent types allow propositions to quantify over data — and in **Homotopy Type Theory (HoTT)**, where identity types are reinterpreted as *paths* in a topological space.

#### Why This Matters for TAGL

The TAGL tagspace already embodies the key ingredients of this correspondence:

- **Tags as types** — a tag is not merely a label; it *is* a type. Instantiating a tag (placing a term under it in the hierarchy) is constructing a proof that the term inhabits that type.
- **The subordinate relation as constructive identity** — Martin-Löf's identity types (`Id_A(a, b)`) assert that `a` and `b` are propositionally equal. In TAGL, the subordinate relation (`_sub`) is the constructive witness of hierarchical identity. To assert `x _sub y` is to inhabit the identity type — to *prove* the relation, not merely assert it.
- **Alexandrov topology as the geometric leg** — the Lambek correspondence connects computation to category theory, and the Alexandrov topology on the tagspace provides exactly the categorical structure (a preorder is a thin category) that makes this geometric reading precise.
- **Modal operators as necessity types** — the `□` (necessity) operator in Kripke semantics corresponds to a dependent type that must be inhabited across all accessible worlds. `◇` (possibility) corresponds to existential inhabitation in at least one world.

The key insight for the TAGL thesis is this:

> A TAGL tag relation is not merely asserted — it is *inhabited*. To place a tag in the hierarchy is to construct a term of that type. The tagspace is a proof environment.

This elevates TAGL from a knowledge representation system to a **constructive formal system** in which every stored relation carries the weight of a proof term, every query is a proof search, and every tagspace dump is a canonical proof object.

#### Recommended Study

The following works provide the theoretical grounding for this section, in suggested reading order:

1. Philip Wadler, *"Propositions as Types"* (CACM, 2015) — the most accessible entry point to Curry-Howard
2. Per Martin-Löf, *Intuitionistic Type Theory* (Bibliopolis, 1984) — the foundational source for dependent types and constructive identity
3. The HoTT Book, Chapter 1 (freely available at homotopytypetheory.org) — identity types as paths in a topological space
4. Joachim Lambek & Phil Scott, *Introduction to Higher Order Categorical Logic* (1986) — for the categorical leg of the correspondence
5. Steve Awodey, *Category Theory* (2nd ed., Oxford, 2010) — a readable foundation for the Lambek piece
6. Dana Scott - mathematical semantics of programming languages
7. Karl Hewwit - Actor Model

