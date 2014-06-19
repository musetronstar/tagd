#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <event2/buffer.h>
#include "tagd.h"
#include "tagl.h"
#include "tagspace/sqlite.h"
#include "tagsh.h"

// TODO put in a utility library
int tagsh::error(const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	char *err = tagd::util::csprintf(errfmt, args);
	va_end (args);

	if (err == NULL)
		std::perror("error: ");
	else
		std::cerr << err << std::endl;

	return 1;
}

tagsh_callback::tagsh_callback(space_type *ts) {
	_TS = ts;
}

void add_history_lines_clear(cmdlines_t& lines) {
	std::string hst;
	for (auto l : lines) {
		hst.append(l);
		if (l != lines.back())
			hst.push_back('\n');
		delete l;
	}
	lines.clear();
	if (!hst.empty())
		add_history(hst.c_str());
}

void tagsh_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag *T;
	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url();
		_TS->get((tagd::url&)*T, t.id(), _driver->flags());
	}
	else {
		T = new tagd::abstract_tag();
		_TS->get(*T, t.id(), _driver->flags());
	}

	if(_TS->ok()) {
		std::cout << *T << std::endl;
	} else {	
		_driver->code(_TS->code()); // stops the scanner
		_TS->print_errors();
		_TS->clear_errors();
	}

	delete T;

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_put(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->put(t, _driver->flags());
	if (_TS->ok()) {
		// std::cout << "-- " << tagd_code_str(ts_rc) << std::endl;
	} else {
		_driver->code(_TS->code()); // stops the scanner
		_TS->print_errors();
		_TS->clear_errors();
	}
	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_del(const tagd::abstract_tag& t) {
	tagd::code ts_rc = _TS->del(t, _driver->flags());
	if (_TS->ok()) {
		// std::cout << "-- " << tagd_code_str(ts_rc) << std::endl;
	} else if (_TS->code() == tagd::TS_NOT_FOUND ) {
		std::cout << "-- " << tagd_code_str(ts_rc) << std::endl;
	} else {
		_driver->code(_TS->code()); // stops the scanner
		_TS->print_errors();
		_TS->clear_errors();
	}
	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	_TS->query(T, q, _driver->flags());
	switch(_TS->code()) {
		case tagd::TAGD_OK:
			if (q.super_object() == HARD_TAG_REFERENT)
				tagd::print_tags(T);
			else
				tagd::print_tag_ids(T);
			break;
		case tagd::TS_NOT_FOUND:  // TS_NOT_FOUND not an error for queries
			std::cout << "-- " << tagd_code_str(_TS->code()) << std::endl;
			break;
		default:	
			_driver->code(_TS->code()); // stops the scanner
			_TS->print_errors();
			_TS->clear_errors();
	}
	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_error() {
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

// TODO put in a utility library
bool tagsh::file_exists(const std::string& fname) {
  struct stat buff;
  return (stat(fname.c_str(), &buff) == 0);
}

void tagsh::dump_file(const std::string& fname, bool check_existing) {
	assert(!fname.empty());

	if (check_existing && file_exists(fname)) {
		error("file exists: %s\nuse '-f' to overwrite", fname.c_str());
		return;
	}

	std::ofstream ofs;
	ofs.open(fname.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (!ofs.is_open()) {
		error("open failed, no such file: %s", fname.c_str());
		return;
	}
	_TS->dump(ofs);
	ofs.close();
	std::cout << "dumped to file: " << fname << std::endl;
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

		if (!file_exists(V[1])) {
			error("no such file: %s", V[1].c_str());
			return;
		}

		this->interpret_fname(V[1]);
		return;
	}

	if ( cmd == ".dump" ) {
		switch (V.size()) {
			case 1:
				_TS->dump();
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

	// dump_grid specific only to tagspace_sqlite
	if ( cmd == ".dump_grid" ) {
		_TS->dump_grid();
		return;
	
	}

	if ( cmd == ".dump_terms" ) {
		_TS->dump_terms();
		return;
	}

	if ( cmd == ".dump_search" ) {
		_TS->dump_search();
		return;
	}

	if ( cmd == ".print_flags" ) {
		std::cout << tagspace::flag_util::flag_list_str(_driver.flags()) << std::endl;
		return;
	}

	if ( cmd == ".trace_on" ) {
		_TS->trace_on();  // specific to sqlite
		_driver.trace_on("trace: ");
		return;
	}

	if ( cmd == ".trace_off" ) {
		_TS->trace_off();
		_driver.trace_off();
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
    while( (input = readline(prompt.c_str())) != NULL ) {
        _CB->_lines.push_back(input);
		this->interpret(input);
    }

	return 0;
}

int tagsh::interpret(const std::string &line) {
	if (line[0] == '.') {
		command(line);
		add_history_lines_clear(_CB->_lines);
		return 0;
	} else {
		_driver.parseln(line);

		// force a reduce action when a terminator
		// hanging on the end of the stack
		// i.e. cmd_statement TERMINATOR
		if (!_driver.has_error() && _driver.token() == TERMINATOR)
			_driver.parse_tok(TERMINATOR, NULL);

		if (_driver.has_error()) {
			_driver.finish();
			_driver.clear_errors();
			return _driver.code();
		}
	}

	return 0;
}

int tagsh::interpret(std::istream& ins) {
	std::string line;

	if (!prompt.empty())
		std::cout << prompt;

	while (getline(ins, line)) {
		this->interpret(line);

		if (!prompt.empty())
			std::cout << prompt;
	}

	_driver.finish();

	return 0;
}

int tagsh::interpret_fname(const  std::string& fname) {
	int fd = open(fname.c_str(), O_RDONLY);
	
	if (fd < 0) 
		return error("failed to open: %s", fname.c_str());

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return error("stat failed: %s", fname.c_str());
	}

	struct evbuffer *input = evbuffer_new();

	if (st.st_size == 0) {
		return 0;  // warn, don't fail
		std::cerr << "ignoring zero length file: "  << fname << std::endl;
		if (fcntl(fd, F_GETFD) != -1)
			close(fd);
	}

	// evbuffer_add_file closes fd for us
	if (evbuffer_add_file(input, fd, 0, st.st_size) == 0) {
		_driver.filename(fname);
		_driver.evbuffer_execute(input);
	} else {
		return error("evbuffer_add_file failed: %s", fname.c_str());
		if (fcntl(fd, F_GETFD) != -1)
			close(fd);
	}

	evbuffer_free(input);

	return 0;
}

void tagsh::cmd_show() {
	std::cout << ".load <filename>\t# load a tagl file" << std::endl;
	std::cout << ".dump\t# dump tagspace to stdout" << std::endl;
	std::cout << ".dump [-f] <filename>\t# dump tagspace to file, [-f] forces overwrite existing" << std::endl;
	std::cout << ".dump_grid\t# dump tagspace to stdout as a grid (specific to sqlite)" << std::endl;
	std::cout << ".dump_terms\t# dump tagspace terms and part_of_speech lists to stdout" << std::endl;
	std::cout << ".dump_search\t# dump full text content of tag search terms" << std::endl;
	std::cout << ".print_flags\t# print TAGL flags set" << std::endl;
	std::cout << ".trace_on\t# trace tagl lexer and parser execution path and sql statements" << std::endl;
	std::cout << ".trace_off\t# turn trace off" << std::endl;
	std::cout << ".exit\t# exit this shell" << std::endl;
	std::cout << ".quit\t# same as .exit" << std::endl;
	std::cout << ".show\t# show commands" << std::endl;
}

cmd_args::cmd_args() :
	opt_create{false}, opt_trace{false}
{
	_cmds["--db"] = {
		[this](char *val) {
			if (val[0] == '-')
				this->db_fname = ":memory:";
			else {
				if (!this->opt_create && !tagsh::file_exists(val)) {
					this->ferror(tagd::TAGD_ERR, "no such file: %s", val);
					return;
				}
				this->db_fname = val;
			}
		}, true
	};

	_cmds["--create"] = {
		[this](char *) { this->opt_create = true; },
		false
	}; 

	_cmds["--tagl"] = {
		[this](char *val) { 
				tagl_file_statements.push_back(
						std::string("--tagl").append(" ").append(val) );
		}, true
	};

	auto trace_f = [this](char *val) { opt_trace = true; };
	_cmds["--trace"] = { trace_f, false };
	_cmds["--trace_on"] = { trace_f, false };
}

void cmd_args::parse(int argc, char **argv) {
	for (int i=1; i<argc; i++) {

		if (argv[i][0] != '-') {
			tagl_file_statements.push_back(argv[i]);
			continue;
		}

		auto cmd_it = _cmds.find(std::string(argv[i]));
		if (cmd_it != _cmds.end()) {
			auto h = cmd_it->second;
			if (h.has_arg) {
				if (++i < argc) {
					h.handler( argv[i] );
				} else {
					this->ferror(tagd::TAGD_OK, "option required: %s <val>", argv[i-1]);
					return;
				}
			} else {
				h.handler( nullptr );
			}
			if (this->has_error()) return;
			continue;
		} else {
			this->ferror(tagd::TAGD_OK, "invalid option: %s", argv[i]);
			return;
		}
	}

	if (db_fname.empty())
		db_fname = tagspace::util::user_db();

	this->code(tagd::TAGD_OK);
}

int cmd_args::interpret(tagsh& shell) {

	auto f_tagl_statement = [&](const std::string &s) -> int {
		int err;
		shell.prompt.clear();
		err = shell.interpret(s);
		if (err) return err;
		err = shell.interpret(";"); // just in case it wasn't provided
		return err;
	};

	if (opt_trace) {
		shell.interpret(".trace_on");
	}

	const std::string TAGL_OPT("--tagl");
	int err;
	// input file(s) 
	for(auto s : tagl_file_statements) {
		if (s == "-") {  // fname == "-", use interpret stdin
			// TODO create an evbuffer from stdin, so shell commands are not exposed
			shell.prompt.clear();
			err = shell.interpret(std::cin);
		} else if (s.substr(0, TAGL_OPT.size()) == TAGL_OPT) {
			err = f_tagl_statement( s.substr(TAGL_OPT.size()) );
		} else {
			err = shell.interpret_fname(s);
		}

		if ( err ) {
			// err already printed
			return err;
		}
	}

	return 0;
}


