// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>

#include "tagspace.h"
#include "tagsh.h"

typedef tagd::tagspace::memory tagspace_type;

class DriverTester : public CxxTest::TestSuite {
	void load_bootstrap(tagsh& shell) {
		TS_ASSERT_EQUALS(shell.interpret_fname("bootstrap.tagl"), 0)
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
	}

	void init_runtime(void) {
		tagd::tagspace_install_logger_validator();
	}

	void run_stmt(tagsh& shell, const std::string& stmt) {
		(void)shell.interpret(stmt);
	}

	void run_input(tagsh& shell, const std::string& input) {
		std::istringstream in(input);
		auto prompt = shell.prompt;
		shell.prompt.clear();
		TS_ASSERT_EQUALS(shell.interpret(in), 0);
		shell.prompt = prompt;
	}

	void set_simple_english_context(tagsh& shell) {
		run_stmt(shell, "%% _context simple_english;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
	}

	void clear_stream(std::stringstream& out) {
		out.str("");
		out.clear();
	}

	public:

	void test_cmd_get_not_found(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);

		run_stmt(shell, "<< dog;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_NOT_FOUND)
	}

	void test_cmd_get_returns_tag(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);
		std::stringstream out;
		shell.set_output(out);
		shell.echo_result_code = false;

		load_bootstrap(shell);
		run_stmt(shell, "<< dog;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
		TS_ASSERT_DIFFERS(out.str().find("dog kind_of mammal"), std::string::npos)
	}

	void test_cmd_put_duplicate(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);

		load_bootstrap(shell);
		run_stmt(shell, ">> cat kind_of mammal;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_DUPLICATE)
	}

	void test_cmd_del_not_found(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);

		load_bootstrap(shell);
		run_stmt(shell, "!! badger;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_NOT_FOUND)
	}

	void test_cmd_del_existing(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);

		load_bootstrap(shell);
		run_stmt(shell, "!! bat;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
	}

	void test_referent_lifecycle(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);
		std::stringstream out;
		shell.set_output(out);
		shell.echo_result_code = false;

		load_bootstrap(shell);
		set_simple_english_context(shell);

		run_stmt(shell, "<< doggy;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_NOT_FOUND)

		run_stmt(shell, ">> doggy refers_to dog context simple_english;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)

		clear_stream(out);
		run_stmt(shell, "<< doggy;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
		TS_ASSERT_DIFFERS(out.str().find("refers_to dog"), std::string::npos)

		run_stmt(shell, "!! doggy;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_MISUSE)

		run_stmt(shell, "!! doggy refers_to dog context simple_english;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)

		run_stmt(shell, "<< doggy;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_NOT_FOUND)
	}

	void test_cmd_query_children(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);
		std::stringstream out;
		shell.set_output(out);
		shell.echo_result_code = false;

		load_bootstrap(shell);
		set_simple_english_context(shell);

		run_stmt(shell, "?? what type_of mammal;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
		TS_ASSERT_DIFFERS(out.str().find("dog"), std::string::npos)
		TS_ASSERT_DIFFERS(out.str().find("cat"), std::string::npos)
		TS_ASSERT_DIFFERS(out.str().find("whale"), std::string::npos)

		clear_stream(out);
		run_input(shell, "?? what type_of animal\nhas blood = warm, tail\ncan bark\n");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
		TS_ASSERT_DIFFERS(out.str().find("dog"), std::string::npos)
	}

	void test_cmd_query_empty(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);

		load_bootstrap(shell);
		set_simple_english_context(shell);
		run_stmt(shell, "?? what type_of dog;");

		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_NOT_FOUND)
	}

	void test_context_referent(void) {
		init_runtime();
		tagspace_type tdb;
		TS_ASSERT_EQUALS(tdb.init(), tagd::TAGD_OK)
		tagsh shell(&tdb);
		std::stringstream out;
		shell.set_output(out);
		shell.echo_result_code = false;

		load_bootstrap(shell);
		set_simple_english_context(shell);

		run_stmt(shell, ">> イヌ _refers_to dog _context japanese;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)

		run_stmt(shell, "<< イヌ;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TS_AMBIGUOUS)

		run_stmt(shell, "%% _context simple_english, japanese;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)

		clear_stream(out);
		run_stmt(shell, "<< イヌ;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
		TS_ASSERT_DIFFERS(out.str().find("refers_to dog"), std::string::npos)

		run_stmt(shell, "%% _context simple_english;");
		TS_ASSERT_EQUALS(shell.last_code(), tagd::TAGD_OK)
	}
};
