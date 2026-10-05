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
The tagspace must therefore remain alive until all its sessions are destroyed.
Copying a session copies its context and identity; it does not create a new
session identity or extend the tagspace object's lifetime.

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

## Ranks and deferred work

Ranks encode ancestry: an ancestor's rank is a prefix of a descendant's rank.
`_entity` has the empty root rank and contains every existing tag; an unknown ID
is never treated as a root. User ranks extend the hard-tag hierarchy without
reusing the ranks of hard tags. Predicates are ordered by the ranks of their
relators and objects when available, with ID text used for unresolved values.

The shared implementations of `contains()` and `rank_comparator()` belong to
`const_tagspace`. They ask the derived class for tags and ranks, so the same
rules apply whether those tags come from generated hard-tag data or storage.

New topological views, runtime tagspace selection in TAGL, tagspace-prefixed URLs,
alternate backend configuration, and filepile storage are deferred. No PUT
declaration of the named tagspace is created by initialization.
