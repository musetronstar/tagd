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

	// Config and selected storage outlive the shell and all borrowed sessions.
	tagd::config cfg;
	auto rc = load_config(cfg, args);
	if (rc != tagd::TAGD_OK) {
		cfg.print_errors();
		return rc;
	}
	install_loggers(cfg);
	tagsh shell(&cfg, cfg);
	rc = shell.select_startup(args);
	if (rc != tagd::TAGD_OK) {
		cfg.print_errors();
		return rc;
	}
	cfg.start_runtime();
	int result = execute_startup_actions(shell, args.actions());
	if (result)
		return result;
	if (args.dump())
		return shell.tsp()->dump();
	if (!args.noshell()) {
		TAGD_COUT << "use .show to list commands" << std::endl;
		return shell.interpret_readline();
	}
	return 0;
}
