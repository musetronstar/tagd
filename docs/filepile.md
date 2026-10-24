# Filepile storage

## Invocation

Filepile stores local regular files in the active persistent tagspace. All
storage calls sharing a home must run sequentially in one process.

```sh
tagsh/bin/tagsh --home /data/tagd --tagspace Photos --create --noshell
tagsh/bin/tagsh --home /data/tagd --tagspace Photos \
  --store 'file:///photos/a.jpg' --noshell
```

* `--store URL` runs in order with `--tagl` and `--file` actions. Errors stop the
  remaining actions; a completed store survives a subsequent TAGL error.
* `.store URL` is available in the interactive terminal loop. Scripts, includes,
  `--tagl`, and redirected input cannot invoke it; use explicit `--store` actions.
* Successful stores print the storage-link ID. Errors and empty-file warnings
  are TAGL error objects on stderr; warnings do not make the operation fail.
* `--store` does not imply `--noshell` or change the subject of subsequent TAGL.
  Annotate A, B, or L by naming that subject explicitly in a PUT.
* Native TAGL `STORE` and HTTP fetching are not implemented.

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
* Unchanged latest bytes return `TS_DUPLICATE`, or `TAGD_OK` with
  `F_IGNORE_DUPLICATES`. Either returns the existing result after verification.
* A new source or changed content returns `TAGD_OK`, including when bytes are
  reused. Returning to earlier content creates a new revision.

## Implementation layout

Filepile is an internal module under `tagspace/filepile/`, alongside
`tagspace/tagdb/`:

* `src/filepile.cc` implements semantic storage and revision history.
* `src/filepile-files.cc` and `include/filepile-files.h` implement acquisition,
  verification, and publication.
* `tests/` contains the semantic, filesystem-failure, and vocabulary suites.

`make -C tagspace/filepile` builds the private `lib/libfilepile.a` archive.
`make -C tagspace/filepile tests` builds the containing tagspace and runs these
suites. Normal tagspace build, test, debug, and clean targets include filepile.
Consumers continue using `tagd::tagspace::store()` and `libtagspace.a`, which
bundles filepile and backend objects.

## Home and eligibility

* Default home: `~/.tagd`. `--home DIR` selects `DIR/filepile/`, shared by its
  tagspaces. Resolve relative homes and home symlinks to the physical root.
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
| Stored B | Canonical physical URL, `_file:stored` | `_has _hash:SHA-256`, `_file_size`, `_media_type`, physical-object `_mtime` |

* Each source has independent revisions within each tagspace, from `001` to
  `999`. Revision 1000 fails; unchanged content at 999 remains a no-op.
* Ordinary tag IDs allow 8192 bytes; URL parsing retains its 2112-byte limit.
  HDURI encoding has a separate expansion bound and decodes back to that URL.
  Oversized generated link IDs return `TS_ERR_MAX_TAG_LEN`; nothing is truncated.
* B metadata is captured only when first recorded in a tagspace. Reuse and
  touching A do not refresh metadata or change final bytes/mtime.
* Timestamps use shared UTC formatting with nine fractional digits:
  `2026-10-23T14:30:12.123456789Z`. Session/event generation uses the same format.
* Explicit metadata survives dump/load. Dumps declare URL identities before
  dependent predicates. Existing timestamp values are not rewritten.

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

## Deferred work and practical limits

* No concurrent writers, permissions/roles, cancellation protocol, history
  deletion, reconciliation, repair, or verification cache is implemented.
* Links remain ordinarily editable; manual changes can damage history.
* Absolute B URLs disclose paths and require explicit reconciliation after home
  relocation. Case-insensitive path aliases may retain separate histories.
* History and recorded-content selection use backend predicate queries. The current SQLite
  plan scans relations despite existing indexes; measured tuning is deferred.
* A duplicate may incur source read, temporary write, and stored-object read.
  This implementation prioritizes verified content over avoiding that I/O.

## Validation

* `make tests` includes URL, copying, injected I/O/commit failures, semantic store,
  dump/load, and tagsh origin/ordering tests.
* `make -C tagd/tests magic_evaluation` reports expected/detected MIME values at
  8/16/32/64/128/256 KiB, false positives/negatives, libmagic version, and database
  digest. A late signature fixture demonstrates the 64 KiB limit explicitly.
* Unit tests validate failure handling, not hardware power-loss guarantees.
