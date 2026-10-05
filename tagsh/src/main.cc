#include "tagspace.h"
#include "tagsh.h"

int main(int argc, char **argv) {
	tagd::tagspace_install_logger_validator();

	cmd_args args;
	args.parse(argc, argv);

	if (args.has_errors()) {
		args.print_errors();
		return args.code();
	}

	/* The tagspace is constructed first so it outlives the shell and its sessions. */
	tagd::tagspace::memory memory;
	tagd::tagspace::persistent persistent;
	tagd::tagspace& tdb = args.tagspace_name.empty()
		? static_cast<tagd::tagspace&>(memory) : static_cast<tagd::tagspace&>(persistent);
	tagd::code rc;
	if (args.tagspace_name.empty()) {
		rc = memory.init();
	} else if (args.opt_create) {
		rc = args.tagd_home.empty() ? persistent.create(args.tagspace_name)
			: persistent.create(args.tagspace_name, args.tagd_home);
	} else {
		rc = args.tagd_home.empty() ? persistent.init(args.tagspace_name)
			: persistent.init(args.tagspace_name, args.tagd_home);
	}
	if (rc != tagd::TAGD_OK) {
		tdb.print_errors();
		return rc;
	}

	tagsh shell(&tdb);
	return args.interpret(shell);
}
