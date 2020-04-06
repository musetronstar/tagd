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
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <event2/buffer.h>
#include "tagd.h"
#include "tagl.h"
#include "tagdb/sqlite.h"
#include "tagsh.h"

void ALL_SET_TRACE_ON() {
	TAGDB_SET_TRACE_ON();
	TAGL_SET_TRACE_ON();
}

void ALL_SET_TRACE_OFF() {
	TAGDB_SET_TRACE_OFF();
	TAGL_SET_TRACE_OFF();
}

// TODO put in a utility library
int tagsh::error(const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	char *err = tagd::util::csprintf(errfmt, args);
	va_end (args);

	if (err == NULL)
		std::perror("error: ");
	else
		TAGD_CERR << err << std::endl;

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
		ssn->print_errors();
		ssn->clear_errors();
	}
	
	if (!_tdb->ok()) {
		_driver->code(_tdb->code()); // stops the scanner
		_tdb->print_errors();
		_tdb->clear_errors();
	}
}

#define CMD_OK() (_tdb->ok() && (!ssn || (ssn && ssn->ok())))

void tagsh_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag *T;
	auto ssn = _driver->session_ptr();

	if (t.pos() == tagd::POS_URL) {
		T = new tagd::url();
		_tdb->get((tagd::url&)*T, t.id(), ssn, _driver->flags());
	} else {
		T = new tagd::abstract_tag();
		_tdb->get(*T, t.id(), ssn, _driver->flags());
	}

	if(CMD_OK())
		TAGD_COUT << *T << std::endl;
	else
		this->handle_cmd_error();

	delete T;

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_put(const tagd::abstract_tag& t) {
	auto ssn = _driver->session_ptr();
	tagd::code tc = _tdb->put(t, ssn, _driver->flags());

	if (CMD_OK()) {
		if (_tsh->echo_result_code)
			TAGD_COUT << "-- " << tagd::code_str(tc) << std::endl;
	} else {
		this->handle_cmd_error();
	}

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_del(const tagd::abstract_tag& t) {
	auto ssn = _driver->session_ptr();
	tagd::code tc = _tdb->del(t, ssn, _driver->flags());

	if (CMD_OK()) {
		if (_tsh->echo_result_code)
			TAGD_COUT << "-- " << tagd::code_str(tc) << std::endl;
	} else if (_tdb->code() == tagd::TS_NOT_FOUND ) {
		if (_tsh->echo_result_code)
			TAGD_COUT << "-- " << tagd::code_str(tc) << std::endl;
	} else {
		this->handle_cmd_error();
	}

	add_history_lines_clear(_lines);
}

void tagsh_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	auto ssn = _driver->session_ptr();

	auto tc = _tdb->query(T, q, ssn, _driver->flags()|tagdb::F_NO_NOT_FOUND_ERROR);
	if (!CMD_OK()) {
		this->handle_cmd_error();
		add_history_lines_clear(_lines);
		return;
	}

	switch(tc) {
		case tagd::TAGD_OK:
			if (q.super_object() == HARD_TAG_REFERENT)
				tagd::print_tags(T);
			else
				tagd::print_tag_ids(T);
			break;
		case tagd::TS_NOT_FOUND:  // TS_NOT_FOUND not an error for queries
			if (_tsh->echo_result_code)
				TAGD_COUT << "-- " << tagd::code_str(tc) << std::endl;
			break;
		default:	
			this->handle_cmd_error();
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
	_tdb->dump(ofs);
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

		this->interpret_fname(V[1]);

		return;
	}

	if ( cmd == ".dump" ) {
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

	// dump_grid specific only to tagdb_sqlite
	if ( cmd == ".dump_grid" ) {
		_tdb->dump_grid();
		return;
	
	}

	if ( cmd == ".dump_terms" ) {
		_tdb->dump_terms();
		return;
	}

	if ( cmd == ".dump_search" ) {
		_tdb->dump_search();
		return;
	}

	if ( cmd == ".print_flags" ) {
		TAGD_COUT << tagdb::flag_util::flag_list_str(_driver.flags()) << std::endl;
		return;
	}

	if ( cmd == ".print_context" ) {
		auto ssn = _driver.session_ptr();
		if (!ssn) return;
		ssn->print_context();
		return;
	}

	if ( cmd == ".trace_on" ) {
		ALL_SET_TRACE_ON();
		return;
	}

	if ( cmd == ".trace_off" ) {
		ALL_SET_TRACE_OFF();
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
        _callback->_lines.push_back(input);
		this->interpret(input);
    }

	return 0;
}

int tagsh::interpret(const std::string &line) {
	if (line[0] == '.') {
		command(line);
		add_history_lines_clear(_callback->_lines);
		return 0;
	} else {
		_driver.parseln(line);

		// force a reduce action when a terminator
		// hanging on the end of the stack
		// i.e. cmd_statement TERMINATOR
		if (!_driver.has_errors() && _driver.token() == TOK_TERMINATOR)
			_driver.parse_tok(TOK_TERMINATOR, NULL);

		if (_driver.has_errors()) {
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
		TAGD_COUT << prompt;

	while (getline(ins, line)) {
		this->interpret(line);

		if (!prompt.empty())
			TAGD_COUT << prompt;
	}

	_driver.finish();

	return 0;
}

int tagsh::interpret_fname(const  std::string& fname) {
	int ret = _driver.include_file(fname);
	_driver.finish();
	return ret;
}

void tagsh::cmd_show() {
	TAGD_COUT << ".load <filename>\t# load a tagl file" << std::endl;
	TAGD_COUT << ".dump\t# dump tagspace to stdout" << std::endl;
	TAGD_COUT << ".dump [-f] <filename>\t# dump tagspace to file, [-f] forces overwrite existing" << std::endl;
	TAGD_COUT << ".dump_grid\t# dump tagspace to stdout as a grid (specific to sqlite)" << std::endl;
	TAGD_COUT << ".dump_terms\t# dump tagspace terms and part_of_speech lists to stdout" << std::endl;
	TAGD_COUT << ".dump_search\t# dump full text content of tag search terms" << std::endl;
	TAGD_COUT << ".print_flags\t# print TAGL flags set" << std::endl;
	TAGD_COUT << ".trace_on\t# trace tagl lexer and parser execution path and sql statements" << std::endl;
	TAGD_COUT << ".trace_off\t# turn trace off" << std::endl;
	TAGD_COUT << ".exit\t# exit this shell" << std::endl;
	TAGD_COUT << ".quit\t# same as .exit" << std::endl;
	TAGD_COUT << ".show\t# show commands" << std::endl;
}

cmd_args::cmd_args()
{
	_cmds["--db"] = {
		[this](char *val) {
			if (val[0] == '-')
				this->db_fname = ":memory:";
			else {
				if (!this->opt_db_create && !tagd::io::file_exists(val)) {
					this->ferror(tagd::TAGD_ERR, "no such file: %s", val);
					return;
				}
				this->db_fname = val;
			}
		}, true
	};

	_cmds["--create"] = {
		[this](char *) { this->opt_db_create = true; },
		false
	}; 

	cmd_handler noshell_handler = {
		[this](char *) { this->opt_noshell = true; },
		false
	};
	_cmds["--noshell"] = noshell_handler;
	_cmds["-n"] = noshell_handler;

	cmd_handler tagl_handler = {
		[this](char *val) { 
			// files and statements processed in order
			tagl_statements.push_back(std::string("t:").append(val));
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
			tagl_statements.push_back(std::string("f:").append(val));
		}, true
	};
	_cmds["--file"] = file_handler;
	_cmds["-f"] = file_handler;

	cmd_handler trace_handler = {
		[this](char *) { opt_trace = true; },
		false
	};
	_cmds["--trace"] = trace_handler;
	_cmds["--trace_on"] = trace_handler;

	_cmds["--dump"] = {
		[this](char *) {
			opt_dump = true;
			opt_noshell = true;
		},
		false
	};
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

	if (db_fname.empty()) {
		// if a default tagdb file is desired ..
		// this->db_fname = tagdb::util::user_db();
		this->db_fname = ":memory:";
	}

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

	if (this->opt_trace)
		ALL_SET_TRACE_ON();
	else
		shell.echo_result_code = false;

	int err;
	// input files and tagl statements process in order,
	// each string prepended with:
	//   "t:"  tagl statment
	//   "f:"  filename
	for(auto s : this->tagl_statements) {
		if (s.size() <= 2)
			return this->error(tagd::TAGL_ERR, "cmd_args: missing value for TAGL statment");

		auto k = s.substr(0, 2);
		auto v = s.substr(2, std::string::npos);
		if (k == "t:") {
			if (v == "-") {  // fname == "-", use interpret stdin
				// TODO create an evbuffer from stdin, so shell commands are not exposed
				shell.prompt.clear();
				err = shell.interpret(std::cin);
			} else {
				err = f_tagl_statement(v);
			}
		} else if (k == "f:") {
			err = shell.interpret_fname(v);
		} else {
			assert(0);
			return this->error(tagd::TAGL_ERR, "cmd_args: internal error processing tagl_statements");
		}

		if (err) return err;
	}

	if (this->opt_dump)
		shell.dump();

	shell.echo_result_code = true;

	if (!this->opt_noshell) {
		TAGD_COUT << "use .show to list commands" << std::endl;
		shell.prompt = tagsh::DEFAULT_PROMPT;
		return shell.interpret_readline();
	}

	return 0;
}


