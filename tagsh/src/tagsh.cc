#include "tagsh.h"

#include <filesystem>
#include <iostream>      // std::cout, std::cerr
#include <fstream>       // std::ofstream
#include <string>        // std::string
#include <vector>        // std::vector
#include <cstring>       // std::strcmp, std::strerror (if used)
#include <cstdlib>       // std::exit, EXIT_SUCCESS, perror

#include <readline/readline.h>  // readline()
#include <readline/history.h>   // add_history()

#include "tagd.h"
#include "tagl.h"
#include "tagspace.h"

static tagd::logger *TAGSH_LOGGER = nullptr;

static void TAGSH_SET_LOGGER(tagd::logger *log) {
	TAGSH_LOGGER = log;
}

static bool TAGSH_LOG_ENABLED(tagd::log_level lvl) {
	if (TAGSH_LOGGER == nullptr)
		return false;

	return static_cast<int>(lvl) <= static_cast<int>(TAGSH_LOGGER->level(std::string(HARD_TAG_ROLE_TAGSH)));
}

static void TAGSH_LOG(tagd::log_level lvl, const std::string& msg) {
	if (TAGSH_LOGGER == nullptr)
		return;

	TAGSH_LOGGER->log(std::string(HARD_TAG_ROLE_TAGSH), lvl, std::string("-- ").append(msg));
}

static void log_tagsh_notice(const char *fmt, const char *arg) {
	if (!TAGSH_LOG_ENABLED(tagd::log_level::NOTICE))
		return;

	char *msg = tagd::util::csprintf(fmt, arg);
	TAGSH_LOG(tagd::log_level::NOTICE,
			(msg == NULL ? "tagsh log formatting failed" : msg));
}

static void log_tagsh_notice(const char *fmt, const std::string& arg) {
	log_tagsh_notice(fmt, arg.c_str());
}

static void log_driver_debug(const char *fmt, const char *arg) {
	if (!TAGL_LOG_ENABLED(std::string(HARD_TAG_ROLE_DRIVER), tagd::log_level::DEBUG))
		return;

	char *msg = tagd::util::csprintf(fmt, arg);
	TAGL_LOG(std::string(HARD_TAG_ROLE_DRIVER), tagd::log_level::DEBUG,
			(msg == NULL ? "driver log formatting failed" : msg));
}

static void log_driver_debug(const char *fmt, const std::string& arg) {
	log_driver_debug(fmt, arg.c_str());
}

// TODO put in a utility library
int tagsh::error(const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	char *err = tagd::util::csprintf(errfmt, args);
	va_end (args);

	if (err == NULL)
		std::perror("error: ");
	else {
		if (TAGSH_LOG_ENABLED(tagd::log_level::ERROR)) {
			TAGSH_LOG(tagd::log_level::ERROR, err);
			TAGSH_LOG(tagd::log_level::ERROR,
					std::string("tagsh code=").append(tagd::code_str(tagd::TAGD_ERR)));
		}
		TAGD_CERR << err << std::endl;
	}

	return 1;
}

void add_history_lines_clear(cmdlines_t& lines) {
	std::string hst;
	for (auto l : lines) {
		hst.append(l);
		if (l != lines.back())
			hst.push_back('\n');
		free(l); // match readline's malloc
	}
	lines.clear();
	if (!hst.empty())
		add_history(hst.c_str());
}

void tagsh_callback::handle_cmd_error() {
	auto ssn = _driver->session_ptr();
	if (ssn && !ssn->ok()) {
		_driver->code(ssn->code()); // stops the scanner
		_tsh->last_code(_driver->code());
		ssn->print_errors();
		ssn->clear_errors();
	}

	if (!_tsp->ok()) {
		_driver->code(_tsp->code()); // stops the scanner
		_tsh->last_code(_driver->code());
		_tsp->print_errors();
		_tsp->clear_errors();
	}
}

#define CMD_OK() (_tsp->ok() && (!ssn || (ssn && ssn->ok())))

void tagsh_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag *T;
	auto ssn = _driver->session_ptr();
	log_driver_debug("driver statement command=CMD_GET subject=%s", t.id());
	log_driver_debug("driver callback=cmd_get subject=%s", t.id());

	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url();
		(void)_tsp->get((tagd::url&)*T, t.id(), ssn, _driver->flags);
	} else {
		T = new tagd::abstract_tag();
		(void)_tsp->get(*T, t.id(), ssn, _driver->flags);
	}

	if(CMD_OK())
		(*_tsh->out) << *T << std::endl;
	else
		this->handle_cmd_error();
	_tsh->last_code(_driver->code());
	log_driver_debug("driver code=%s", tagd::code_str(_driver->code()));

	delete T;

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_put(const tagd::abstract_tag& t) {
	auto ssn = _driver->session_ptr();
	log_driver_debug("driver statement command=CMD_PUT subject=%s", t.id());
	log_driver_debug("driver callback=cmd_put subject=%s", t.id());
	tagd::code tc = _tsp->put(t, ssn, _driver->flags);

	if (CMD_OK()) {
		if (_tsh->echo_result_code())
			(*_tsh->out) << "-- " << tagd::code_str(tc) << std::endl;
	} else {
		this->handle_cmd_error();
	}
	_tsh->last_code(CMD_OK() ? tc : _driver->code());
	log_driver_debug("driver code=%s", tagd::code_str(_driver->code()));

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_del(const tagd::abstract_tag& t) {
	auto ssn = _driver->session_ptr();
	log_driver_debug("driver statement command=CMD_DEL subject=%s", t.id());
	log_driver_debug("driver callback=cmd_del subject=%s", t.id());
	tagd::code tc = _tsp->del(t, ssn, _driver->flags);

	if (CMD_OK()) {
		if (_tsh->echo_result_code())
			(*_tsh->out) << "-- " << tagd::code_str(tc) << std::endl;
	} else if (_tsp->code() == tagd::TS_NOT_FOUND) {
		if (_tsh->echo_result_code())
			(*_tsh->out) << "-- " << tagd::code_str(tc) << std::endl;
	} else {
		this->handle_cmd_error();
	}
	_tsh->last_code((CMD_OK() || _tsp->code() == tagd::TS_NOT_FOUND) ? tc : _driver->code());
	log_driver_debug("driver code=%s", tagd::code_str(_driver->code()));

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	auto ssn = _driver->session_ptr();
	log_driver_debug("driver statement command=CMD_QUERY subject=%s", q.id());
	log_driver_debug("driver callback=cmd_query subject=%s", q.id());

	auto tc = _tsp->query(T, q, ssn, _driver->flags|tagd::F_NO_NOT_FOUND_ERROR);
	if (!CMD_OK()) {
		this->handle_cmd_error();
		log_driver_debug("driver code=%s", tagd::code_str(_driver->code()));
		add_history_lines_clear(_lines);
		return;
	}

	switch(tc) {
		case tagd::TAGD_OK:
			if (q.super_object() == HARD_TAG_REFERENT)
				tagd::print_tags(T, *_tsh->out);
			else
				tagd::print_tag_ids(T, *_tsh->out);
			break;
		case tagd::TS_NOT_FOUND:  // TS_NOT_FOUND not an error for queries
			if (_tsh->echo_result_code())
				(*_tsh->out) << "-- " << tagd::code_str(tc) << std::endl;
			break;
		default:
			this->handle_cmd_error();
	}
	_tsh->last_code((!CMD_OK()) ? _driver->code() : tc);
	log_driver_debug("driver code=%s", tagd::code_str(_driver->code()));

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_error() {
	_tsh->last_code(_driver->code());
	log_driver_debug("driver callback=cmd_error code=%s", tagd::code_str(_driver->code()));
	_driver->print_errors();
	add_history_lines_clear(_lines);
}

std::vector<std::string> split_string(const std::string &s, const char *delim = " ", bool allow_empty = false)
{
	std::vector<std::string> results;

	size_t prev = 0;
	size_t next = 0;

	while ((next = s.find_first_of(delim, prev)) != std::string::npos) {
		if (allow_empty || (next - prev != 0)) {
			results.push_back(s.substr(prev, next - prev));
		}
		prev = next + 1;
	}

	if (prev < s.size()) {
		results.push_back(s.substr(prev));
	}

	return results;
}

void tagsh::dump_file(const std::string& fname, bool check_existing) {
	assert(!fname.empty());

	if (check_existing && tagd::io::file_exists(fname)) {
		error("file exists: %s\nuse '-f' to overwrite", fname.c_str());
		return;
	}

	std::ofstream ofs;
	ofs.open(fname.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (!ofs.is_open()) {
		error("open failed, no such file: %s", fname.c_str());
		return;
	}
	_tsp->dump(ofs);
	ofs.close();
	TAGD_COUT << "dumped to file: " << fname << std::endl;
}

void tagsh::command(const std::string& cmdline) {
	std::vector<std::string> V = split_string(cmdline);
	assert(V.size() != 0);
	std::string cmd(V[0]);

	if (cmd == ".load") {
		if (V.size() != 2) {
			error("usage: %s <filename>", cmd.c_str());
			return;
		}

		if (!tagd::io::file_exists(V[1])) {
			error("no such file: %s", V[1].c_str());
			return;
		}

		auto batch = this->_batch;
		this->_batch = true;
		this->interpret_fname(V[1]);
		this->_batch = batch;
		return;
	}

	if (cmd == ".dump") {
		switch (V.size()) {
			case 1:
				this->dump();
				return;
			case 2:
				this->dump_file(V[1]);
				return;
			case 3:
				if (V[1] == "-f")
					this->dump_file(V[2], false);
				else
					error("malformed command: %s", cmdline.c_str());
				return;
		}
	}

	// The diagnostic grid is provided by the storage implementation.
	if (cmd == ".dump_grid") {
		_tsp->dump_grid();
		return;

	}

	if (cmd == ".dump_terms") {
		_tsp->dump_terms();
		return;
	}

	if (cmd == ".dump_search") {
		_tsp->dump_search();
		return;
	}

	if (cmd == ".print_flags") {
		TAGD_COUT << tagd::flag_util::flag_list_str(_driver.flags) << std::endl;
		return;
	}

	if (cmd == ".print_context") {
		auto ssn = _driver.session_ptr();
		if (!ssn) return;
		ssn->print_context();
		return;
	}

	if (cmd == ".exit" || cmd == ".quit")
		std::exit(EXIT_SUCCESS);

	if (cmd == ".show") {
		cmd_show();
		return;
	}

	error("no such command: %s", cmd.c_str());
}

int tagsh::interpret_readline() {
	// tab auto-complete paths
	rl_bind_key('\t', rl_complete);

	char* input;
	while((input = readline(prompt().c_str())) != NULL) {
		_callback->_lines.push_back(input);
		this->interpret(input);
	}

	return 0;
}

int tagsh::interpret(const std::string &line) {
	if (!line.empty() && line[0] == '.') {
		this->last_code(tagd::TAGD_OK);
		command(line);
		add_history_lines_clear(_callback->_lines);
		return 0;
	} else {
		this->last_code(tagd::TAGD_OK);
		_driver.parseln(line);

		// force a reduce action when a terminator
		// hanging on the end of the stack
		// i.e. cmd_statement TERMINATOR
		if (!_driver.has_errors() && _driver.token() == TOK_TERMINATOR)
			_driver.parse_tok(TOK_TERMINATOR, TAGL::EMPTY_VALUE);

		if (_driver.has_errors()) {
			auto code = _driver.code();
			_driver.finish();
			_driver.clear_errors();
			this->last_code(code);
			return code;
		}
	}

	return 0;
}

int tagsh::interpret(std::istream& ins) {
	std::string line;

	if (!prompt().empty())
		TAGD_COUT << prompt();

	while (getline(ins, line)) {
		int rc = this->interpret(line);
		if (rc)
			return rc;

		if (!prompt().empty())
			TAGD_COUT << prompt();
	}

	_driver.finish();

	return 0;
}

int tagsh::interpret_fname(const std::string& fname, bool ignore_duplicates) {
	log_tagsh_notice("tagsh load file=%s", fname);
	auto flags = _driver.flags;
	if (ignore_duplicates)
		_driver.flags |= tagd::F_IGNORE_DUPLICATES;
	this->last_code(tagd::TAGD_OK);
	auto rc = _driver.include_file(fname);
	_driver.flags = flags;
	_driver.finish();
	if (rc != tagd::TAGD_OK)
		this->last_code(rc);
	return this->last_code();
}

void tagsh::cmd_show() {
	TAGD_COUT << ".load <filename>\t# load a tagl file" << std::endl;
	TAGD_COUT << ".dump\t# dump tagspace to stdout" << std::endl;
	TAGD_COUT << ".dump [-f] <filename>\t# dump tagspace to file, [-f] forces overwrite existing" << std::endl;
	TAGD_COUT << ".dump_grid\t# dump tagspace to stdout as a grid" << std::endl;
	TAGD_COUT << ".dump_terms\t# dump tagspace terms and part_of_speech lists to stdout" << std::endl;
	TAGD_COUT << ".dump_search\t# dump full text content of tag search terms" << std::endl;
	TAGD_COUT << ".print_flags\t# print TAGL flags set" << std::endl;
	TAGD_COUT << ".exit\t# exit this shell" << std::endl;
	TAGD_COUT << ".quit\t# same as .exit" << std::endl;
	TAGD_COUT << ".show\t# show commands" << std::endl;
}

cmd_args::cmd_args()
{

	// Collect storage options without opening anything: creation and selection
	// are resolved together after parsing, regardless of their argument order.
	_cmds["--tagspace"] = {
		[this](char *val) {
			if (!*val) { this->error(tagd::TS_ERR, "invalid tagspace name"); return; }
			this->_tagspace_name = val;
		}, true
	};
	_cmds["--home"] = {
		[this](char *val) {
			if (!*val) { this->error(tagd::TAGD_ERR, "empty --home"); return; }
			this->_home = val;
			_overrides.emplace_back(tagd::HARD_TAG_CONFIG_HOME, val);
		}, true
	};
	_cmds["--create"] = {
		[this](char *) { this->_create = true; }, false
	};

	_cmds["--config"] = {
		[this](char* val) {
			if (!*val) { this->error(tagd::TAGD_ERR, "empty --config"); return; }
			_config_file = val;
		}, true
	};
	_cmds["--config-id"] = {
		[this](char* val) {
			if (!*val) { this->error(tagd::TAGD_ERR, "empty --config-id"); return; }
			_config_id = val;
		}, true
	};

	cmd_handler noshell_handler = {
		[this](char *) { this->_noshell = true; },
		false
	};
	_cmds["--noshell"] = noshell_handler;
	_cmds["-n"] = noshell_handler;

	cmd_handler tagl_handler = {
		[this](char *val) {
			// files and statements processed in order
			_actions.push_back({std::string(val) == "-" ? startup_action::kind::stdin_stream
				: startup_action::kind::tagl, val});
		}, true
	};
	_cmds["--tagl"] = tagl_handler;
	_cmds["-t"] = tagl_handler;

	cmd_handler file_handler = {
		[this](char *val) {
			if (!tagd::io::file_exists(val)) {
				this->ferror(tagd::TAGD_ERR, "no such file: %s", val);
				return;
			}
			// files and statements processed in order
			_actions.push_back({startup_action::kind::file, val});
		}, true
	};
	_cmds["--file"] = file_handler;
	_cmds["-f"] = file_handler;

	_cmds["--log-level"] = {
		[this](char *val) {
			tagd::logger candidate;
			if (!*val || !tagd::parse_log_level_spec(val, candidate))
				this->ferror(tagd::TAGD_ERR, "invalid log level: %s", val);
			else {
				if (!_log_spec.empty())
					_log_spec += ",";
				_log_spec += val;
				_overrides.emplace_back(tagd::HARD_TAG_CONFIG_LOG_LEVEL, _log_spec);
			}
		},
		true
	};

	_cmds["--dump"] = {
		[this](char *) {
			_dump = true;
			_noshell = true;
		},
		false
	};

	cmd_handler help_handler = {
		[this](char *) {
			std::cout
			<< "tagsh" 																		<< std::endl
			<< "		with no options: interactive tagl prompt with an in-memory tagspace" 	<< std::endl
			<< "tagsh [options]" 															<< std::endl
			<< "---------------" 															<< std::endl
			<< "  --config FILE    alternate TAGL configuration file" << std::endl
			<< "  --config-id ID   configuration subject (default default_config)" << std::endl
			<< "  --home DIR       tagd home (default ~/.tagd)" << std::endl
			<< "  --tagspace ID    open an existing tagspace (default: memory)" << std::endl
			<< "  --create         create the named tagspace; fail if it exists" << std::endl
			<< "  -f <tagl file>" 															<< std::endl
			<< "  --file" 																	<< std::endl
			<< "		execute tagl file, multiple flags will be processed in order" 		<< std::endl
			<< "  -t <tagl code>" 															<< std::endl
			<< "  --tagl" 																	<< std::endl
			<< "		tagl code to be executed in order" 									<< std::endl
			<< "  -n" 																		<< std::endl
			<< "  --noshell" 																<< std::endl
			<< "		execute then exit with no shell" 									<< std::endl
			<< "  --dump" 																	<< std::endl
			<< "		dump tagspace" 														<< std::endl
			<< "  --log-level <level>"														<< std::endl
			<< "		set log level or role overrides, e.g. warning,_role:scanner:debug"	<< std::endl
			<< std::endl;
			std::exit(EXIT_SUCCESS);
		}, false
	};
	_cmds["--help"] = help_handler;
	_cmds["-h"] = help_handler;

}

void cmd_args::parse(int argc, char **argv) {
	for (int i=1; i<argc; ++i) {
		auto it = _cmds.find(std::string(argv[i]));
		if (it == _cmds.end()) {
			this->ferror(tagd::TAGD_ERR, "invalid option: %s", argv[i]);
			return;
		}
		auto h = it->second;
		if (h.has_arg) {
			if (++i < argc) {
				h.handler(argv[i]);
			} else {
				this->ferror(tagd::TAGD_ERR, "value required: %s <val>", argv[i-1]);
				return;
			}
		} else {
			h.handler(nullptr);
		}
		if (this->has_errors())
			return;
	}

	// Validate the combination only after parsing: option order has no meaning.
	if (_create && _tagspace_name.empty()) {
		this->error(tagd::TAGD_ERR, "--create requires --tagspace ID");
		return;
	}

	this->code(tagd::TAGD_OK);
}

void cmd_args::config_option(const std::string& option, tagd::id_view setting) {
	_cmds[option] = {[this, id = tagd::id_string(setting)](char* value) {
		_overrides.emplace_back(id, value);
	}, true};
}

void uninstall_loggers() {
	TAGL_SET_LOGGER(nullptr);
	TAGSPACE_SET_LOGGER(nullptr);
	TAGSH_SET_LOGGER(nullptr);
}

void install_loggers(tagd::config& cfg) {
	TAGL_SET_LOGGER(&cfg.log());
	TAGSPACE_SET_LOGGER(&cfg.log());
	TAGSH_SET_LOGGER(&cfg.log());
}

tagd::code load_config(tagd::config& cfg, const cmd_args& args) {
	const auto home = args.home().empty() ? tagd::default_tagd_home() : args.home();
	auto rc = cfg.init(home);
	if (rc != tagd::TAGD_OK)
		return rc;
	// A missing home means no implicit config source, never a cwd-relative fallback.
	const auto file = !args.config_file().empty() ? args.config_file()
		: home.empty() ? std::string{}
		: (std::filesystem::path(home) / "_config/_config.tagl").string();
	if (!file.empty()) {
		std::error_code ec;
		auto status = std::filesystem::symlink_status(file, ec);
		if (ec == std::errc::no_such_file_or_directory)
			ec.clear();
		bool present = status.type() != std::filesystem::file_type::not_found;
		if (ec)
			return cfg.error(tagd::TAGD_ERR, "cannot inspect config file: " + file);
		if (present || !args.config_file().empty()) {
			tagsh loader(&cfg, cfg);
			loader.batch(true);
			rc = static_cast<tagd::code>(loader.interpret_fname(file, true));
			if (rc != tagd::TAGD_OK)
				return cfg.error(rc, "cannot load config file: " + file);
		}
	}

	rc = cfg.select(args.config_id(), home);
	if (rc != tagd::TAGD_OK)
		return rc;
	for (const auto& [id, value] : args.overrides()) {
		rc = cfg.set(id, value);
		if (rc != tagd::TAGD_OK)
			return rc;
	}
	return tagd::TAGD_OK;
}

int execute_startup_actions(tagsh& shell, const std::vector<startup_action>& actions) {
	// Batch execution must not overwrite preferences changed by its TAGL input.
	struct restore_batch {
		tagsh& shell;
		bool previous;
		~restore_batch() { shell.batch(previous); }
	} restore{shell, shell.batch()};
	shell.batch(true);
	for (const auto& action : actions) {
		int rc;
		switch (action.type) {
			case startup_action::kind::file:
				rc = shell.interpret_fname(action.input);
				break;
			case startup_action::kind::stdin_stream:
				rc = shell.interpret(std::cin);
				break;
			case startup_action::kind::tagl:
				rc = shell.interpret(action.input);
				if (!rc)
					rc = shell.interpret(";");
				break;
		}
		if (rc)
			return rc;
	}
	return 0;
}
