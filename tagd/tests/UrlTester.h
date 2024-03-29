// Test suites

#include <cxxtest/TestSuite.h>

#include <stdlib.h>
#include <time.h>

#include "tagd.h"

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))

// returns true if each of the url components were parsed identically
bool url_test(std::string u,
              std::string scheme,
              std::string host,
              std::string port,
              std::string path,
              std::string query,
              std::string fragment)
{
    tagd::url a(u);

    if (a.scheme() != scheme) {
        std::cerr << "url_test: " << a.scheme() << " != " << scheme << std::endl;
        return false;
    }

    if (a.host() != host) {
        std::cerr << "url_test: " << a.host() << " != " << host << std::endl;
        return false;
    }

    if (a.port() != port) {
        std::cerr << "url_test: " << a.port() << " != " << port << std::endl;
        return false;
    }

    if (a.path() != path) {
        std::cerr << "url_test: " << a.path() << " != " << path << std::endl;
        return false;
    }

    if (a.query() != query) {
        std::cerr << "url_test: " << a.query() << " != " << query << std::endl;
        return false;
    }

    if (a.fragment() != fragment) {
        std::cerr << "url_test: " << a.fragment() << " != " << fragment << std::endl;
        return false;
    }

    return true;
}

// returns true if each of the url compenents were parsed identically
bool url_user_test(std::string u,
              std::string scheme,
              std::string user,
              std::string pass,
              std::string host,
              std::string port,
              std::string path,
              std::string query,
              std::string fragment)
{
    tagd::url a(u);

    if (a.scheme() != scheme) {
        std::cerr << "url_test: " << a.scheme() << " != " << scheme << std::endl;
        return false;
    }

    if (a.user() != user) {
        std::cerr << "url_test: " << a.user() << " != " << user << std::endl;
        return false;
    }

    if (a.pass() != pass) {
        std::cerr << "url_test: " << a.pass() << " != " << pass << std::endl;
        return false;
    }

    if (a.host() != host) {
        std::cerr << "url_test: " << a.host() << " != " << host << std::endl;
        return false;
    }

    if (a.port() != port) {
        std::cerr << "url_test: " << a.port() << " != " << port << std::endl;
        return false;
    }

    if (a.path() != path) {
        std::cerr << "url_test: " << a.path() << " != " << path << std::endl;
        return false;
    }

    if (a.query() != query) {
        std::cerr << "url_test: " << a.query() << " != " << query << std::endl;
        return false;
    }

    if (a.fragment() != fragment) {
        std::cerr << "url_test: " << a.fragment() << " != " << fragment << std::endl;
        return false;
    }

    return true;
}


class Tester : public CxxTest::TestSuite {
	public:

    void test_simple(void) {
        tagd::url a("http://example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://example.com" );
//  rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!!!!http" )

		tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!!!!http" )
    }

    void test_hduri_ctor(void) {
        tagd::url a("http://example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!!!!http" )

		auto hduri = a.hduri();
        TS_ASSERT_EQUALS( hduri, "hd:com!example!!!!!!!!http" )
		tagd::url b(hduri);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(b.code()) , "TAGD_OK");
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!!!!http" )
    }

    void test_ok(void) {
        tagd::url a("http://example.com");
        TS_ASSERT( a.ok() );

        tagd::url b;
        TS_ASSERT( !b.ok() );
    }

    void test_slash_path(void) {
        tagd::url a("http://example.com/");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/!!!!!!http" )

		tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/!!!!!!http" )
    }

    void test_sub_slash_path(void) {
        tagd::url a("http://a.b.c.example.com/");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "a.b.c.example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.id(), "http://a.b.c.example.com/" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!c.b.a!/!!!!!!http" )

		tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "a.b.c.example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.id(), "http://a.b.c.example.com/" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!c.b.a!/!!!!!!http" )
    }

    void test_path(void) {
        tagd::url a("http://example.com/a/b/c");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!!!!!!http" )
    }

    void test_encode_hduri_delim(void) {
		TS_ASSERT_EQUALS( tagd::encode_hduri_delim("!"), "%21" );
		TS_ASSERT_EQUALS( tagd::encode_hduri_delim("some!label!!to"), "some%21label%21%21to" );
		TS_ASSERT_EQUALS( tagd::encode_hduri_delim("/!path/to"), "/%21path/to" );
	}

    void test_decode_hduri_delim(void) {
		TS_ASSERT_EQUALS( tagd::decode_hduri_delim("%21"), "!" );
		TS_ASSERT_EQUALS( tagd::decode_hduri_delim("some%21label%21%21to"), "some!label!!to" );
		TS_ASSERT_EQUALS( tagd::decode_hduri_delim("/%21path/to"), "/!path/to" );
	}

    void test_tilde_path(void) {
        tagd::url a("http://example.com/~user/path/to");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/~user/path/to" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/~user/path/to" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/~user/path/to!!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/~user/path/to" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/~user/path/to" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/~user/path/to!!!!!!http" )
    }

    void test_bang_path(void) {
        tagd::url a("http://example.com/!path/to");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/!path/to" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/!path/to" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/%21path/to!!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/!path/to" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/!path/to" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/%21path/to!!!!!!http" )
    }

    void test_no_path_query(void) {
        tagd::url a("http://example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!?a=1&b=2!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!?a=1&b=2!!!!!http" )
    }

    void test_slash_path_query(void) {
        tagd::url a("http://example.com/?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/!?a=1&b=2!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/!?a=1&b=2!!!!!http" )
    }

    void test_path_query(void) {
        tagd::url a("http://example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!!!http" )
    }

    void test_path_slash_query(void) {
        tagd::url a("http://example.com/a/b/c/?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c/" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/a/b/c/?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c/!?a=1&b=2!!!!!http" )

        tagd::HDURI b(a.hduri());

        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c/" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com/a/b/c/?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c/!?a=1&b=2!!!!!http" )
    }

    void test_empty_query(void) {
        tagd::url a("http://example.com?");
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
		// the '?' is retained query strings, otherwise we would have no
		// in the hduri to determine between "http://example.com?" and "http://example.com"
        TS_ASSERT_EQUALS( a.query(),  "?" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!?!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(),  "?" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!?!!!!!http" )
    }

    void test_path_empty_query(void) {
        tagd::url a("http://example.com/a/b/c?");
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(),  "?" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!?!!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(),  "?" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!?!!!!!http" )
    }

    void test_simple_port(void) {
		//                     1         2
		//           01234567890123456789012
        tagd::url a("http://example.com:8080");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com:8080" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!8080!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com:8080" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!8080!!!http" )
    }

    void test_port_path(void) {
        tagd::url a("http://example.com:8080/a/b/c");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com:8080/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!!!8080!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com:8080/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!!!8080!!!http" )
    }

    void test_port_query(void) {
        tagd::url a("http://example.com:8080?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com:8080?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!?a=1&b=2!!8080!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com:8080?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!?a=1&b=2!!8080!!!http" )
    }

    void test_fragment(void) {
        tagd::url a("http://example.com#here");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.fragment(), "#here" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com#here" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!#here!!!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.fragment(), "#here" );
        TS_ASSERT_EQUALS( b.id(), "http://example.com#here" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!#here!!!!http" )
    }

	//  TODO maybe - test between NULL fragments and blank ones 

    void test_simple_user(void) {
        tagd::url a("http://joe@example.com");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://joe@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!!joe!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://joe@example.com" );
		// rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!!joe!!http" )
    }

    void test_sub_user(void) {
        tagd::url a("http://joe@www.example.com");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "www.example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://joe@www.example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!www!!!!!joe!!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "www.example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://joe@www.example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!www!!!!!joe!!http" )
    }

    void test_simple_user_pass(void) {
        tagd::url a("http://joe:bl0w@example.com");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://joe:bl0w@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!!joe!bl0w!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://joe:bl0w@example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!!joe!bl0w!http" )
    }

    void test_user_blank_pass(void) {
        tagd::url a("http://joe:@example.com");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://joe:@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!!!!joe!%00!http" );

		tagd::HDURI b(a.hduri());

        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://joe:@example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!!!!joe!%00!http" );
    }

    void test_sub_user_blank_pass(void) {
        tagd::url a("http://joe:@www.example.com");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "www.example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.id(), "http://joe:@www.example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!www!!!!!joe!%00!http" );

		tagd::HDURI b(a.hduri());

        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "www.example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.id(), "http://joe:@www.example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!www!!!!!joe!%00!http" );
    }

    void test_simple_user_pass_path(void) {
        tagd::url a("http://joe:bl0w@example.com/a/b/c");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.id(), "http://joe:bl0w@example.com/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!!!!joe!bl0w!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.id(), "http://joe:bl0w@example.com/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!!!!joe!bl0w!http" )
    }

    void test_simple_user_pass_query(void) {
        tagd::url a("http://joe:bl0w@example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://joe:bl0w@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!?a=1&b=2!!!joe!bl0w!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://joe:bl0w@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!?a=1&b=2!!!joe!bl0w!http" )
    }

    void test_simple_user_pass_path_query(void) {
        tagd::url a("http://joe:bl0w@example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://joe:bl0w@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!joe!bl0w!http" )

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://joe:bl0w@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!joe!bl0w!http" )
    }

    void test_simple_user_blank_pass_query(void) {
        tagd::url a("http://joe:@example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://joe:@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!!?a=1&b=2!!!joe!%00!http" );

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://joe:@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!!?a=1&b=2!!!joe!%00!http" );
    }

    void test_simple_user_blank_pass_path_query(void) {
        tagd::url a("http://joe:@example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://joe:@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!joe!%00!http" );

        tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.id(), "http://joe:@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:com!example!!/a/b/c!?a=1&b=2!!!joe!%00!http" );
    }

    void test_init_hduri_dash_number_path(void) {
		std::string url = "https://en.wikipedia.org/wiki/-99Dog";
		std::string hduri = "hd:org!wikipedia!en!/wiki/-99Dog!!!!!!https";

        tagd::url a(url);
        TS_ASSERT( !a.empty() );
		TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "https" );
        TS_ASSERT_EQUALS( a.user(), "" );
        TS_ASSERT_EQUALS( a.pass(), "" );
        TS_ASSERT_EQUALS( a.host(), "en.wikipedia.org" );
        TS_ASSERT_EQUALS( a.path(), "/wiki/-99Dog" );
        TS_ASSERT_EQUALS( a.query(), "" );
        TS_ASSERT_EQUALS( a.id(), url );
        TS_ASSERT_EQUALS( a.hduri(), hduri );

        tagd::HDURI b(hduri);
        TS_ASSERT( !b.empty() );
		TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "https" );
        TS_ASSERT_EQUALS( b.user(), "" );
        TS_ASSERT_EQUALS( b.pass(), "" );
        TS_ASSERT_EQUALS( b.host(), "en.wikipedia.org" );
        TS_ASSERT_EQUALS( b.path(), "/wiki/-99Dog" );
        TS_ASSERT_EQUALS( b.query(), "" );
        TS_ASSERT_EQUALS( b.id(), url );
        TS_ASSERT_EQUALS( b.hduri(), hduri );
    }

	void test_hduri(void) {
		std::string hduri = "hd:com!example!!/a/b/c/!?a=1&b=2!!!!!http";
		tagd::HDURI a(hduri);
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c/" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.id(), "http://example.com/a/b/c/?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), hduri );
    }

	void test_query_map(void) {
		url_query_map_t qm, tm;
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, ""), 0 );
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?"), 0 );
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "&"), 0 );

		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a"), 1 );
		tm = {
			{"a", ""}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a="), 1 );
		tm = {
			{"a", ""}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a&"), 1 );
		tm = {
			{"a", ""}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a&b"), 2 );
		tm = {
			{"a", ""},
			{"b", ""}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=&"), 1 );
		tm = {
			{"a", ""}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		tm = {
			{"a", "1"},
			{"b", "2"},
			{"yo", "hey"}
		};
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=1&b=2&yo=hey"), 3 );
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		// no leading '?'
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "a=1&b=2&yo=hey"), 3 );
		tm = {
			{"a", "1"},
			{"b", "2"},
			{"yo", "hey"}
		};
		TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		tm = {
			{"a", "1"},
			{"b", ""},
			{"yo", "hey"}
		};
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=1&b=&yo=hey"), 3 );
		TS_ASSERT_EQUALS( qm, tm );

		// haven't decided how this should work
		// qm.clear();
		// TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=1&b=?&yo=hey"), 3 );
		// tm = {
		// 	{"a", "1"},
		// 	{"b", "?"},
		// 	{"yo", "hey"}
		// };
		// TS_ASSERT_EQUALS( qm, tm );

		qm.clear();
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=1&b&yo=hey"), 3 );
		tm = {
			{"a", "1"},
			{"b", ""},
			{"yo", "hey"}
		};
		TS_ASSERT_EQUALS( qm, tm );
	}

	void test_query_map_endcoded(void) {
		url_query_map_t qm, tm;

		// TODO be comprehensive

		// test '+' => ' '
		tm = {
			{"a", "1"},
			{"b", ""},
			{"yo", "hey bro"}
		};
		TS_ASSERT_EQUALS( tagd::url::parse_query(qm, "?a=1&b=&yo=hey+bro"), 3 );
		TS_ASSERT_EQUALS( qm, tm );
	}

	void test_query_find(void) {
		url_query_map_t qm;
		size_t n = tagd::url::parse_query(qm, "?a=1&b=2&yo=hey");
		TS_ASSERT_EQUALS( n , 3 );
		std::string val;
		TS_ASSERT( tagd::url::query_find(qm, val, "a") );
		TS_ASSERT_EQUALS( val , "1" );
		TS_ASSERT( tagd::url::query_find(qm, val, "b") );
		TS_ASSERT_EQUALS( val , "2" );
		TS_ASSERT( tagd::url::query_find(qm, val, "yo") );
		TS_ASSERT_EQUALS( val , "hey" );
		TS_ASSERT( !tagd::url::query_find(qm, val, "caca") );
	}

	void test_looks_like(void) {
		TS_ASSERT( tagd::url::looks_like_url("http://example.com") )
        TS_ASSERT( tagd::url::looks_like_hduri("hd:com!example!http") )

        TS_ASSERT( tagd::url::looks_like_url("http://example.com/?a=1&b=2") )
        TS_ASSERT( tagd::url::looks_like_hduri("hd:com!example!!/!?a=1&b=2!http") )

		TS_ASSERT( tagd::url::looks_like_url("http://localhost:2112/each:a?v=browse.html") )
		TS_ASSERT( !tagd::url::looks_like_hduri("http://localhost:2112/each:a?v=browse.html") )
		TS_ASSERT( !tagd::url::looks_like_hduri("hd:each!a") )
	}

	void test_percent_encode(void) {
		TS_ASSERT(tagd::uri_encode("ABC") == "ABC");

		std::string src1(u8"イヌ");
		std::string enc1("%E3%82%A4%E3%83%8C");

		TS_ASSERT_EQUALS(tagd::uri_encode(src1) , enc1);
		TS_ASSERT_EQUALS(tagd::uri_decode(enc1) , src1);

		const std::string src("\0\1\2", 3);
		const std::string enc("%00%01%02");
		TS_ASSERT_EQUALS(tagd::uri_encode(src) , enc);
		TS_ASSERT_EQUALS(tagd::uri_decode(enc) , src);

		TS_ASSERT_EQUALS(tagd::uri_encode("\xFF") , "%FF");
		TS_ASSERT_EQUALS(tagd::uri_decode("%FF") , "\xFF");
		TS_ASSERT_EQUALS(tagd::uri_decode("%ff") , "\xFF");

		// unsafe chars test, RFC1738
		const std::string UNSAFE(" <>#{}|\\^~[]`");
		std::string unsafe_enc = tagd::uri_encode(UNSAFE);
		TS_ASSERT_EQUALS(std::string::npos , unsafe_enc.find_first_of(UNSAFE));
		TS_ASSERT_EQUALS(tagd::uri_decode(unsafe_enc) , UNSAFE);

		// random test
		const int MAX_LEN = 128;
		char a[MAX_LEN];
		srand((unsigned)time(NULL));
		for (int i = 0; i < 100; i++)
		{
			for (int j = 0; j < MAX_LEN; j++)
				a[j] = rand() % (1 << 8);
			int l = rand() % MAX_LEN;
			std::string o(a, l);
			std::string e = tagd::uri_encode(o);
			TS_ASSERT_EQUALS(o , tagd::uri_decode(e));
		}
	}

    void test_relations(void) {
		tagd::url a("http://hypermega.com");
		tagd::url::insert_url_part_relations(a.relations, a);
		TS_ASSERT( a.related(HARD_TAG_HAS, HARD_TAG_HOST, "hypermega.com") )
		TS_ASSERT( a.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "hypermega") )
		TS_ASSERT( a.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "com") )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_PATH) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_QUERY) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_PORT) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_USER) )
		TS_ASSERT( !a.related(HARD_TAG_HAS, HARD_TAG_PASS) )
		TS_ASSERT( a.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http") )

		tagd::url b("http://www.hypermega.com#here");
		tagd::url::insert_url_part_relations(b.relations, b);
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_HOST, "www.hypermega.com") )
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "hypermega") )
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "com") )
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, "www") )
		TS_ASSERT( !b.related(HARD_TAG_HAS, HARD_TAG_PATH) )
		TS_ASSERT( !b.related(HARD_TAG_HAS, HARD_TAG_QUERY) )
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT, "#here") )
		TS_ASSERT( !b.related(HARD_TAG_HAS, HARD_TAG_PORT) )
		TS_ASSERT( !b.related(HARD_TAG_HAS, HARD_TAG_USER) )
		TS_ASSERT( !b.related(HARD_TAG_HAS, HARD_TAG_PASS) )
		TS_ASSERT( b.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http") )

		tagd::url c("http://en.wikipedia.org/wiki/Dog");
		tagd::url::insert_url_part_relations(c.relations, c);
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_HOST, "en.wikipedia.org") )
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_PRIV_LABEL, "wikipedia") )
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_PUBLIC, "org") )
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_SUBDOMAIN, "en") )
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_PATH, "/wiki/Dog") )
		TS_ASSERT( !c.related(HARD_TAG_HAS, HARD_TAG_QUERY) )
		TS_ASSERT( !c.related(HARD_TAG_HAS, HARD_TAG_FRAGMENT) )
		TS_ASSERT( !c.related(HARD_TAG_HAS, HARD_TAG_PORT) )
		TS_ASSERT( !c.related(HARD_TAG_HAS, HARD_TAG_USER) )
		TS_ASSERT( !c.related(HARD_TAG_HAS, HARD_TAG_PASS) )
		TS_ASSERT( c.related(HARD_TAG_HAS, HARD_TAG_SCHEME, "http") )
	}

    void test_urls(void) {
        // Borrowed from https://github.com/chadmyers/UrlRegex/blob/master/start.html
        // Thanks John Gruber
        TS_ASSERT( url_test("http://www.RegExr.com", "http", "www.regexr.com", "", "", "", "") );
        TS_ASSERT( url_test("http://RegExr.com", "http", "regexr.com", "", "", "", "") );
        TS_ASSERT( url_test("http://www.RegExr.com?2rjl6", "http", "www.regexr.com", "", "", "?2rjl6", "") );
        TS_ASSERT( url_test("http://www.RegExr.com?2rjl6(v85)&x=99&498", "http", "www.regexr.com", "", "", "?2rjl6(v85)&x=99&498", "") );
        TS_ASSERT( url_test("http://RegExr.com#foo", "http", "regexr.com", "", "", "", "#foo") );
        TS_ASSERT( url_test("http://RegExr.com#foo(bar)", "http", "regexr.com", "", "", "", "#foo(bar)") );
        TS_ASSERT( url_test("http://foo.com/blah_blah", "http", "foo.com", "", "/blah_blah", "", "") );
        TS_ASSERT( url_test("http://foo.com/blah_blah_(wikipedia)", "http", "foo.com", "", "/blah_blah_(wikipedia)", "", "") );
        TS_ASSERT( url_test("http://foo.com/blah_(wikipedia)_blah#cite-1", "http", "foo.com", "", "/blah_(wikipedia)_blah", "", "#cite-1") );
        TS_ASSERT( url_test("http://foo.com/unicode_(✪)_in_parens", "http", "foo.com", "", "/unicode_(✪)_in_parens", "", "") );
        TS_ASSERT( url_test("http://foo.com/(something)?after=parens", "http", "foo.com", "", "/(something)", "?after=parens", "") );
        TS_ASSERT( url_test("http://✪df.ws/1234", "http", "✪df.ws", "", "/1234", "", "") );
        TS_ASSERT( url_test("rdar://1234", "rdar", "1234", "", "", "", "") );
        // host gets lowered
        TS_ASSERT( url_test("x-yojimbo-item://6303E4C1-6A6E-45A6-AB9D-3A908F59AE0E", "x-yojimbo-item", "6303e4c1-6a6e-45a6-ab9d-3a908f59ae0e", "", "", "", "") );
        TS_ASSERT( url_user_test("message://%3c330e7f840905021726r6a4ba78dkf1fd71420c1bf6ff@mail.gmail.com%3e", "message", "%3c330e7f840905021726r6a4ba78dkf1fd71420c1bf6ff", "", "mail.gmail.com%3e", "", "", "", "") );
        TS_ASSERT( url_test("http://➡.ws/䨹", "http", "➡.ws", "", "/䨹", "", "") );
        TS_ASSERT( url_test("http://example.com/something?with,commas,in,url", "http", "example.com", "", "/something", "?with,commas,in,url", "") );
        TS_ASSERT( url_user_test("mailto:gruber@daringfireball.net?subject=TEST", "mailto", "gruber", "", "daringfireball.net", "", "", "?subject=TEST", "") );
        TS_ASSERT( url_test("http://www.asianewsphoto.com/(S(neugxif4twuizg551ywh3f55))/Web_ENG/View_DetailPhoto.aspx?PicId=752", "http", "www.asianewsphoto.com", "", "/(S(neugxif4twuizg551ywh3f55))/Web_ENG/View_DetailPhoto.aspx", "?PicId=752", "") );
        TS_ASSERT( url_test("http://lcweb2.loc.gov/cgi-bin/query/h?pp/horyd:@field(NUMBER+@band(thc+5a46634))", "http", "lcweb2.loc.gov", "", "/cgi-bin/query/h", "?pp/horyd:@field(NUMBER+@band(thc+5a46634))", "") );
    }

    void test_file_uri(void) {
        tagd::url a("file:///favicon.ico");
        TS_ASSERT( !a.empty() );
        TS_ASSERT( a.ok() )
        TS_ASSERT_EQUALS( a.scheme(), "file" );
        TS_ASSERT( a.host().empty() );
        TS_ASSERT_EQUALS( a.path(), "/favicon.ico" );
        TS_ASSERT_EQUALS( a.id(), "file:///favicon.ico" );
        TS_ASSERT_EQUALS( a.hduri(), "hd:!!!/favicon.ico!!!!!!file" );

		tagd::HDURI b(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT( b.ok() )
        TS_ASSERT_EQUALS( b.scheme(), "file" );
        TS_ASSERT( b.host().empty() );
        TS_ASSERT_EQUALS( b.path(), "/favicon.ico" );
        TS_ASSERT_EQUALS( b.id(), "file:///favicon.ico" );
        TS_ASSERT_EQUALS( b.hduri(), "hd:!!!/favicon.ico!!!!!!file" );

		// local path
        tagd::url c("file:///somefile.mp4?w=full&qv=hd#o=2112");
        TS_ASSERT( !c.empty() );
        TS_ASSERT( c.ok() )
        TS_ASSERT_EQUALS( c.scheme(), "file" );
        TS_ASSERT( c.host().empty() );
        TS_ASSERT_EQUALS( c.path(), "/somefile.mp4" );
        TS_ASSERT_EQUALS( c.query(), "?w=full&qv=hd" );
        TS_ASSERT_EQUALS( c.fragment(), "#o=2112" );
        TS_ASSERT_EQUALS( c.id(), "file:///somefile.mp4?w=full&qv=hd#o=2112" );
        // TODO local-path : everything after authority
		// TS_ASSERT_EQUALS( c.local_path(), "/somefile.mp4?w=full&qv=hd#o=2112" );
		// rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
        TS_ASSERT_EQUALS( c.hduri(), "hd:!!!/somefile.mp4!?w=full&qv=hd!#o=2112!!!!file" );
    }
};
