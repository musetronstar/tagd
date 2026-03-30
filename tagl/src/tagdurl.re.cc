#include "tagl.h"

/*!include:re2c "../include/scanner.h" */

namespace TAGL {

void scanner::tagdurl::scan(int cmd, const std::string& path) {
	if (path.empty() || path == "/") {
		_driver->error(tagd::TAGL_ERR, "tag id required in path");
		return;
	}

	if (path[0] != '/') {
		_driver->error(tagd::TAGL_ERR, "malformed path: no leading '/'");
		return;
	}

	if (cmd == TOK_CMD_GET) {
		_driver->cmd(cmd);
		_driver->parse_tok(TOK_CMD_GET, nullptr);
		this->scanner::scan(tagd::uri_decode(path.substr(1)));
	} else {
		_driver->error(tagd::TS_MISUSE, "unhandled command");
	}
}

void scanner::tagdurl::scan(tagd::http_method method, const std::string& path) {
	this->scan(method_command(method), path);
}

void scanner::tagdurl::scan(const char *cur, size_t sz) {
	begin_scan(cur, sz);
	if (_driver->has_errors())
		return;

	(void)cur;
	(void)sz;

#define YYCURSOR _cur
#define YYLIMIT	_lim
#define YYGETSTATE()    _state
#define YYSETSTATE(x)   { _state = (x);  }
#define	YYFILL(n)	{ if(_do_fill && _evbuf && !_eof){ this->fill(); if(_driver->has_errors()) return; } }
#define YYMARKER        _mark
#define YYDEBUG(s,c) { if(TAGL_TRACE_ON) LOG_DEBUG("yydebug: s = " << s << ", c = " << c << std::endl) }

next:
	if (_driver->has_errors())
		return;

/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	!use:tagl_root;
*/

comment:
/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	!use:tagl_comment;
*/

block_comment:
/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	!use:tagl_block_comment;
*/

quoted_str:
/*!re2c
	re2c:define:YYCTYPE  = "char";
	re2c:yyfill:enable   = 1;

	!use:tagl_quoted;
*/
}

} // namespace TAGL
