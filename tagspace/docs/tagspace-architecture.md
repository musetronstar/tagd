# Tagspace Architecture

The mathematical anchor is `../../docs/tagd-math/tagd-math.tex`.
TAGL semantics are specified by `../../docs/TAGL-spec.md`.

## Classes and responsibilities

| Class | Responsibility | Why it is separate |
| --- | --- | --- |
| `tagd::const_tagspace` | Reads tags, ranks, and parts of speech; checks containment; orders predicates; creates tagspace sessions. | Both stored tags and immutable hard tags need these operations. |
| `tagd::tagspace` | Adds `put()`, `del()`, and `merge()` to `const_tagspace`. | Applications can modify tags without choosing a storage backend. |
| `tagd::hard_tagspace` | Reads the hard tags and ranks generated from `hard-tags.h`. | Hard tags are shared by all tagspaces and cannot be modified at runtime. |
| `tagd::stored_tagspace` | Owns a private backend and forwards operations to it. | Backend types, handles, and headers stay inside the `tagspace/` module. |
| `tagd::tagspace::persistent` | Creates or opens a named tagspace under a tagd home directory. | Persistent data must survive destruction of the C++ object. |
| `tagd::tagspace::memory` | Opens an independent SQLite `:memory:` database. | It provides the same storage and full-text search behavior without a database file. |
| `tagd::session` | Holds session identity, event sequence, and errors. | Event identity and errors are useful to modules that do not work with tags. |
| `tagd::session_factory` | Generates core session identities and timestamps. | Core session constructors use the same identity-generation code. |
| `tagd::tagspace_session` | Adds referent context and a tag-membership callback to `session`. | Tag operations need context and access to their tagspace as well as event identity. |
| `tagdb::tagdb` | Defines the internal storage interface. | A future backend can replace SQLite without changing application calls. |
| `tagdb::sqlite` | Implements storage and full-text search using SQLite. | Database files, SQL, and SQLite handles belong inside this backend. |

The first three classes and the session classes live in the core `tagd/`
module. The storage classes live in `tagspace/`, with the backend under
`tagspace/tagdb/`. Public storage headers do not expose backend types.

## Ownership and sessions

Applications construct memory or persistent tagspace objects on the stack.
These objects cannot be copied or moved: each owns its backend, and sessions
refer to that particular tagspace object. A consumer given `tagd::tagspace*`
uses the object but does not own or delete it.

The tagspace creates sessions for tag operations. `get_session()` returns a
session value suitable for a local variable. `new_session()` returns an owned
pointer; the caller must delete it, normally through `release_session()`.
A session contains a callback that asks its tagspace whether a tag exists.
For shared-owned tagspaces, this callback retains the owner, including when a
session is copied. Stack-owned tagspaces must outlive their sessions. Copying
a session copies its context and identity without creating a new identity.

`httagd` gets a session from the selected tagspace for each request. It passes
that session through the TAGL driver and tag operations to the backend.
The backend uses the supplied session; it does not create tag-operation
sessions. Backend-only tests supply a membership callback themselves.
The inherited `tagd::session` constructor uses the core session factory to
create the event identity. No additional project-wide session factory is needed.

An error in a tag operation, such as an unknown tag, is reported on the supplied
session. Internal storage errors are reported on the tagspace object. Callers
can share an error list, as httagd does for each request. Sharing the list does
not share each object's return code. The storage wrapper keeps its own code
consistent with the backend code.

## Logging

Backend operations use `_role:tagdb` and `_event:tagdb_get`, `_event:tagdb_put`,
`_event:tagdb_del`, and `_event:tagdb_query`. Their event program is `tagdb`.
`_role:tagspace` is distinct and is available for diagnostics from the tagspace
module itself. No tagspace event types are declared until an operation in that
module needs them. A logger configured through `TAGSPACE_SET_LOGGER()` also
receives backend logs, filtered by the backend's own role.

## Initialization and named storage

```cpp
tagd::tagspace::persistent ts;
auto rc = ts.create("Zoology", "/tmp/tagd-home");
// On later runs, use ts.init("Zoology", "/tmp/tagd-home") instead.
```

`create()` and `init()` return `tagd::code` and record errors on failure.
The overloaded methods `create(name)` and `init(name)`, which omit the home
argument, use `~/.tagd`. These are initialization methods, not constructors.
Successful creation leaves the tagspace ready for use; do not call `init()`
again on that object. `tagspace::memory::init()` opens independent memory storage.

Persistent layout is `<home>/tagspaces/<name>/<name>.db`. `init()` opens existing
storage only. `create()` reserves a new database file and initializes it,
creating missing parent directories. An existing directory can be reused
if its database is absent. An existing database is never overwritten, even
if invalid. Failed creation removes its new database and any newly created
empty directories, preserving pre-existing contents.

Names are validated as UTF-8 before being used as path components. Unicode
and internal spaces are allowed; path separators, control characters, `.` and
`..`, empty names, surrounding whitespace, and overlong filenames are rejected.
Tagspace directory/database symlinks are rejected. The selected home is a
caller-supplied filesystem path.

## Integration

`tagl`, `tagsh`, and `httagd` include public tagspace headers and link
`libtagspace.a` (plus the required system library). They do not open database
files or include backend headers. Logging identifiers can name the backend
without giving applications access to backend storage types.

`--tagspace ID` opens existing storage, `--create` requires that option and
creates new storage, and `--home DIR` changes the default home. Options are
order-independent. Without `--tagspace`, both applications use memory storage.
Each process operates on one selected tagspace. The former `--db` option is removed.

## Predicate ordering and canonical caching

Ranks encode ancestry: an ancestor's rank is a prefix of a descendant's rank.
`_entity` has the empty root rank and contains every existing tag; an unknown ID
is never treated as a root. User ranks extend the hard-tag hierarchy without
reusing the ranks of hard tags. Stored predicates resolve canonical relator
and object ranks before referent encoding. Iteration and serialization compare
those ranks, then literal modifier bytes, then operator/type metadata.

Each predicate owns immutable private ordering information. The comparator
performs no storage lookup and captures no tagspace or cache lifetime. The
existing `rank_comparator()` returns that same comparator; explicit preparation
uses `order_predicates()`. Ordinary GET, query, and dump paths prepare values
automatically. Rank lookup with a status result distinguishes the empty root,
a missing tag, and a storage error; SQLite reads only the rank column.

Query condition evaluation retains its existing semantic order and selected
predicates. Ordering results does not turn a partial predicate query into a full
tag retrieval. Full-text queries continue to retrieve their existing full tags.

`predicate_set` has a set-style interface with stable const node iterators for
insertion and erasure of other nodes. Copy/move/swap preserve snapshot ranks.
Extraction, node insertion, and merge transfer nodes without invalidating
references to transferred values. Distinct IDs break rank collisions when
values from different snapshots are combined.
Semantic find/count/erase and duplicate detection preserve numeric equivalence.
A separate membership index gives string and empty-value lookups logarithmic
cost. Numeric lookups scan only predicates with the same relator, object, and
operator: legacy numeric equivalence is not contiguous in literal order and is
not suitable as an unchecked tree comparator for arbitrary numeric text.
Rank indexes resolve bounds and insertion keys without scanning predicates.
Indexes refer to owned nodes; extraction/erasure removes those references, copying
rebuilds them, and move/swap transfers them with their nodes. The additional
indexes use O(N) memory. String-only equality is linear; numeric groups can still
require pairwise checks. Three-way comparison traverses semantic groups in order
and sorts only within numeric groups: O(N) for strings, O(N log N) in the numeric
worst case, without changing iteration order.

Bounds and `key_comp()` use literal ordering, so `find(02)` can locate integer `2` even when its literal `equal_range(02)` is empty. This distinction
is intentional, not full `std::set` equivalence semantics. Use ADL `erase_if`
for this user-defined container. Explicit ordering and label transformation
rebuild the container and invalidate iterators; failed resolution leaves it
unchanged. Bounds reuse the destination container's known ranks even for keys
copied from an older snapshot. Editing a detached predicate's IDs discards stale rank data.
Locally inserted unresolved values retain deterministic fallback order until
explicitly resolved. They never trigger an implicit database read.

### Canonical cache contract

The opened tagspace owns one bounded cache through its private backend, shared
across sessions and operations without a session. Access to a tagspace and its
sessions must be serialized, including reads; the backend and cache are not
thread-safe. Canonical ranks, independently loaded identity fields, complete
direct predicates, and not-found results are retained; storage failures are not
negative cache hits. Identity reads populate
the rank without loading it separately. Rank-only requests never load identity
fields or predicates. GET reconstructs URLs and applies contextual substitution
and synthetic `_refers_to` markers after loading canonical data. An unloaded
predicate component differs from a complete empty set. Inherited GET retains
its recursive SQL query and nearest-ancestor masking; inherited and query-selected
predicates never populate the direct-predicate cache.
Direct predicates are retained as compact canonical rows of relator, object,
and literal modifier text, matching the current storage schema. No indexed
`predicate_set`, contextual labels, or private ordering allocations are retained
in these rows. Each returned set constructs its own indexes and resolves ranks
within the current read snapshot. This keeps retained storage smaller, at the
cost of rebuilding returned sets on warm reads. Identity, predicate, and rank
components share one entry and one eviction policy; filling one component does
not discard another. Oversized growth can leave a smaller existing entry intact.
Direct rows require an admitted rank/identity entry; if no such entry fits, those
rows also bypass admission.

Hard-tag definitions bypass the mutable cache. A separate temporary map avoids
repeated lookups during an operation even when retained-cache admission is
disabled. Rank lookups use non-owning IDs until a stored key is needed. Referent
encoding also reuses successful and not-found substitutions within one read,
keyed by ID and the current context contents. These substitutions never enter
the canonical cache and are cleared at operation/mutation boundaries; failures
are not memoized. Returned values do not reference cache entries.

Default limits are 256 KiB of accounted storage, 1,024 entries, and five minutes
of idle time. Least-recently-used entries are removed on admission pressure;
oversized entries bypass admission. A monotonic clock expires idle entries
during later reads, including membership and part-of-speech checks. Expiry removes
entries from the least-recently-used end until it reaches an unexpired entry,
using the monotonic last-use ordering. A tree index
locates retained entries without scanning the LRU list; its storage is included
in the byte budget, together with entry objects, list links, identity/rank string
capacities, predicate vector capacity, and each row's string capacities. Counting
string capacities also for inline strings is conservative. Allocator bookkeeping
is implementation-dependent and is not measured; the budget bounds accounted
retained storage, not process RSS or temporary returned-value allocations. There
are no shared predicate allocations requiring split ownership accounting. Complete
inactivity needs no worker thread. Erasure destroys
entry allocations; allocator reuse, not a guaranteed reduction in process RSS,
is the memory-reclamation contract.

Every potentially mutating operation clears all cached components on entry and exit,
including errors and rollback paths. Internal mutation reads cannot publish
cache entries. Close/reopen releases entries.

### SQLite snapshots and diagnostics

Raw backend SQL also invalidates, including schema changes that do not change
SQLite's row counter. Reads pin a
SQLite transaction snapshot and check connection-local changes and `data_version`
around it, invalidating after external commits. Snapshot/version SQL statements
are prepared once per connection; boundary resets release only active cursors.
If releasing an owned read transaction fails, the backend records an error and
closes storage, preventing later writes from entering the abandoned transaction.
Cleanup runs in the read-scope destructor, so its error is recorded on the backend
rather than replacing an already evaluated return code. Explicit caller-owned transactions bypass
retained caching. Session context changes and target switching cannot reuse
another tagspace's ranks.

Warm GETs still perform transaction/version checks, term classification,
caller-specific reconstruction, and predicate ordering. Cache metrics distinguish
identity, predicate, and rank reads/hits by name. Referent read counters count SQL
queries per context level; referent hits count substitutions reused within a read.
Admission can be disabled for comparison without disabling per-operation rank
reuse. No public caller needs cache-management operations.

## Deferred work

New topological views, runtime tagspace selection in TAGL, tagspace-prefixed URLs,
alternate backend configuration, and filepile storage are deferred. No PUT
declaration of the named tagspace is created by initialization.
