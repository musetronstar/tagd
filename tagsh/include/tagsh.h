#pragma once

#include <functional>

#include "tagl.h"
#include "tagspace/sqlite.h"

// TODO use a template
typedef tagspace::sqlite space_type;
typedef std::vector<char *> cmdlines_t;

class tagsh;

class tagsh_callback : public TAGL::callback {
		friend class tagsh;
		space_type *_TS;
		cmdlines_t _lines;

	public:
		tagsh_callback(space_type*);
		~tagsh_callback() { for(auto l : _lines) delete l; }
		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_del(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void cmd_error();
};

class tagsh {
	protected:
		space_type *_TS;
		tagsh_callback *_CB;
		bool _own_callback;  // true if this own's the callback pointer
		TAGL::driver _driver;

	public:
		std::string prompt;
		tagsh(space_type *ts, tagsh_callback *cb) :
			_TS(ts), _CB(cb), _own_callback{false}, _driver(ts, cb), prompt("tagd> ")
		{}

		tagsh(space_type *ts) :
			_TS(ts), _CB{ new tagsh_callback(_TS) }, _own_callback{true},
			_driver(_TS, _CB), prompt("tagd> ")
		{}

		~tagsh() {
			if (_own_callback)
				delete _CB;
		}
		

		void command(const std::string&);
		int interpret_readline();
		int interpret(std::istream&);
		int interpret(const std::string&);  // tagl statement
		int interpret_fname(const std::string&);  // filename
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
		typedef std::vector<std::string> str_vec_t; 
		str_vec_t tagl_file_statements;
		std::string db_fname;
		bool opt_create;
		bool opt_trace;

		cmd_args();
		void parse(int, char **); 

		int interpret(tagsh&);
};


