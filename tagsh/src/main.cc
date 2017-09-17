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
#include "tagdb/sqlite.h"
#include "tagsh.h"

int main(int argc, char **argv) {
	cmd_args args;
	args.parse(argc, argv);

	if ( args.has_errors() ) {
		args.print_errors();
		return args.code();
	}

	tagdb_type tdb;
	if ( tdb.init(args.db_fname) != tagd::TAGD_OK ) {
		tdb.print_errors();
		return tdb.code();
	}

	tagsh shell(&tdb);

	if (args.tagl_file_statements.size() == 0) {
		std::cout << "use .show to list commands" << std::endl;
		return shell.interpret_readline();
	} else {
		return args.interpret(shell);
	}

	return 0;
}
