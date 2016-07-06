// Test suites

#include <cxxtest/TestSuite.h>

#include "tagd.h"

// takes in a domain and returns the the portion of the
// domain where setting cookies would be allowed
// (according to the public suffix list)
// returns NULL if setting cookies would not be allowed
const char* public_suffix(const char *dom) {
    if (dom == NULL) return NULL;
    if (dom[0] == '.') return NULL;

    tagd::domain d(dom);

    // std::cout << "tld_code: " << tld_code_str(d.code()) << std::endl;

/*
    if (d.is_registrable())
        return d.reg().c_str();
    else
        return NULL;
*/
    switch (d.code()) {
        case TLD_UNKNOWN: {
            /* return label2.label1 or NULL if label2 not exists */
            size_t i = strlen(dom);
            bool got_dot = false;
            while (i > 0) {
                if (dom[i] == '.') {
                    if (got_dot) {
                        i++;
                        return &dom[i];
                    }
                    got_dot = true;
                }
                i--;
            }
            return (got_dot ? &dom[i] : NULL);
        } 
        case TLD_ICANN_REG:
        case TLD_WILDCARD_REG:
        case TLD_EXCEPTION_REG:
        case TLD_PRIVATE_REG:
            return d.reg().c_str();
        case TLD_ICANN:
        case TLD_WILDCARD:
        case TLD_PRIVATE:
        case TLD_ERR:
        default:
            return NULL;
    }
}

// returns whether A and B match according
// to the return value of public_suffix()
bool checkPublicSuffix(const char *A, const char *B) {

    // std::cout << "====================" << std::endl;
    // std::cout << "checkPublicSuffix( " << (A == NULL ? "NULL" : A)
    //           << " , " << (B == NULL ? "NULL" : B) << " )" << std::endl;

    const char *res = public_suffix(A);

    // std::cout << "suffix: " << (A == NULL ? "NULL" : A)
    //           << " => " << (res == NULL ? "NULL" : res) << std::endl;

    if (res == NULL)
        return (B == NULL);

    if (B == NULL)
        return (res == NULL);

    return ( strcmp(res, B) == 0 );
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
        TS_ASSERT( d.is_registrable() );
    }

    void test_domain_init(void) {
        tagd::domain d;
        TS_ASSERT( d.empty() );

        d.init("www.example.com");
        TS_ASSERT( d.is_registrable() );
        TS_ASSERT_EQUALS( tld_code_str(d.code()), "TLD_ICANN_REG" );
        TS_ASSERT_EQUALS( d.pub(), "com" );
        TS_ASSERT_EQUALS( d.reg(), "example.com" );
        TS_ASSERT_EQUALS( d.priv(), "www.example" );
        TS_ASSERT_EQUALS( d.priv_label(), "example" );
        TS_ASSERT_EQUALS( d.str(), "www.example.com" );
        TS_ASSERT( d.is_registrable() );
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
        TS_ASSERT( checkPublicSuffix(NULL, NULL) );
        // Mixed case.
        TS_ASSERT( checkPublicSuffix("COM", NULL) );
        TS_ASSERT( checkPublicSuffix("example.COM", "example.com") );
        TS_ASSERT( checkPublicSuffix("WwW.example.COM", "example.com") );
        // Leading dot.
        TS_ASSERT( checkPublicSuffix(".com", NULL) );
        TS_ASSERT( checkPublicSuffix(".example", NULL) );
        TS_ASSERT( checkPublicSuffix(".example.com", NULL) );
        TS_ASSERT( checkPublicSuffix(".example.example", NULL) );
        // Unlisted TLD.
        TS_ASSERT( checkPublicSuffix("example", NULL) );
        TS_ASSERT( checkPublicSuffix("example.example", "example.example") );
        TS_ASSERT( checkPublicSuffix("b.example.example", "example.example") );
        TS_ASSERT( checkPublicSuffix("a.b.example.example", "example.example") );
        // Listed, but non-Internet, TLD.
        //TS_ASSERT( checkPublicSuffix("local", NULL) );
        //TS_ASSERT( checkPublicSuffix("example.local", NULL) );
        //TS_ASSERT( checkPublicSuffix("b.example.local", NULL) );
        //TS_ASSERT( checkPublicSuffix("a.b.example.local", NULL) );
        // TLD with only 1 rule.
        TS_ASSERT( checkPublicSuffix("biz", NULL) );
        TS_ASSERT( checkPublicSuffix("domain.biz", "domain.biz") );
        TS_ASSERT( checkPublicSuffix("b.domain.biz", "domain.biz") );
        TS_ASSERT( checkPublicSuffix("a.b.domain.biz", "domain.biz") );
        // TLD with some 2-level rules.
        TS_ASSERT( checkPublicSuffix("com", NULL) );
        TS_ASSERT( checkPublicSuffix("example.com", "example.com") );
        TS_ASSERT( checkPublicSuffix("b.example.com", "example.com") );
        TS_ASSERT( checkPublicSuffix("a.b.example.com", "example.com") );
        TS_ASSERT( checkPublicSuffix("uk.com", NULL) );
        TS_ASSERT( checkPublicSuffix("example.uk.com", "example.uk.com") );
        TS_ASSERT( checkPublicSuffix("b.example.uk.com", "example.uk.com") );
        TS_ASSERT( checkPublicSuffix("a.b.example.uk.com", "example.uk.com") );
        TS_ASSERT( checkPublicSuffix("test.ac", "test.ac") );
        // TLD with only 1 (wildcard) rule.
        TS_ASSERT( checkPublicSuffix("bd", NULL) );
        TS_ASSERT( checkPublicSuffix("c.bd", NULL) );
        TS_ASSERT( checkPublicSuffix("b.c.bd", "b.c.bd") );
        TS_ASSERT( checkPublicSuffix("a.b.c.bd", "b.c.bd") );
        // More complex TLD.
        TS_ASSERT( checkPublicSuffix("jp", NULL) );
        TS_ASSERT( checkPublicSuffix("test.jp", "test.jp") );
        TS_ASSERT( checkPublicSuffix("www.test.jp", "test.jp") );
        TS_ASSERT( checkPublicSuffix("ac.jp", NULL) );
        TS_ASSERT( checkPublicSuffix("test.ac.jp", "test.ac.jp") );
        TS_ASSERT( checkPublicSuffix("www.test.ac.jp", "test.ac.jp") );
        TS_ASSERT( checkPublicSuffix("kyoto.jp", NULL) );
        TS_ASSERT( checkPublicSuffix("test.kyoto.jp", "test.kyoto.jp") );
        TS_ASSERT( checkPublicSuffix("ide.kyoto.jp", NULL) );
        TS_ASSERT( checkPublicSuffix("b.ide.kyoto.jp", "b.ide.kyoto.jp") );
        TS_ASSERT( checkPublicSuffix("a.b.ide.kyoto.jp", "b.ide.kyoto.jp") );
        /* WTF inconsistent with PSL
        TS_ASSERT( checkPublicSuffix("c.kobe.jp", NULL) );
        TS_ASSERT( checkPublicSuffix("b.c.kobe.jp", "b.c.kobe.jp") );
        TS_ASSERT( checkPublicSuffix("a.b.c.kobe.jp", "b.c.kobe.jp") );
        */
        TS_ASSERT( checkPublicSuffix("c.kobe.jp", "c.kobe.jp") );
        TS_ASSERT( checkPublicSuffix("b.c.kobe.jp", "c.kobe.jp") );
        TS_ASSERT( checkPublicSuffix("a.b.c.kobe.jp", "c.kobe.jp") );

        TS_ASSERT( checkPublicSuffix("city.kobe.jp", "city.kobe.jp") );
        TS_ASSERT( checkPublicSuffix("www.city.kobe.jp", "city.kobe.jp") );
        // TLD with a wildcard rule and exceptions.
        TS_ASSERT( checkPublicSuffix("ck", NULL) );
        TS_ASSERT( checkPublicSuffix("test.ck", NULL) );
        TS_ASSERT( checkPublicSuffix("b.test.ck", "b.test.ck") );
        TS_ASSERT( checkPublicSuffix("a.b.test.ck", "b.test.ck") );
        TS_ASSERT( checkPublicSuffix("www.ck", "www.ck") );
        TS_ASSERT( checkPublicSuffix("www.www.ck", "www.ck") );
        // US K12.
        TS_ASSERT( checkPublicSuffix("us", NULL) );
        TS_ASSERT( checkPublicSuffix("test.us", "test.us") );
        TS_ASSERT( checkPublicSuffix("www.test.us", "test.us") );
        TS_ASSERT( checkPublicSuffix("ak.us", NULL) );
        TS_ASSERT( checkPublicSuffix("test.ak.us", "test.ak.us") );
        TS_ASSERT( checkPublicSuffix("www.test.ak.us", "test.ak.us") );
        TS_ASSERT( checkPublicSuffix("k12.ak.us", NULL) );
        TS_ASSERT( checkPublicSuffix("test.k12.ak.us", "test.k12.ak.us") );
        TS_ASSERT( checkPublicSuffix("www.test.k12.ak.us", "test.k12.ak.us") );

    }
};
