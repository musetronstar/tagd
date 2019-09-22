#pragma once

#include <functional>

#include "tagl.h"
#include "tagdb/sqlite.h"

// TODO use a template
typedef tagdb::sqlite tagdb_type;
typedef std::vector<char *> cmdlines_t;

class tagsh;

class tagsh_callback : public TAGL::callback {
		friend class tagsh;
		tagdb_type *_tdb;
		tagsh *_tsh;
		cmdlines_t _lines;

	public:
		tagsh_callback(tagdb_type *tdb, tagsh *tsh) : _tdb{tdb}, _tsh{tsh} {}
		~tagsh_callback() { for(auto l : _lines) delete l; }

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();

		void handle_cmd_error();
};

class tagsh {
	protected:
		tagdb_type *_tdb;
		tagsh_callback *_callback;
		bool _own_callback = false;  // true if this alloced the callback pointer
		TAGL::driver _driver;

	public:
		static constexpr const char* DEFAULT_PROMPT = "tagd> ";
		std::string prompt = DEFAULT_PROMPT;
		bool echo_result_code = true;

		tagsh(tagdb_type *tdb, tagsh_callback *cb) :
			_tdb{tdb}, _callback{cb}, _driver(tdb, cb)
		{
			_driver.own_session(tdb->new_session());
		}

		tagsh(tagdb_type *tdb) :
			_tdb{tdb}, _callback{new tagsh_callback(_tdb, this)}, _own_callback{true},
			_driver(_tdb, _callback)
		{
			_driver.own_session(tdb->new_session());
		}

		~tagsh() {
			if (_own_callback)
				delete _callback;
		}
	
		void trace_on() {
			_tdb->trace_on();  // specific to sqlite
			_driver.trace_on("trace: ");
		}

		void command(const std::string&);
		int interpret_readline();
		int interpret(std::istream&);
		int interpret(const std::string&);  // tagl statement
		int interpret_fname(const std::string&);  // filename
		void dump() { _tdb->dump(); }
		void dump_file(const std::string&, bool = true);
		static int error(const char *errfmt, ...);
		static bool file_exists(const std::string&);
		static void cmd_show();
};

typedef std::function<void(char *)> cmd_arg_handler_t;
struct cmd_handler {
	cmd_arg_handler_t handler;
	bool has_arg;
};
typedef std::map< std::string, cmd_handler > cmd_handler_map_t;

class cmd_args : public tagd::errorable {
	protected:
		cmd_handler_map_t _cmds;

	public:
		std::vector<std::string> tagl_statements;
		std::string db_fname;
		bool opt_db_create = false;
		bool opt_trace = false;
		bool opt_noshell = false;
		bool opt_dump = false;

		cmd_args();
		void parse(int, char **); 

		int interpret(tagsh&);
};
