httagd architecture
-------------------
 (in terms of MVC)

     +-----------+     +--------------------+
     |  (model)  |     |        view        |
     |-----------|     |--------------------|
     |   tagdb   |     | handler | template |
     +-----------+     +--------------------+
         ^     \              ^      /
          \     \            /      /
           \     v          /      v
        +-------------------------------+
        |   tagl_callback (controller)  |
        |-------------------------------|
        |  route view   | call handler  |
        |---------------|---------------|
        |  tagdb CRUD   |expand template|
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
   method (`cmd_get`, `cmd_put`, or `cmd_del`) when finished.
* `tagl_callback`:
  * Creates a `session` object to keep track of the `tagdb`, `server`,
    `request`, `response`, and `context` instances.
  * Calls `get`, `put`, or `del` method on tagdb, passing it the object
    parsed from the tagdurl.
  * Gets the view corresponding to the view `v=view_name`
    query parameter. The view returned is comprised of a
    handler and a template.
  * Calls the `handler` which populates the `template` object.
  * Expands the tempate object data into the template file.
    Expanded output is emitted into response evbuffers directly.
  * Adds `response codes` to response objects.
* HTTP server writes `response` and the HTTP `transaction` completes.
