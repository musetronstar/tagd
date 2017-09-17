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
   and calls main callback.
*  Main callback calls tagdurl scanner which send tokens through TAGL driver
   and parser.  TAGL driver calls the corresponding `tagl_callback` command
   method (`cmd_get`, `cmd_put`, or `cmd_del`) when finished.
* `tagl_callback`:
  * creates a session object to keep track of the tagdb, server,
    request, response, and context instances.
  * calls get, put, or del method on tagdb, passing it the object
    parsed from the tagdurl.
  * gets the view corresponding to the view `v=view_name`
    query parameter. The view returned is comprised of a handler and a template.
  * calls the handler which populates the template object.
  * expands the tempate object data into the template file.  Data is emitted into evbuffers directly.
  * adds evbuffers and response codes to response objects.
* HTTP server writes response and HTTP transaction completes.
