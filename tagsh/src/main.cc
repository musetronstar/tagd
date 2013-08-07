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
#include "tagd.h"
#include "tagl.h"
#include "tagspace/sqlite.h"

#define PRG "tagsh"

class tagsh_callback : public TAGL::callback {
		tagspace::tagspace *_TS;

	public:
		tagsh_callback(tagspace::tagspace*);
		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&); 
        void error(const TAGL::driver&);
};

tagsh_callback::tagsh_callback(tagspace::tagspace *ts) {
	_TS = ts;
}

void tagsh_callback::cmd_get(const tagd::abstract_tag& t) {
	tagd::abstract_tag T;
	ts_res_code ts_rc = _TS->get(T, t.id());
	std::cout << ts_res_str(ts_rc) << std::endl;
	if (ts_rc == TS_OK) {
		std::cout << T << std::endl;
	}
}

void tagsh_callback::cmd_put(const tagd::abstract_tag& t) {
	ts_res_code ts_rc = _TS->put(t);
	std::cout << ts_res_str(ts_rc) << std::endl;
}

void tagsh_callback::cmd_query(const tagd::interrogator& q) {
	tagd::tag_set T;
	ts_res_code ts_rc = _TS->query(T, q);
	std::cout << ts_res_str(ts_rc) << std::endl;
	tagd::print_tag_ids(T);		
}

void tagsh_callback::error(const TAGL::driver& D) {
	std::cerr << tagl_code_str(D.code()) << ": " << D.msg() << std::endl;
}


class tagsh {
		tagspace::tagspace *_TS;
		TAGL::callback *_CB;
		TAGL::driver _driver;
	public:
		std::string prompt;
		tagsh(tagspace::tagspace*, TAGL::callback*);
		void command(const std::string&);
		int interpret(std::istream&);
		int interpret(const std::string&);
		void dump_file(const std::string&, bool = true);
};

tagsh::tagsh(tagspace::tagspace *ts, TAGL::callback *cb) :
	_TS(ts), _CB(cb), _driver(ts, cb), prompt("tagd> ")
{}

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

bool file_exists(const std::string& fname) {
  struct stat buff;
  return (stat(fname.c_str(), &buff) == 0);
}

void tagsh::dump_file(const std::string& fname, bool check_existing) {
	assert(!fname.empty());

	if (check_existing && file_exists(fname)) {
		std::cerr << "file exists: " << fname << std::endl
			      << "use '-f' to overwrite" << std::endl;
		return;
	}

	std::ofstream ofs;
	ofs.open(fname.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (!ofs.is_open()) {
		std::string msg("failed to open ");
		msg.append(fname);
		std::perror(msg.c_str());
		return;
	}
	_TS->dump(ofs);
	ofs.close();
	std::cout << "dumped to file: " << fname << std::endl;
}

void cmd_show() {
	std::cout << ".load <filename>\t# load a tagl file" << std::endl;
	std::cout << ".dump\t# dump tagspace to stdout" << std::endl;
	std::cout << ".dump [-f] <filename>\t# dump tagspace to file, [-f] overwrites existing" << std::endl;
	std::cout << ".dump_grid\t# dump tagspace to stdout as a grid (specific to sqlite)" << std::endl;
	std::cout << ".trace_on\t# trace tagl lexer and parser execution path" << std::endl;
	std::cout << ".trace_off\t# turn trace off" << std::endl;
	std::cout << ".exit\t# exit this shell" << std::endl;
	std::cout << ".quit\t# same as .exit" << std::endl;
	std::cout << ".show\t# show commands" << std::endl;
}

void tagsh::command(const std::string& cmdline) {
	std::vector<std::string> V = split_string(cmdline);
	assert(V.size() != 0);
	std::string cmd(V[0]);

	if (cmd == ".load") {
		if (V.size() != 2) {
			std::cerr << "usage: " << cmd << " <filename>" << std::endl;
			return;
		}

		if (!file_exists(V[1])) {
			std::cerr << "no such file: " << V[1] << std::endl;
			return;
		}

		this->interpret(V[1]);
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
					std::cerr << "malformed command: " << cmdline << std::endl;
				return;
		}
	}

	// dump_grid specific only to tagspace_sqlite
	if ( cmd == ".dump_grid" ) {
		_TS->dump_grid();
		return;
	}

	if ( cmd == ".trace_on" ) {
		_driver.trace_on("trace: ");
		return;
	}

	if ( cmd == ".trace_off" ) {
		_driver.trace_off();
		return;
	}

	if (cmd == ".exit" || cmd == ".quit")
		std::exit(EXIT_SUCCESS);

	if (cmd == ".show") {
		cmd_show();
		return;
	}

	std::cerr << "no such command: " << cmd << std::endl;
}

/* returns err code */
int tagsh::interpret(std::istream& ins) {
	std::string line;

	if (!prompt.empty())
		std::cout << prompt;

	while (getline(ins, line)) {
		if (line[0] == '.') {
			command(line);
		} else {
			tagl_code tc = _driver.parseln(line);

			if (tc == TAGL_ERR)
				_driver.finish();
		}

		if (!prompt.empty())
			std::cout << prompt;
	}

	_driver.finish();

	return 0;
}

int tagsh::interpret(const  std::string& fname) {
	std::ifstream fs(fname.c_str());
	if (!fs.is_open()) {
		std::string msg("failed to open ");
		msg.append(fname);
		std::perror(msg.c_str());
		return 1;
	}

	// interpret in another context so this one is not affected
	tagsh sub = tagsh(this->_TS, this->_CB);
	sub.prompt.clear();

	int ret = sub.interpret(fs);
	fs.close();

	return ret;
}

int main(int argc, char **argv) {

	//const std::string db_fname = "tagspace.sqlite";
	const std::string db_fname = ":memory:";

	tagspace::sqlite TS;
	TS.init(db_fname);
	tagsh_callback CB(&TS);
	tagsh shell(&TS,&CB);

	if (argc==1) {
		std::cout << "use .show to list commands" << std::endl;
		return shell.interpret(std::cin);
	}

	int err;
	// input file(s) 
	for(int i=1; i<argc; i++) {
		err = shell.interpret(argv[i]);
		if ( err ) {
			// err already printed
			return err;
		}
	}

	return 0;
}
