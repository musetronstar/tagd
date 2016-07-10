#include "tagspace/sqlite.h"
#include "httagd.h"

int main(int argc, char ** argv) {
	httagd::httagd_args  args;
	args.parse(argc, argv);

	if (args.has_error()) {
		args.print_errors();
		return args.code();
	}

	tagspace::sqlite TS;
	if (TS.init(args.db_fname) != tagd::TAGD_OK) {
		TS.print_errors();
		return TS.code();
	}

	httagd::viewspace VS(args.tpl_dir);
	init_viewspace(VS);
	if (VS.has_error()) {
		VS.print_errors();
		return VS.code();
	}

	httagd::server svr(&TS, &VS, &args);
	svr.start();

	if (svr.has_error())
		svr.print_errors();

	return svr.code();
}

