httagd architecture
-------------------
 (in terms of MVC)

     +-----------+     +--------------------+
     |  (model)  |     |        view        |
     |-----------|     |--------------------|
     |   tagspace   |     | handler | template |
     +-----------+     +--------------------+
         ^     \              ^      /
          \     \            /      /
           \     v          /      v
        +-------------------------------+
        |   tagl_callback (controller)  |
        |-------------------------------|
        |  route view   | call handler  |
        |---------------|---------------|
        |  tagspace CRUD   |expand template|
        |---------------|---------------|
        | http session  | add response  |
        +-------------------------------+
             ^                    |
             |                    v
        +-------------------------------+
        | scan/parse    |    write      |
        | method and    |   response    |
        | tagdurl       |               |
        |-------------------------------|
        |          HTTP server          |
        +-------------------------------+
             ^                    |
             |                    v
        +---------+          +----------+
        | request |          | response |
        +---------+          +----------+

*  HTTP server recieves incoming request, parses headers and URL,
   and calls `main_callback`.
*  `main_callback` calls the driver which scans the `tagdurl` sending
   tokens through the parser.  `TAGL_driver` calls the corresponding
   `tagl_callback` command
   method (`cmd_get`, `cmd_put`, or `cmd_delete`) when finished.
* `main_callback` creates a `transaction` linking the server, request, response,
  tagspace, and driver. It gets a `tagspace_session` from the tagspace for
  referent context and event identity during this request.
* `tagl_callback`:
  * Resolves `?v=view_name` and the action before calling the tagspace.
    Unknown view names fail with `TS_NOT_FOUND` without changing stored tags.
  * Calls `get`, `put`, `del`, or `query` on the tagspace, passing it the object
    parsed from the tagdurl.
  * Calls the `handler` which populates the `template` object.
  * Expands the tempate object data into the template file.
    Expanded output is emitted into response evbuffers directly.
  * Adds `response codes` to response objects.
* HTTP server writes `response` and the HTTP `transaction` completes.


Target dispatch uses the shared TAGL tagdurl parser. The first segment resolves
through the config-owned collection into a request-local selection; the remaining
path translates into the existing GET, PUT, DELETE, or QUERY operation. Description
GET/HEAD operates in config, subject to its access policy. The server default
remains fixed, and special file, favicon, and root routes precede target dispatch.


View contract
-------------

`include/views.h` declares view IDs, actions, typed handlers, owning `view` values,
and the `viewspace` registry. `src/views.cc` implements resolution, configuration
validation, and built-in error rendering. `httagd.h` keeps the transaction and
HTTP callback contract. A view uses `std::variant` for handler ownership;
constructors derive both the ID action and the active alternative from the handler
type. Pass-through calls use `std::get_if` and return `TAGD_ERR` on a mismatch.

| Selection | Representation |
| --- | --- |
| No `?v=` | `_config:default_view`, or `tagl` when unset |
| `tagl` | Built-in TAGL; this name cannot be registered by an app |
| Registered name with the requested action | Registered handler |
| Registered name without the requested action | Built-in TAGL |
| Unknown name | `TS_NOT_FOUND` (HTTP 404) |

GET, QUERY, PUT, and DELETE handlers run only after their tagspace operation
succeeds. PUT/DELETE handlers receive the parsed subject and predicates. Built-in
PUT/DELETE responses have an empty body on success. A rendering failure uses
the error path but cannot roll back the completed storage operation. App-specific
write handlers and templates remain separate frontend work.

The registry checks whether any action exists under a name. Its resolver returns
an `UNDEF` view to select built-in TAGL; it never invokes an undefined handler.
Root GET/HEAD requests without a search use EMPTY. The built-in EMPTY body is
`This is tagd`; an app may supply a home page. Context validation still applies.
Existing `?q=` queries keep their tagdurl semantics; root `?s=` is unsupported
and returns `TS_MISUSE`. HEAD retains GET metadata and suppresses the body.

Errors use the requested view family's ERROR handler (or the configured default
family when `?v=` is absent), then `_config:fallback_error_view`, then built-in
TAGL. This selection is independent of a missing action's fallback: a family
without a PUT handler may still render PUT failures through its ERROR handler.
Built-in `tagl` supplies its own ERROR representation.
The callback renders errors once per response: command callbacks and the driver's
later error callback share one error list and must not append it twice.
A valid QUERY with no matches returns an empty set and `TS_NOT_FOUND` without
an error diagnostic. Built-in TAGL emits an empty body; app QUERY handlers receive
the empty set. A standalone empty query keeps HTTP 404. It does not add shared
errors or stop a TAGL batch; a later successful command can finish the batch with
HTTP 200. Genuine query failures still use ERROR handlers.

View configuration is read from `tagd::config`, without a cached app fallback.
Startup checks require a registered default name and an ERROR handler for the
configured fallback; absent settings and `tagl` are valid. See
[configuration](../docs/configuration.md) and the optional
[app configuration](app/config.tagl), loaded explicitly with `--config`.

The callback API names DELETE as `cmd_delete` / `default_cmd_delete`; view APIs
use `view_action::DELETE`, `delete_handler_t`, and `delete_view_id`.
The storage API remains `tagspace::del()`.
