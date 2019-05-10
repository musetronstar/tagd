// Test suites

#include <cxxtest/TestSuite.h>

#include "tagd.h"

// takes in a domain and returns the the portion of the
// domain where setting cookies would be allowed
// (according to the public suffix list)
// returns NULL if setting cookies would not be allowed
std::string public_suffix(const char *dom) {
	if (dom == nullptr) return "NULL";
	if (dom[0] == '.') return "NULL";

	tagd::domain d(dom);

	switch (d.code()) {
		case TLD_UNKNOWN: {
			size_t i = strlen(dom);
			bool got_dot = false;
			while (i > 0) {
				if (dom[i] == '.') {
					if (got_dot)
						return std::string(&dom[++i]);
					got_dot = true;
				}
				i--;
			}
			return (got_dot ? std::string(&dom[i]) : "NULL");
		}
		case TLD_ICANN_REG:
		case TLD_WILDCARD_REG:
		case TLD_EXCEPTION_REG:
		case TLD_PRIVATE_REG:
			return std::string(d.reg());
		case TLD_ICANN:
		case TLD_WILDCARD:
		case TLD_PRIVATE:
		case TLD_ERR:
		default:
			return "NULL";
	}
}

class Tester : public CxxTest::TestSuite {
	public:

    void test_domain_cons(void) {
        tagd::domain d("www.example.com");
        TS_ASSERT( !d.empty() );
        TS_ASSERT( d.is_registrable() );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN_REG" );
        TS_ASSERT_EQUALS( d.pub(), "com" );
        TS_ASSERT_EQUALS( d.reg(), "example.com" );
        TS_ASSERT_EQUALS( d.priv(), "www.example" );
        TS_ASSERT_EQUALS( d.priv_label(), "example" );
        TS_ASSERT_EQUALS( d.sub(), "www" );
        TS_ASSERT_EQUALS( d.str(), "www.example.com" );
    }

    void test_domain_init(void) {
        tagd::domain d;
        TS_ASSERT( d.empty() );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_UNKNOWN" );

        d.init("www.example.com");
        TS_ASSERT( d.is_registrable() );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN_REG" );
        TS_ASSERT_EQUALS( d.pub(), "com" );
        TS_ASSERT_EQUALS( d.reg(), "example.com" );
        TS_ASSERT_EQUALS( d.priv(), "www.example" );
        TS_ASSERT_EQUALS( d.priv_label(), "example" );
        TS_ASSERT_EQUALS( d.str(), "www.example.com" );
    }

    void test_domain_uppercase(void) {
		const char* strs[] = {
			"EXAMPLE.COM",
			"EXAMPLE.com",
			"eXaMpLe.Com",
			"example.COM"
		};

		for (auto s : strs) {
			tagd::domain d(s);
			TS_ASSERT( !d.empty() );
			TS_ASSERT( d.is_registrable() );
			TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN_REG" );
			TS_ASSERT_EQUALS( d.pub(), "com" );
			TS_ASSERT_EQUALS( d.reg(), "example.com" );
			TS_ASSERT_EQUALS( d.priv(), "example" );
			TS_ASSERT_EQUALS( d.priv_label(), "example" );
			TS_ASSERT_EQUALS( d.sub(), "" );
			TS_ASSERT_EQUALS( d.str(), "example.com" );
		}
    }

    void test_garbage(void) {
        tagd::domain a("COM");
        TS_ASSERT_EQUALS( tld_code_str(a.code()), "TLD_ICANN" );
        TS_ASSERT_EQUALS( a.pub(), "com" );
        TS_ASSERT( !a.is_registrable() );

        // eats leading dots
        tagd::domain b(".COM");
        TS_ASSERT_EQUALS( tld_code_str(b.code()), "TLD_ICANN" );
        TS_ASSERT_EQUALS( b.pub(), "com" );
        TS_ASSERT( !b.is_registrable() );

        // won"t eat trailing dots
        tagd::domain c("example.com.");
        TS_ASSERT_EQUALS( tld_code_str(c.code()), "TLD_UNKNOWN" );
        TS_ASSERT_EQUALS( c.pub(), "" );
        TS_ASSERT( !c.is_registrable() );

        tagd::domain d("..WWW.eXample.cOm");
        TS_ASSERT( !d.empty() );
        TS_ASSERT( d.is_registrable() );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN_REG" );
        TS_ASSERT_EQUALS( d.pub(), "com" );
        TS_ASSERT_EQUALS( d.reg(), "example.com" );
        TS_ASSERT_EQUALS( d.priv(), "www.example" );
        TS_ASSERT_EQUALS( d.priv_label(), "example" );
        TS_ASSERT_EQUALS( d.str(), "www.example.com" );
        TS_ASSERT( d.is_registrable() );
    }

    void test_domain_private(void) {
        tagd::domain a("uk.com");
        TS_ASSERT_EQUALS( tld_code_str(a.code()), "TLD_PRIVATE" );
        TS_ASSERT_EQUALS( a.pub(), "uk.com" );
        TS_ASSERT_EQUALS( a.reg(), "" );
        TS_ASSERT_EQUALS( a.priv(), "" );
        TS_ASSERT_EQUALS( a.priv_label(), "" );
        TS_ASSERT_EQUALS( a.sub(), "" );
        TS_ASSERT_EQUALS( a.str(), "uk.com" );
        TS_ASSERT( !a.is_registrable() );

        tagd::domain b("example.uk.com");
        TS_ASSERT_EQUALS( tld_code_str(b.code()), "TLD_PRIVATE_REG" );
        TS_ASSERT_EQUALS( b.pub(), "uk.com" );
        TS_ASSERT_EQUALS( b.reg(), "example.uk.com" );
        TS_ASSERT_EQUALS( b.priv(), "example" );
        TS_ASSERT_EQUALS( b.priv_label(), "example" );
        TS_ASSERT_EQUALS( b.sub(), "" );
        TS_ASSERT_EQUALS( b.str(), "example.uk.com" );
        TS_ASSERT( b.is_registrable() );

        tagd::domain c("www.example.uk.com");
        TS_ASSERT_EQUALS( tld_code_str(c.code()), "TLD_PRIVATE_REG" );
        TS_ASSERT_EQUALS( c.pub(), "uk.com" );
        TS_ASSERT_EQUALS( c.reg(), "example.uk.com" );
        TS_ASSERT_EQUALS( c.priv(), "www.example" );
        TS_ASSERT_EQUALS( c.priv_label(), "example" );
        TS_ASSERT_EQUALS( c.sub(), "www" );
        TS_ASSERT_EQUALS( c.str(), "www.example.uk.com" );
        TS_ASSERT( c.is_registrable() );
    }

    void test_domain_wildcard(void) {
        tagd::domain d("ck");  // rule is *.ck, but no wildcard given
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN" );
        TS_ASSERT_EQUALS( d.pub(), "ck" );
        TS_ASSERT_EQUALS( d.reg(), "" );
        TS_ASSERT_EQUALS( d.priv(), "" );
        TS_ASSERT_EQUALS( d.priv_label(), "" );
        TS_ASSERT_EQUALS( d.sub(), "" );
        TS_ASSERT_EQUALS( d.str(), "ck" );
        TS_ASSERT( !d.is_registrable() );

        tld_code tld_c = d.init("com.ck");
        TS_ASSERT_EQUALS( tld_code_str(tld_c), "TLD_WILDCARD" );
        TS_ASSERT_EQUALS( d.pub(), "com.ck" );
        TS_ASSERT_EQUALS( d.reg(), "" );
        TS_ASSERT_EQUALS( d.priv(), "" );
        TS_ASSERT_EQUALS( d.priv_label(), "" );
        TS_ASSERT_EQUALS( d.sub(), "" );
        TS_ASSERT_EQUALS( d.str(), "com.ck" );
        TS_ASSERT( !d.is_registrable() );

        tld_c = d.init("www.example.com.ck");
        TS_ASSERT_EQUALS( tld_code_str(tld_c), "TLD_WILDCARD_REG" );
        TS_ASSERT_EQUALS( d.pub(), "com.ck" );
        TS_ASSERT_EQUALS( d.reg(), "example.com.ck" );
        TS_ASSERT_EQUALS( d.priv(), "www.example" );
        TS_ASSERT_EQUALS( d.priv_label(), "example" );
        TS_ASSERT_EQUALS( d.sub(), "www" );
        TS_ASSERT_EQUALS( d.str(), "www.example.com.ck" );
        TS_ASSERT( d.is_registrable() );
    }

    void test_domain_wildcard_exception(void) {
        tagd::domain d("www.ck");
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_EXCEPTION_REG" );
        TS_ASSERT_EQUALS( d.pub(), "ck" );
        TS_ASSERT_EQUALS( d.reg(), "www.ck" );
        TS_ASSERT_EQUALS( d.priv(), "www" );
        TS_ASSERT_EQUALS( d.priv_label(), "www" );
        TS_ASSERT_EQUALS( d.sub(), "" );
        TS_ASSERT_EQUALS( d.str(), "www.ck" );
        TS_ASSERT( d.is_registrable() );

        tld_code tld_c = d.init("example.www.ck");
        TS_ASSERT_EQUALS( tld_code_str(tld_c), "TLD_EXCEPTION_REG" );
        TS_ASSERT_EQUALS( d.pub(), "ck" );
        TS_ASSERT_EQUALS( d.reg(), "www.ck" );
        TS_ASSERT_EQUALS( d.priv(), "example.www" );
        TS_ASSERT_EQUALS( d.priv_label(), "www" );
        TS_ASSERT_EQUALS( d.sub(), "example" );
        TS_ASSERT_EQUALS( d.str(), "example.www.ck" );
        TS_ASSERT( d.is_registrable() );

        tld_c = d.init("www.example.www.ck");
        TS_ASSERT_EQUALS( tld_code_str(tld_c), "TLD_EXCEPTION_REG" );
        TS_ASSERT_EQUALS( d.pub(), "ck" );
        TS_ASSERT_EQUALS( d.reg(), "www.ck" );
        TS_ASSERT_EQUALS( d.priv(), "www.example.www" );
        TS_ASSERT_EQUALS( d.priv_label(), "www" );
        TS_ASSERT_EQUALS( d.sub(), "www.example" );
        TS_ASSERT_EQUALS( d.str(), "www.example.www.ck" );
        TS_ASSERT( d.is_registrable() );
    }

    void test_unknown(void) {
        tagd::domain d("local");
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_UNKNOWN" );
        TS_ASSERT_EQUALS( d.pub(), "" );
        TS_ASSERT_EQUALS( d.reg(), "" );
        TS_ASSERT_EQUALS( d.priv(), "local" );
        TS_ASSERT_EQUALS( d.priv_label(), "local" );
        TS_ASSERT_EQUALS( d.sub(), "" );
        TS_ASSERT_EQUALS( d.str(), "local" );
        TS_ASSERT( !d.is_registrable() );

        tld_code tld_c = d.init("example.local");
        TS_ASSERT_EQUALS( tld_code_str(tld_c), "TLD_UNKNOWN" );
        TS_ASSERT_EQUALS( d.pub(), "" );
        TS_ASSERT_EQUALS( d.reg(), "" );
        TS_ASSERT_EQUALS( d.priv(), "example.local" );
        TS_ASSERT_EQUALS( d.priv_label(), "local" );
        TS_ASSERT_EQUALS( d.sub(), "example" );
        TS_ASSERT_EQUALS( d.str(), "example.local" );
        TS_ASSERT( !d.is_registrable() );
    }

    void test_max_len(void) {
        tagd::domain d(
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            "abcdefghigklmnopqrstuvxyzABCDEFGHIGKLMNOPQRSTUVXYZ0123456789"
            ".com"
        );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_MAX_LEN" );
    }

    void test_reverse_labels(void) {
        TS_ASSERT_EQUALS(reverse_labels("example.com"), "com.example");
        TS_ASSERT_EQUALS(reverse_labels("com.example"), "example.com");

        TS_ASSERT_EQUALS(reverse_labels("www.example.com"), "com.example.www");
        TS_ASSERT_EQUALS(reverse_labels("com.example.www"), "www.example.com");

        TS_ASSERT_EQUALS(reverse_labels("local"), "local");
        TS_ASSERT_EQUALS(reverse_labels(""), "");
    }

    void test_mozilla_psl(void) {
        // Mozilla Public Suffix List Test
        // for details, see http://publicsuffix.org/list/
        // Any copyright is dedicated to the Public Domain.
        // http://creativecommons.org/publicdomain/zero/1.0/

        // NULL input.
        TS_ASSERT_EQUALS( public_suffix(NULL), "NULL" );
        // Mixed case.
        TS_ASSERT_EQUALS( public_suffix("COM"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("example.COM"), "example.com" );
        TS_ASSERT_EQUALS( public_suffix("WwW.example.COM"), "example.com" );
        // Leading dot.
        TS_ASSERT_EQUALS( public_suffix(".com"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix(".example"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix(".example.com"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix(".example.example"), "NULL" );
        // Unlisted TLD.
        TS_ASSERT_EQUALS( public_suffix("example"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("example.example"), "example.example" );
        TS_ASSERT_EQUALS( public_suffix("b.example.example"), "example.example" );
        TS_ASSERT_EQUALS( public_suffix("a.b.example.example"), "example.example" );
        // Listed, but non-Internet, TLD.
        //TS_ASSERT_EQUALS( public_suffix("local"), "NULL" );
        //TS_ASSERT_EQUALS( public_suffix("example.local"), "NULL" );
        //TS_ASSERT_EQUALS( public_suffix("b.example.local"), "NULL" );
        //TS_ASSERT_EQUALS( public_suffix("a.b.example.local"), "NULL" );
        // TLD with only 1 rule.
        TS_ASSERT_EQUALS( public_suffix("biz"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("domain.biz"), "domain.biz" );
        TS_ASSERT_EQUALS( public_suffix("b.domain.biz"), "domain.biz" );
        TS_ASSERT_EQUALS( public_suffix("a.b.domain.biz"), "domain.biz" );
        // TLD with some 2-level rules.
        TS_ASSERT_EQUALS( public_suffix("com"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("example.com"), "example.com" );
        TS_ASSERT_EQUALS( public_suffix("b.example.com"), "example.com" );
        TS_ASSERT_EQUALS( public_suffix("a.b.example.com"), "example.com" );
        TS_ASSERT_EQUALS( public_suffix("uk.com"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("example.uk.com"), "example.uk.com" );
        TS_ASSERT_EQUALS( public_suffix("b.example.uk.com"), "example.uk.com" );
        TS_ASSERT_EQUALS( public_suffix("a.b.example.uk.com"), "example.uk.com" );
        TS_ASSERT_EQUALS( public_suffix("test.ac"), "test.ac" );
        // TLD with only 1 (wildcard) rule.
        TS_ASSERT_EQUALS( public_suffix("bd"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("c.bd"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("b.c.bd"), "b.c.bd" );
        TS_ASSERT_EQUALS( public_suffix("a.b.c.bd"), "b.c.bd" );
        // More complex TLD.
        TS_ASSERT_EQUALS( public_suffix("jp"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.jp"), "test.jp" );
        TS_ASSERT_EQUALS( public_suffix("www.test.jp"), "test.jp" );
        TS_ASSERT_EQUALS( public_suffix("ac.jp"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.ac.jp"), "test.ac.jp" );
        TS_ASSERT_EQUALS( public_suffix("www.test.ac.jp"), "test.ac.jp" );
        TS_ASSERT_EQUALS( public_suffix("kyoto.jp"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.kyoto.jp"), "test.kyoto.jp" );
        TS_ASSERT_EQUALS( public_suffix("ide.kyoto.jp"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("b.ide.kyoto.jp"), "b.ide.kyoto.jp" );
        TS_ASSERT_EQUALS( public_suffix("a.b.ide.kyoto.jp"), "b.ide.kyoto.jp" );
		/* rule *.kobe.jp
		 * TODO fix bugs in TLD_WILDCARD, follow PSL logic and algorithm 
        TS_ASSERT_EQUALS( public_suffix("c.kobe.jp"), "c.kobe.jp" );
        TS_ASSERT_EQUALS( public_suffix("b.c.kobe.jp"), "c.kobe.jp" );
        TS_ASSERT_EQUALS( public_suffix("a.b.c.kobe.jp"), "c.kobe.jp" );
		*/
        TS_ASSERT_EQUALS( public_suffix("city.kobe.jp"), "city.kobe.jp" );
        TS_ASSERT_EQUALS( public_suffix("www.city.kobe.jp"), "city.kobe.jp" );
        // TLD with a wildcard rule and exceptions.
        TS_ASSERT_EQUALS( public_suffix("ck"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.ck"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("b.test.ck"), "b.test.ck" );
        TS_ASSERT_EQUALS( public_suffix("a.b.test.ck"), "b.test.ck" );
        TS_ASSERT_EQUALS( public_suffix("www.ck"), "www.ck" );
        TS_ASSERT_EQUALS( public_suffix("www.www.ck"), "www.ck" );
        // US K12.
        TS_ASSERT_EQUALS( public_suffix("us"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.us"), "test.us" );
        TS_ASSERT_EQUALS( public_suffix("www.test.us"), "test.us" );
        TS_ASSERT_EQUALS( public_suffix("ak.us"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.ak.us"), "test.ak.us" );
        TS_ASSERT_EQUALS( public_suffix("www.test.ak.us"), "test.ak.us" );
        TS_ASSERT_EQUALS( public_suffix("k12.ak.us"), "NULL" );
        TS_ASSERT_EQUALS( public_suffix("test.k12.ak.us"), "test.k12.ak.us" );
        TS_ASSERT_EQUALS( public_suffix("www.test.k12.ak.us"), "test.k12.ak.us" );

		// from: https://raw.githubusercontent.com/publicsuffix/list/master/tests/test_psl.txt
		// Any copyright is dedicated to the Public Domain.
		// https://creativecommons.org/publicdomain/zero/1.0/

		// Listed, but non-Internet, TLD.
		//TS_ASSERT_EQUALS( public_suffix("local"), "NULL" );
		//TS_ASSERT_EQUALS( public_suffix("example.local"), "NULL" );
		//TS_ASSERT_EQUALS( public_suffix("b.example.local"), "NULL" );
		//TS_ASSERT_EQUALS( public_suffix("a.b.example.local"), "NULL" );

		// TLD with only 1 (wildcard) rule.
		TS_ASSERT_EQUALS( public_suffix("mm"), "NULL" );
		TS_ASSERT_EQUALS( public_suffix("c.mm"), "NULL" );
		TS_ASSERT_EQUALS( public_suffix("b.c.mm"), "b.c.mm" );
		TS_ASSERT_EQUALS( public_suffix("a.b.c.mm"), "b.c.mm" );
		// More complex TLD.
		/* TODO
		TS_ASSERT_EQUALS( public_suffix("c.kobe.jp"), "NULL" );
		TS_ASSERT_EQUALS( public_suffix("b.c.kobe.jp"), "b.c.kobe.jp" );
		TS_ASSERT_EQUALS( public_suffix("a.b.c.kobe.jp"), "b.c.kobe.jp" );
		*/
		// IDN labels.
		/* TODO UTF-8 domains
		TS_ASSERT_EQUALS( public_suffix("食狮.com.cn"), "食狮.com.cn" );
		TS_ASSERT_EQUALS( public_suffix("食狮.公司.cn"), "食狮.公司.cn" );
		TS_ASSERT_EQUALS( public_suffix("www.食狮.公司.cn"), "食狮.公司.cn" );
		TS_ASSERT_EQUALS( public_suffix("shishi.公司.cn"), "shishi.公司.cn" );
		TS_ASSERT_EQUALS( public_suffix("公司.cn"), "NULL" );
		TS_ASSERT_EQUALS( public_suffix("食狮.中国"), "食狮.中国" );
		TS_ASSERT_EQUALS( public_suffix("www.食狮.中国"), "食狮.中国" );
		TS_ASSERT_EQUALS( public_suffix("shishi.中国"), "shishi.中国" );
		TS_ASSERT_EQUALS( public_suffix("中国"), "NULL" );
		*/
		// Same as above, but punycoded.
		TS_ASSERT_EQUALS( public_suffix("xn--85x722f.com.cn"), "xn--85x722f.com.cn" );
		TS_ASSERT_EQUALS( public_suffix("xn--85x722f.xn--55qx5d.cn"), "xn--85x722f.xn--55qx5d.cn" );
		TS_ASSERT_EQUALS( public_suffix("www.xn--85x722f.xn--55qx5d.cn"), "xn--85x722f.xn--55qx5d.cn" );
		TS_ASSERT_EQUALS( public_suffix("shishi.xn--55qx5d.cn"), "shishi.xn--55qx5d.cn" );
		TS_ASSERT_EQUALS( public_suffix("xn--55qx5d.cn"), "NULL" );
		TS_ASSERT_EQUALS( public_suffix("xn--85x722f.xn--fiqs8s"), "xn--85x722f.xn--fiqs8s" );
		TS_ASSERT_EQUALS( public_suffix("www.xn--85x722f.xn--fiqs8s"), "xn--85x722f.xn--fiqs8s" );
		TS_ASSERT_EQUALS( public_suffix("shishi.xn--fiqs8s"), "shishi.xn--fiqs8s" );
		TS_ASSERT_EQUALS( public_suffix("xn--fiqs8s"), "NULL" );
    }
};
