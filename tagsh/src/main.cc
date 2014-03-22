#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <event2/buffer.h>
#include "tagd.h"
#include "tagl.h"
#include "tagspace/sqlite.h"
#include "tagsh.h"


int main(int argc, char **argv) {
	typedef std::vector<std::string> str_vec_t; 
	str_vec_t tagl_files;
	std::string db_fname, tagl_statement;
	bool opt_create = false;
	bool opt_trace = false;

	for(int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--db") == 0) {
			if (++i < argc) {
				if (argv[i][0] == '-')
					db_fname = ":memory:";
				else {
					if (!opt_create && !tagsh::file_exists(argv[i]))
						return tagsh::error("no such file: %s", argv[i]);
					db_fname = argv[i];
				}
			} else {
				return tagsh::error("usage: --db <database file>");
			}
		} else if (strcmp(argv[i], "--trace_on") == 0) {
			opt_trace = true;
		} else if (strcmp(argv[i], "--create") == 0) {
			opt_create = true;
		} else if (strcmp(argv[i], "--tagl") == 0) {
			if (++i <= argc)
				tagl_statement = argv[i];
		} else {

			tagl_files.push_back(argv[i]);
		}
	}

	if (db_fname.empty())
		db_fname = tagspace::util::user_db();

	space_type TS;
	if ( TS.init(db_fname) != tagd::TAGD_OK ) {
		TS.print_errors();
		return 1;
	}

	tagsh_callback CB(&TS);
	tagsh shell(&TS,&CB);

	if (opt_trace) {
		shell.interpret(".trace_on");
	}

	if (tagl_statement.empty() && tagl_files.size() == 0) {
		std::cout << "use .show to list commands" << std::endl;
		return shell.interpret(std::cin);
	}

	int err;
	if (!tagl_statement.empty()) {
		shell.prompt.clear();
		err = shell.interpret(tagl_statement);
		if (err) return err;
	}
	
	// input file(s) 
	for(str_vec_t::iterator it = tagl_files.begin(); it != tagl_files.end(); ++it) {
		if (*it == "-") {  // fname == "-", use interpret stdin
			// TODO create an evbuffer from stdin, so shell commands are not exposed
			shell.prompt.clear();
			err = shell.interpret(std::cin);
		} else {
			err = shell.interpret_fname(*it);
		}

		if ( err ) {
			// err already printed
			return err;
		}
	}

	return 0;
}
