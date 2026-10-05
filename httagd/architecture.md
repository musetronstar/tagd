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
   method (`cmd_get`, `cmd_put`, or `cmd_del`) when finished.
* `main_callback` creates a `transaction` linking the server, request, response,
  tagspace, and driver. It gets a `tagspace_session` from the tagspace for
  referent context and event identity during this request.
* `tagl_callback`:
  * Calls `get`, `put`, or `del` method on tagspace, passing it the object
    parsed from the tagdurl.
  * Gets the view corresponding to the view `v=view_name`
    query parameter. The view returned is comprised of a
    handler and a template.
  * Calls the `handler` which populates the `template` object.
  * Expands the tempate object data into the template file.
    Expanded output is emitted into response evbuffers directly.
  * Adds `response codes` to response objects.
* HTTP server writes `response` and the HTTP `transaction` completes.
