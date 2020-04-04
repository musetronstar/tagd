#include <cxxtest/TestSuite.h>

#include "tagd.h"

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

	void test_ext(void) {
		std::string path, ext;
		size_t pos;

		path = "/path/to/file.html";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "html" )

		path = "file.css";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "css" )

		path = "./file.txt";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "txt" )

		path = "../file.tagl";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "tagl" )

		path = ".file.conf";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "conf" )

		path = "~/.file.rc";
		pos = tagd::file::ext_pos(path);
		ext = path.substr(pos);
		TS_ASSERT_EQUALS( ext , "rc" )

		// npos == not found
		path = "/path/to/file";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "/path/to/file.";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "/path/to/.file";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "/path/to/file.ext.";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "file";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "file.ext.";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = ".file";
		pos = tagd::file::ext_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )
	}

	void test_dir_shift_pos(void) {
		std::string path, sub;
		size_t pos;

		path = "/path/to/file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "to/file" )

		path = "path/to/file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "to/file" )

		path = "../path/to/file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "path/to/file" )

		path = "./../path/to/file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "../path/to/file" )

		path = "../../path/to/file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "../path/to/file" )

		path = "file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "file" )

		path = "";
		pos = tagd::file::dir_shift_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "/";
		pos = tagd::file::dir_shift_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "//";
		pos = tagd::file::dir_shift_pos(path);
		TS_ASSERT_EQUALS( pos , std::string::npos )

		path = "//file";
		pos = tagd::file::dir_shift_pos(path);
		sub = path.substr(pos);
		TS_ASSERT_EQUALS( sub , "file" )
	}

	void test_ext_media_type(void) {
		const char *mt_html = tagd::file::ext_media_type("html");
		TS_ASSERT_EQUALS( mt_html , "text/html" )

		const char *mt_upper = tagd::file::ext_media_type("JPG");
		TS_ASSERT_EQUALS( mt_upper , "image/jpeg" )

		const char *mt_not_found = tagd::file::ext_media_type("caca");
		TS_ASSERT_EQUALS( mt_not_found , nullptr )
	}

	void test_concat_dir(void) {
		TS_ASSERT_EQUALS( tagd::io::concat_dir("", ""), "" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("a", ""), "a" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("", "b"), "b" )

		TS_ASSERT_EQUALS( tagd::io::concat_dir("a/", ""), "a/" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("", "/b"), "/b" )

		TS_ASSERT_EQUALS( tagd::io::concat_dir("a", "b"), "a/b" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("a/", "b"), "a/b" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("a", "/b"), "a/b" )
		TS_ASSERT_EQUALS( tagd::io::concat_dir("a/", "/b"), "a/b" )
	}

	void test_file_dir_exists(void) {
		TS_ASSERT( tagd::io::dir_exists("./io-test") )
		TS_ASSERT( !tagd::io::dir_exists("./no-test") )

		TS_ASSERT( tagd::io::file_exists("./io-test/file.txt") )
		TS_ASSERT( !tagd::io::file_exists("./io-test/nofile.txt") )
	}
};
