#include "tagdb/sqlite.h"
#include "tagsh.h"

int main(int argc, char **argv) {
	tagdb::hard_tag::install_logger_validator();

	cmd_args args;
	args.parse(argc, argv);

	if (args.has_errors()) {
		args.print_errors();
		return args.code();
	}

	tagdb::sqlite tdb;
	if (tdb.init(args.db_fname) != tagd::TAGD_OK) {
		tdb.print_errors();
		return tdb.code();
	}

	tagsh shell(&tdb);
	return args.interpret(shell);
}
