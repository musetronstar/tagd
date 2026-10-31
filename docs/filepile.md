# Filepile storage

## Invocation

Filepile stores local regular files in the active persistent tagspace. All
storage calls must run sequentially in one process. Initially, only one persistent
tagspace may participate in a filepile; references within it can share bytes.

```sh
tagsh/bin/tagsh --home /data/tagd --tagspace Photos --create --noshell
tagsh/bin/tagsh --home /data/tagd --tagspace Photos \
  --tagl '>>+ file:///photos/a.jpg;' --noshell
```

* `>>+ URL ...;` uses PUT grammar and applies its predicates to source A in the
  same semantic transaction as storage. The subject must be `POS_URL`.
* Interactive input, scripts, includes, `--tagl`, and redirected input all use
  the same synchronous command and local process permissions. Actions execute
  in order and stop at the first failure.
* Successful stores print the retained storage-link ID. Errors and advisory
  warnings are TAGL error objects on stderr; warnings do not fail the command.
* `!!+ URL;` deletes managed storage and its selected records. Predicate-bearing
  `!!+` uses ordinary DELETE semantics and keeps the subject and stored bytes.
* `.store` is removed. `--store FILE_OR_URL...` queues the same TAGL STORE
  command for each operand. Shell-expanded globs supply multiple operands;
  tagsh does not expand patterns itself.
* `--with 'PREDICATES'` applies to every source in the immediately preceding
  `--store` group. Use it once per group, before any other option. Predicates
  and type relations are part of each STORE, not a separate PUT after storage.
  Referenced tags and types must exist; earlier TAGL actions may declare them.
* `--with` accepts a nonempty predicate fragment without a command or terminator.
  Quoted modifiers may contain punctuation and command-like text. `--tagl`
  continues to accept complete, independent TAGL commands.
* Options end a store operand list. Prefix filenames starting with `-` with `./`.
  Relative paths are made absolute and URL-encoded. Symlink spelling is retained
  unless `..` requires filesystem resolution to select the correct source.
* Each source is an independent STORE. Failure stops subsequent files/actions;
  earlier successful stores stay committed. Duplicate settings, link-ID output,
  warnings, and exit status are shared with explicit `>>+` commands.
* HTTP acquisition and remote invocation remain separate work.

```sh
tagsh/bin/tagsh --home /data/tagd --tagspace Photos \
  --store "holiday photo.jpg" file:///photos/another.jpg \
  --with '_has _message = "happy holidays"' --noshell

tagsh/bin/tagsh --home /data/tagd --tagspace Photos \
  --tagl '>> tags _sub _entity;' \
  --store file:///photos/beach.jpg --with '_has tags = "beach, friends"' --noshell
```

## Public C++ interface

```cpp
tagd::store_result result;
auto rc = space.store(result, tagd::url("file:///photos/a.jpg"), session, flags);
```

* `space` is a `tagd::tagspace`; the backend remains private.
* `result.source`, `result.stored`, `result.link`, and `result.revision` identify
  A, B, L, and the per-source content revision.
* `result.outcome` is `new_revision` or `unchanged`; failures other than
  `TS_DUPLICATE` clear output to `none`.
* `result.warnings` contains advisory `tagd::error` objects. Library code does
  not print them.
* Only a command changing neither the PUT declaration nor latest stored bytes
  returns `TS_DUPLICATE`, or `TAGD_OK` with `F_IGNORE_DUPLICATES`. Both return the
  existing link after verification. A predicate-only change succeeds without a
  new revision; a byte-only change succeeds even when A already exists.
* `result.changed` distinguishes successful state changes from ignored duplicates.
  `result.commit_unknown` reports an unconfirmed semantic outcome.
  User predicates and type relations supplied on the URL describe A.
* A new source or changed content returns `TAGD_OK`, including when bytes are
  reused. Returning to earlier content creates a new revision.

## Implementation layout

Filepile is an internal module under `tagspace/filepile/`, alongside
`tagspace/tagdb/`:

* `src/filepile.cc` implements semantic storage and revision history.
* `src/filepile-delete.cc` selects managed deletions and reports cleanup.
* `src/filepile-records.cc` validates associations, protects generated data, and logs events.
* `src/filepile-files.cc` and `include/filepile-files.h` implement acquisition,
  verification, publication, and descriptor-relative removal.
* `tests/` contains the semantic, filesystem-failure, and vocabulary suites.

`make -C tagspace/filepile` builds the private `lib/libfilepile.a` archive.
`make -C tagspace/filepile tests` builds the containing tagspace and runs these
suites. Normal tagspace build, test, debug, and clean targets include filepile.
Consumers continue using `tagd::tagspace::store()` and `libtagspace.a`, which
bundles filepile and backend objects.

Backend adapters now implement `inherited_sources` for a set of requested
ancestors in one snapshot; the single-source `inherits` helper delegates to it.
Filepile role guards use the batch operation. Effective type queries reuse the
relation resolver's existing source closure.

## Home and eligibility

* Default home: `~/.tagd`. `--home DIR` selects `DIR/filepile/`. Only one
  persistent tagspace may use that filepile in this implementation. Resolve
  relative homes and home symlinks once at configuration. STORE, association
  validation, and DELETE cleanup use that same canonical home; retargeting the
  configured symlink does not redirect subsequent operations.
* `--db` disables home lookup and therefore filepile storage, with a `TS_MISUSE`
  explanation. An unavailable home never falls back to another directory.
* `_config`, `_default`, and other memory tagspaces are volatile and return
  `TS_MISUSE`; loading or dumping TAGL does not make them persistent.
* Direct C++ named persistent storage retains its selected home. Configuration
  supplies the effective home independently of the database's own location.
* Filepile requires a backend supporting durable atomic PUT. SQLite verifies
  `synchronous=EXTRA` and rejects OFF/MEMORY journaling and read-only databases.
* Frontend write restrictions remain in force. No HTTP store endpoint is added.

## Source identity and acquisition

* Supply an existing `tagd::url`, not a bare path. Unsupported fetcher schemes
  return `TS_NOT_IMPLEMENTED`; malformed URLs retain URL validation errors.
* Local URLs require `file://` with an absolute path and empty or `localhost`
  authority. Scheme and localhost comparison are case-insensitive.
* Reject raw whitespace, controls, non-ASCII, backslashes, query/fragment syntax,
  credentials, ports, remote hosts, malformed escapes, dot/empty components,
  encoded separators/controls, invalid UTF-8, and Windows drive forms.
* Decode once per segment; canonicalize using unreserved ASCII and uppercase
  percent escapes. Do not normalize Unicode or resolve source symlinks for A.
* Open once, require a regular file, and copy through that descriptor. Follow
  source symlinks, but retain the supplied path's canonical URL as A.
* Copy the initial size with bounded memory, check EOF and descriptor snapshots,
  then check the path still names the same inode before publication.
* No arbitrary size cap is imposed. Checked reads, writes, and syncs propagate
  failures. Change detection cannot guarantee an immutable snapshot against an
  external writer, especially on coarse-timestamp filesystems.
* Filesystem access uses the local process's permissions. Remote authorization
  and confinement require separate design before exposing acquisition remotely.

## Content and metadata

The physical layout is `<home>/filepile/<hash[0:2]>/<hash[2:4]>/<hash>.<ext>`.
Hashing uses SHA-256 over every copied byte. Detection examines the complete first
64 KiB through libmagic; its MIME result is advisory. The offline Perl/gperf table
selects the extension, with `bin` as fallback. Existing verified suffixes remain
unchanged even when detection or mapping changes.

| Record | Identity/type | Explicit metadata |
| --- | --- | --- |
| Source A | Canonical source URL, `_file` | Existing URL component predicates |
| Link L | `link:<complete-source-HDURI>#001`, `_link:filepile` | `_from A`, `_to B`, `_has _revision`, source `_mtime`, `_stored_time` |
| Stored B | Canonical URL (`_sub _file`), `_type_of _file:stored` | `_has _hash:SHA-256`, `_file_size`, `_media_type`, physical-object `_mtime` |

* Each source has independent revisions within each tagspace, from `001` to
  `999`. A new revision is the highest retained number plus one, or `001`
  if none remain. Deleted numbers may be reused; lower gaps are not filled.
  Revision 1000 fails; unchanged content at 999 remains a no-op.
* Ordinary tag IDs allow 8192 bytes; URL parsing retains its 2112-byte limit.
  HDURI encoding has a separate expansion bound and decodes back to that URL.
  Oversized generated link IDs return `TS_ERR_MAX_TAG_LEN`; nothing is truncated.
* B metadata is captured only when first recorded in a tagspace. Reuse and
  touching A do not refresh metadata or change final bytes/mtime.
* Timestamps use shared UTC formatting with nine fractional digits:
  `2026-10-23T14:30:12.123456789Z`. Session/event generation uses the same format.
* Dumps retain B's type classification, A/B/L associations, and metadata.
  Ordinary loading cannot recreate protected filepile records until explicit
  restore mode is implemented. Old `_is_a _file:stored` declarations are
  incompatible with this pre-beta format; no migration is provided.

HDURI escapes `%` as `%25` and `!` as `%21`, independently of file URL encoding.
For example, source `file:///tmp/a%21b` has link ID
`link:hd:!!!/tmp/a%2521b!!!!!!file#001`. A literal filename `a%21b` instead has
source `file:///tmp/a%2521b` and link ID
`link:hd:!!!/tmp/a%252521b!!!!!!file#001`; the two histories stay distinct.

This is a pre-beta encoding break. Recreate affected URL records, saved HDURIs,
and filepile link identifiers from authoritative source identities. No version
marker or migration is provided, and physical content is not deleted or renamed
by the encoding change. See [HDURI identity encoding](TAGL-spec.md#hduri-identity-encoding).

## Integrity and failures

* Managed directories are opened relative to checked directory descriptors;
  `filepile`, `.staging`, and fanout components cannot be symlinks.
* New directories request mode `0700`; exclusively created random staging files
  request `0600`. A restrictive umask is respected; existing modes are unchanged.
* Existing content is opened nonblocking with `O_NOFOLLOW`, required to be
  regular, and fully hashed before reuse, including duplicate results.
* Missing required content, wrong hashes, symlinks, and multiple matching
  filenames fail without repair. Relevant recorded addresses cannot be replaced
  silently with an alternate suffix. Unrelated historical bytes are not scanned.
* New bytes are fsynced, published with no-overwrite `linkat`, and directory
  entries synced before A/B/L commit atomically. Reused content also receives
  required durability checks before new semantic references.
* Owned staging is removed on normal failure; cleanup errors are additional
  diagnostics. Complete published content is never deleted as rollback.
* A failed commit or crash can leave unreferenced complete bytes. Unknown commit
  outcomes return `TS_INTERNAL_ERR` and block further writes through that handle
  until reopen/check. No automatic retry or crash-recovery sweep is performed.

## DELETE STORAGE (`!!+`)

```tagl
!!+ file:///photos/a.jpg;
```

* A source A deletes A and all its retained links. User relations on removed
  A, B, and L records are deleted with those records. B and its bytes remain when
  another source still has a link to B in this tagspace.
* A stored URL B deletes B and every link to B, preserving all source A tags.
  A URL with both roles fails with `TS_MISUSE`.
* A stored B without any links is damaged state: `!!+ B` returns `TS_ERR` and
  retains its record and bytes. Cleanup awaits the future role-restricted repair
  facility. A later STORE of identical content can reuse B and restore a link.
* A URL with neither a stored B role nor managed associations follows ordinary
  DELETE behavior, preserving that URL's ordinary identity and database durability settings. Managed
  sources accept the same local URL spellings as STORE, including `localhost` and
  equivalent percent encodings. Predicate-bearing forms delete only the requested
  predicates or type relations, subject to the
  generated-field guards. Bare non-URL subjects and subordinate relations are
  not managed deletion requests.
* Unrelated references to selected A/B/L records block the entire cascade with
  `TS_DEPENDENCY`. Event mentions are literal modifiers, so audit records remain
  intact without retaining the mentioned files. Original source files are never removed.
* Validate references, commit A/B/L deletion atomically, then remove eligible
  files. An unknown database commit prevents cleanup. An unavailable required
  database is `TS_NOT_AVAILABLE`; confirmed absence is `TS_NOT_FOUND`.
* Confirmed missing stored bytes produce advisory `TS_NOT_FOUND` errors and
  `WARNING` logs. The shell prints them to stderr with a warning label while an
  otherwise successful command returns `TAGD_OK`.
* Cleanup failure returns failure after the tagspace deletion has committed.
  Results distinguish removed, retained, already-missing, failed, and
  not-attempted content. A directory-sync failure after unlink reports removed
  bytes with failed durability; it does not claim that the file still exists.
* Malformed association endpoints, revision values, identities, or B addresses
  in the selected history or shared-content references fail validation. Unrelated
  associations are not loaded or validated. Managed-path symlinks cannot redirect
  physical cleanup.
  Repair and cross-tagspace reference discovery remain separate work.

C++ callers use the same declaration contract:

```cpp
tagd::filepile_delete_result result;
auto rc = space.delete_storage(result, tagd::url("file:///photos/a.jpg"), session, flags);
```

`result.committed` and `result.commit_unknown` describe the semantic outcome.
`result.content` contains each affected stored URL, cleanup outcome and status;
`result.warnings` contains advisory errors separately from the command status.
Pass a URL containing only its identity for whole-subject deletion. Passing a
fetched record's predicates requests predicate deletion, just as in TAGL.

## Deferred work and practical limits

* No concurrent writers, permissions/roles, cancellation protocol, automatic
  garbage collection, reconciliation, repair, or verification cache is implemented.
* The tagspace layer protects generated URL parts, B metadata and classification,
  and L identity, endpoints, revision and timestamps. Ordinary PUT cannot create
  managed records or classify through a subtype of `_file:stored`. PUT, STORE,
  and REPLACE accept user relations on existing A, B, L, and event records.
  Supplied system facts or filepile classes are `TS_MISUSE`, even unchanged;
  identity may be restated but not changed. Self and `_entity` typing are
  validated no-ops. An unrecorded URL inside filepile is `TS_MISUSE`.
  Ordinary PUT compares the decoded, lexically normalized path with the
  configured and canonical filepile roots, cached when the home is configured.
  It does not inspect the resource.
  If home resolution failed, ordinary PUT still checks the configured root;
  storage operations report the configuration error.
  An outside symlink alias remains an ordinary URL identifier; STORE resolves
  aliases and rejects any source inside filepile. STORE predicates describe A;
  B cannot be a source. Ordinary deletion of A with links, or of L/B, is
  `TS_DEPENDENCY`. The previous `TS_RELATION_DEPENDENCY` name is removed.
* Filepile events use `_event:filepile_store` or `_event:filepile_delete`, with
  `_has _message` to record
  the requested URL, affected A/B/L mappings, actual result code, and changes.
  Complete TAGL event descriptions use the existing logger and its filters;
  events are not automatically inserted into the selected tagspace. Deletion
  retains one initiating event identity and occurrence time through final cleanup
  reporting; it never emits a separate cleanup event.
* Absolute B URLs disclose paths and require explicit reconciliation after home
  relocation. Case-insensitive path aliases may retain separate histories.
* History and recorded-content selection use backend predicate queries. The current SQLite
  plan scans relations despite existing indexes; measured tuning is deferred.
* A duplicate may incur source read, temporary write, and stored-object read.
  This implementation prioritizes verified content over avoiding that I/O.

## Validation

* `make tests` includes URL, copying, injected I/O/commit failures, semantic store,
  canonical dumps, protected-record guards, managed deletion and cleanup failures,
  revision reuse, and tagsh invocation/ordering tests.
* `make -C tagd/tests magic_evaluation` reports expected/detected MIME values at
  8/16/32/64/128/256 KiB, false positives/negatives, libmagic version, and database
  digest. A late signature fixture demonstrates the 64 KiB limit explicitly.
* Unit tests validate failure handling, not hardware power-loss guarantees.
