#pragma once

#include <functional>
#include <iostream>
#include <ostream>

#include "tagd/logger.h"
#include "tagspace.h"
#include "tagspace-config.h"
#include "tagl.h"

typedef std::vector<char *> cmdlines_t;

class tagsh;
class cmd_args;

class tagsh_callback : public TAGL::callback {
		friend class tagsh;
		tagsh *_tsh;         // borrowed
		cmdlines_t _lines;

	public:
		explicit tagsh_callback(tagsh *tsh) : _tsh{tsh} {}
		~tagsh_callback() { for(auto l : _lines) delete l; }

		void cmd_get(const tagd::abstract_tag&);
		void cmd_put(const tagd::abstract_tag&);
		void cmd_store(const tagd::abstract_tag&) override;
		void cmd_delete_storage(const tagd::abstract_tag&) override;
		void cmd_delete(const tagd::abstract_tag&);
		void cmd_query(const tagd::interrogator&);
		void cmd_error();

		void handle_cmd_error();
};

class tagsh {
	protected:
		tagd::config& _config;        // borrowed
		bool _batch = false;
		tagsh_callback *_callback;
		bool _own_callback = false;   // true when this instance allocated _callback
		TAGL::driver _driver;
		tagd::code _last_code = tagd::TAGD_OK;

	public:
		enum class input_origin { script, terminal };

		std::string prompt() const {
			return _batch ? std::string{} : _config.prompt();
		}
		bool echo_result_code() const {
			return !_batch && _config.echo_result_code();
		}
		bool batch() const {
			return _batch;
		}
		void batch(bool value) {
			_batch = value;
		}
		std::ostream *out = &std::cout;

		tagsh(tagd::tagspace *tsp, tagd::config& cfg, tagsh_callback *cb) :
			_config{cfg}, _callback{cb}, _driver(tsp, cb)
		{
			_driver.own_session(tsp->new_session());  // heap session; ownership transferred to _driver
			_driver.configure(cfg);
		}

		tagsh(tagd::tagspace *tsp, tagd::config& cfg) :
			_config{cfg}, _callback{new tagsh_callback(this)}, _own_callback{true},
			_driver(tsp, _callback)
		{
			_driver.own_session(tsp->new_session());  // heap session; ownership transferred to _driver
			_driver.configure(cfg);
		}

		~tagsh() {
			if (_own_callback)
				delete _callback;
		}

		tagd::tagspace* tsp() { return _driver.tsp(); }
		const tagd::tagspace* tsp() const { return _driver.tsp(); }
		tagd::code select_startup(const cmd_args&);
		void configuration_loading(bool value) { _driver.configuration_loading(value); }
		void command(const std::string&, input_origin = input_origin::script);
		int interpret_readline();
		int interpret(std::istream&);
		int interpret(const std::string&, input_origin = input_origin::script);  // tagl statement
		int interpret_store(const std::string& source, const std::string& predicates);
		int interpret_fname(const std::string&, bool ignore_duplicates = false);  // filename
		tagd::code last_code() const { return (_last_code == tagd::TAGD_OK ? _driver.tsp()->code() : _last_code); }
		void last_code(tagd::code code) { _last_code = code; }
		void set_output(std::ostream& os) { out = &os; }
		void dump() { _driver.tsp()->dump(); }
		void dump_file(const std::string&, bool = true);
		int error(const char *errfmt, ...);
		static void cmd_show();
};

typedef std::function<void(char *)> cmd_arg_handler_t;
struct cmd_handler {
	cmd_arg_handler_t handler;
	bool has_arg;
};
typedef std::map< std::string, cmd_handler > cmd_handler_map_t;

struct startup_action {
	enum class kind { tagl, file, stdin_stream, store };
	kind type;
	std::string input;
	std::string predicates = {};
};

class cmd_args : public tagd::errorable {
	std::vector<startup_action> _actions;
	std::vector<std::pair<tagd::id_string, std::string>> _overrides;
	std::string _tagspace_name;
	std::string _home;
	std::string _database;
	std::string _config_file;
	std::string _config_id;
	std::string _log_spec;
	bool _create = false;
	bool _noshell = false;
	bool _dump = false;

protected:
	cmd_handler_map_t _cmds;
	void config_option(const std::string& option, tagd::id_view setting);

public:
	cmd_args();
	// Option handlers capture this parser's address.
	cmd_args(const cmd_args&) = delete;
	cmd_args& operator=(const cmd_args&) = delete;
	cmd_args(cmd_args&&) = delete;
	cmd_args& operator=(cmd_args&&) = delete;
	void parse(int, char**);
	const auto& actions() const {
		return _actions;
	}
	const auto& overrides() const {
		return _overrides;
	}
	const std::string& tagspace_name() const {
		return _tagspace_name;
	}
	const std::string& database() const { return _database; }
	const std::string& home() const {
		return _home;
	}
	const std::string& config_file() const {
		return _config_file;
	}
	const std::string& config_id() const {
		return _config_id;
	}
	bool create() const {
		return _create;
	}
	bool noshell() const {
		return _noshell;
	}
	bool dump() const {
		return _dump;
	}
};

// These functions coordinate startup; the parser owns no runtime resources.
tagd::code load_config(tagd::config&, const cmd_args&);
void install_loggers(tagd::config&);
void uninstall_loggers();
int execute_startup_actions(tagsh&, const std::vector<startup_action>&);
