// Test suites

#include <cxxtest/TestSuite.h>

#include "tagd.h"

// returns true if each of the url compenents were parsed identically 
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
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://example.com" );
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
        TS_ASSERT_EQUALS( a.hduri(), "com:example:http" )
        TS_ASSERT_EQUALS( a.id(),  a.hduri() )  // hduri is uri identity

		// std::cout << a << std::endl;
		// com:example:http _is_a _url
		// has	_rpub	com:dns,
		//    	_private_dns_label	example,
		//    	_scheme	http 

		tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://example.com" );
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
        TS_ASSERT_EQUALS( b.hduri(), "com:example:http" )
        TS_ASSERT_EQUALS( b.id(),  b.hduri() )  // hduri is uri identity
    }

    void test_hdurl_ctor(void) {
        tagd::url a("http://example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://example.com" );
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
        TS_ASSERT_EQUALS( a.hduri(), "com:example:http" )
        TS_ASSERT_EQUALS( a.id(),  a.hduri() )  // hduri is uri identity
		// std::cout << a << std::endl;

		std::string hdurl("hduri://");
		hdurl.append(a.hduri());

		tagd::url b(hdurl);
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://example.com" );
//  rpub:priv_label:rsub:path:query:fragment:port:user:pass:scheme
        TS_ASSERT_EQUALS( b.hduri(), "com:example:http" )
        TS_ASSERT_EQUALS( b.id(),  b.hduri() )  // hduri is uri identity
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
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com/" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/:http" )
		// std::cout << a << std::endl;

		tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com/" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/:http" );
    }

    void test_sub_slash_path(void) {
        tagd::url a("http://a.b.c.example.com/");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "a.b.c.example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.str(), "http://a.b.c.example.com/" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:c.b.a:/:http" )
		// std::cout << a << std::endl;

		tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "a.b.c.example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.str(), "http://a.b.c.example.com/" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:c.b.a:/:http" );
    }

    void test_path(void) {
        tagd::url a("http://example.com/a/b/c");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:http" )
    }

    void test_no_path_query(void) {
        tagd::url a("http://example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::?a=1&b=2:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::?a=1&b=2:http" )

    }

    void test_slash_path_query(void) {
        tagd::url a("http://example.com/?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com/?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/:?a=1&b=2:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com/?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/:?a=1&b=2:http" )
    }

    void test_path_query(void) {
        tagd::url a("http://example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:?a=1&b=2:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:?a=1&b=2:http" )
    }

    void test_path_slash_query(void) {
        tagd::url a("http://example.com/a/b/c/?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c/" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com/a/b/c/?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c/:?a=1&b=2:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());

        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c/" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com/a/b/c/?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c/:?a=1&b=2:http" )
    }

    void test_empty_query(void) {
        tagd::url a("http://example.com?");
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
		// the '?' is retained query strings, otherwise we would have no
		// in the hduri to determine between "http://example.com?" and "http://example.com"
        TS_ASSERT_EQUALS( a.query(),  "?" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::?:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
		// the '?' is retained query strings, otherwise we would have no
		// in the hduri to determine between "http://example.com?" and "http://example.com"
        TS_ASSERT_EQUALS( b.query(),  "?" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::?:http" )
    }

    void test_path_empty_query(void) {
        tagd::url a("http://example.com/a/b/c?");
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(),  "?" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:?:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(),  "?" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:?:http" )
    }

    void test_simple_port(void) {
		//                     1         2
		//           01234567890123456789012
        tagd::url a("http://example.com:8080");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com:8080" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::::8080:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com:8080" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::::8080:http" )
    }

    void test_port_path(void) {
        tagd::url a("http://example.com:8080/a/b/c");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com:8080/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:::8080:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com:8080/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:::8080:http" )
    }

    void test_port_query(void) {
        tagd::url a("http://example.com:8080?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.port(), "8080" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com:8080?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::?a=1&b=2::8080:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.port(), "8080" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com:8080?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::?a=1&b=2::8080:http" )
    }

    void test_fragment(void) {
        tagd::url a("http://example.com#here");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.fragment(), "here" );
        TS_ASSERT_EQUALS( a.str(), "http://example.com#here" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::::here:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.fragment(), "here" );
        TS_ASSERT_EQUALS( b.str(), "http://example.com#here" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::::here:http" )
    }

	//  TODO maybe
	//  - we have of telling between NULL fragments and blank ones (like below)
//    void test_blank_fragment(void) {
//        tagd::url a("http://example.com#");
//        TS_ASSERT( !a.empty() );
//        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
//        TS_ASSERT_EQUALS( a.scheme(), "http" );
//        TS_ASSERT_EQUALS( a.host(), "example.com" );
//        TS_ASSERT( a.path().empty() );
//        TS_ASSERT( a.fragment().empty() );
//        TS_ASSERT_EQUALS( a.str(), "http://example.com#" );
//        TS_ASSERT_EQUALS( a.hduri(), "com:example::::::::http" )
//		std::cout << "a.debug(" << __LINE__ << "):" << std::std::endl;
//		a.debug();

//        tagd::url b;
//		b.init_hduri(a.hduri());
//		std::cout << "b.debug(" << __LINE__ << "):" << std::std::endl;
//		b.debug();
//        TS_ASSERT( !b.empty() );
//        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
 //       TS_ASSERT_EQUALS( b.scheme(), "http" );
 //       TS_ASSERT_EQUALS( b.host(), "example.com" );
 //       TS_ASSERT( b.path().empty() );
//        TS_ASSERT( b.fragment().empty() );
//        TS_ASSERT_EQUALS( b.str(), "http://example.com#" );
//        TS_ASSERT_EQUALS( b.hduri(), "com:example::::::::http" )
//    }

    void test_simple_user(void) {
        tagd::url a("http://joe@example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://joe@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::::::joe:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://joe@example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::::::joe:http" )
    }

    void test_sub_user(void) {
        tagd::url a("http://joe@www.example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "www.example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://joe@www.example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:www:::::joe:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "www.example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://joe@www.example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:www:::::joe:http" )
    }

    void test_simple_user_pass(void) {
        tagd::url a("http://joe:bl0w@example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://joe:bl0w@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::::::joe:bl0w:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://joe:bl0w@example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::::::joe:bl0w:http" )
    }

    void test_user_blank_pass(void) {
        tagd::url a("http://joe:@example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://joe:@example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::::::joe::http" );
		// std::cout << a << std::endl;

		tagd::url b;
		b.init_hduri(a.hduri());

        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://joe:@example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::::::joe::http" );
    }

    void test_sub_user_blank_pass(void) {
        tagd::url a("http://joe:@www.example.com");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "www.example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.str(), "http://joe:@www.example.com" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:www:::::joe::http" );
		// std::cout << a << std::endl;

		tagd::url b;
		b.init_hduri(a.hduri());

        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "www.example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.str(), "http://joe:@www.example.com" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:www:::::joe::http" );
    }

    void test_simple_user_pass_path(void) {
        tagd::url a("http://joe:bl0w@example.com/a/b/c");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.str(), "http://joe:bl0w@example.com/a/b/c" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c::::joe:bl0w:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.str(), "http://joe:bl0w@example.com/a/b/c" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c::::joe:bl0w:http" )
    }

    void test_simple_user_pass_query(void) {
        tagd::url a("http://joe:bl0w@example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://joe:bl0w@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::?a=1&b=2:::joe:bl0w:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://joe:bl0w@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::?a=1&b=2:::joe:bl0w:http" )
    }

    void test_simple_user_pass_path_query(void) {
        tagd::url a("http://joe:bl0w@example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT_EQUALS( a.pass(), "bl0w" );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://joe:bl0w@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:?a=1&b=2:::joe:bl0w:http" )
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT_EQUALS( b.pass(), "bl0w" );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://joe:bl0w@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:?a=1&b=2:::joe:bl0w:http" )
    }

    void test_simple_user_blank_pass_query(void) {
        tagd::url a("http://joe:@example.com?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT( a.path().empty() );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://joe:@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example:::?a=1&b=2:::joe::http" );
		// std::cout << a << std::endl;
		
        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT( b.path().empty() );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://joe:@example.com?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example:::?a=1&b=2:::joe::http" );
    }

    void test_simple_user_blank_pass_path_query(void) {
        tagd::url a("http://joe:@example.com/a/b/c?a=1&b=2");
        TS_ASSERT( !a.empty() );
        TS_ASSERT_EQUALS( url_code_str(a.code()), "URL_OK" );
        TS_ASSERT_EQUALS( a.scheme(), "http" );
        TS_ASSERT_EQUALS( a.user(), "joe" );
        TS_ASSERT( a.pass().empty() );
        TS_ASSERT_EQUALS( a.host(), "example.com" );
        TS_ASSERT_EQUALS( a.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( a.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( a.str(), "http://joe:@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( a.hduri(), "com:example::/a/b/c:?a=1&b=2:::joe::http" );
		// std::cout << a << std::endl;

        tagd::url b;
		b.init_hduri(a.hduri());
        TS_ASSERT( !b.empty() );
        TS_ASSERT_EQUALS( url_code_str(b.code()), "URL_OK" );
        TS_ASSERT_EQUALS( b.scheme(), "http" );
        TS_ASSERT_EQUALS( b.user(), "joe" );
        TS_ASSERT( b.pass().empty() );
        TS_ASSERT_EQUALS( b.host(), "example.com" );
        TS_ASSERT_EQUALS( b.path(), "/a/b/c" );
        TS_ASSERT_EQUALS( b.query(), "?a=1&b=2" );
        TS_ASSERT_EQUALS( b.str(), "http://joe:@example.com/a/b/c?a=1&b=2" );
        TS_ASSERT_EQUALS( b.hduri(), "com:example::/a/b/c:?a=1&b=2:::joe::http" );
    }

    void test_urls(void) {
        // Borrowed from https://github.com/chadmyers/UrlRegex/blob/master/start.html
        // Thanks John Gruber
        TS_ASSERT( url_test("http://www.RegExr.com", "http", "www.regexr.com", "", "", "", "") );
        TS_ASSERT( url_test("http://RegExr.com", "http", "regexr.com", "", "", "", "") );
        TS_ASSERT( url_test("http://www.RegExr.com?2rjl6", "http", "www.regexr.com", "", "", "?2rjl6", "") );
        TS_ASSERT( url_test("http://www.RegExr.com?2rjl6(v85)&x=99&498", "http", "www.regexr.com", "", "", "?2rjl6(v85)&x=99&498", "") );
        TS_ASSERT( url_test("http://RegExr.com#foo", "http", "regexr.com", "", "", "", "foo") );
        TS_ASSERT( url_test("http://RegExr.com#foo(bar)", "http", "regexr.com", "", "", "", "foo(bar)") );
        TS_ASSERT( url_test("http://foo.com/blah_blah", "http", "foo.com", "", "/blah_blah", "", "") );
        TS_ASSERT( url_test("http://foo.com/blah_blah_(wikipedia)", "http", "foo.com", "", "/blah_blah_(wikipedia)", "", "") );
        TS_ASSERT( url_test("http://foo.com/blah_(wikipedia)_blah#cite-1", "http", "foo.com", "", "/blah_(wikipedia)_blah", "", "cite-1") );
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
};
