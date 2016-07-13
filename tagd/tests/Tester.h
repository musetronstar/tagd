// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>
#include "tagd.h"
#include "tagd/utf8.h"

#define TAGD_CODE_STRING(c)	std::string(tagd_code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

    void test_utf8_read(void) {
		std::string str;
		size_t pos = 0;
		uint32_t cp = tagd::utf8_read(str, &pos);
		TS_ASSERT( cp == 0 )
		TS_ASSERT( pos == 0 )

		str = "\x00";
		pos = 0;
		cp = tagd::utf8_read(str, &pos);
		TS_ASSERT( cp == 0 )
		TS_ASSERT( pos == 0 )

		str = "\x01";
		pos = 0;
		cp = tagd::utf8_read(str, &pos);
		TS_ASSERT_EQUALS( cp , 1 )
		TS_ASSERT_EQUALS( pos , 1 )
	}

    void test_utf8_append(void) {
		std::string str = "\x01";
		uint32_t cp = 2;
		tagd::utf8_append(str, cp);
		TS_ASSERT( str == "\x01\x02" )
	}

    void test_utf8_read_append(void) {
		// काचं शक्नोम्यत्तुम् । नोपहिनस्ति माम् ॥
		std::string str = "\xE0\xA4\x95\xE0\xA4\xBE\xE0\xA4\x9A\xE0\xA4\x82\x20\xE0\xA4\xB6\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xA8\xE0\xA5\x8B\xE0\xA4\xAE\xE0\xA5\x8D\xE0\xA4\xAF\xE0\xA4\xA4\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x81\xE0\xA4\xAE\xE0\xA5\x8D\x20\xE0\xA5\xA4\x20\xE0\xA4\xA8\xE0\xA5\x8B\xE0\xA4\xAA\xE0\xA4\xB9\xE0\xA4\xBF\xE0\xA4\xA8\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA4\xBF\x20\xE0\xA4\xAE\xE0\xA4\xBE\xE0\xA4\xAE\xE0\xA5\x8D\x20\xE0\xA5\xA5";
		std::string res;
		size_t pos = 0;
		uint32_t cp;
		do {
			cp = tagd::utf8_read(str, &pos);
			tagd::utf8_append(res, cp);
		} while (cp != 0);

		TS_ASSERT( res == str )
	}

    void test_utf8_pos_back(void) {
		// काचं शक्नोम्यत्तुम् । नोपहिनस्ति माम् ॥
		std::string str = "\xE0\xA4\x95\xE0\xA4\xBE\xE0\xA4\x9A\xE0\xA4\x82\x20\xE0\xA4\xB6\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xA8\xE0\xA5\x8B\xE0\xA4\xAE\xE0\xA5\x8D\xE0\xA4\xAF\xE0\xA4\xA4\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x81\xE0\xA4\xAE\xE0\xA5\x8D\x20\xE0\xA5\xA4\x20\xE0\xA4\xA8\xE0\xA5\x8B\xE0\xA4\xAA\xE0\xA4\xB9\xE0\xA4\xBF\xE0\xA4\xA8\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA4\xBF\x20\xE0\xA4\xAE\xE0\xA4\xBE\xE0\xA4\xAE\xE0\xA5\x8D\x20\xE0\xA5\xA5";
		size_t pos = tagd::utf8_pos_back(str);
		TS_ASSERT( pos != std::string::npos )
		TS_ASSERT_EQUALS( str.substr(pos) , "\xE0\xA5\xA5" )

		pos = tagd::utf8_pos_back("\x81\x82\x83");
		TS_ASSERT ( pos == std::string::npos )

		pos = tagd::utf8_pos_back(str, 1);
		TS_ASSERT ( pos == 0 )
	}

    void test_utf8_increment(void) {
		uint32_t cp = 2405;
		uint32_t inc = tagd::utf8_increment(cp);
		TS_ASSERT_EQUALS( inc , (cp + 1) )

		inc = tagd::utf8_increment(0xD801);  // UTF16 surrogate
		TS_ASSERT_EQUALS( inc , (0xDFFF + 1) )  // advance past UTF16 surrogate
	}

    void test_rank_nil(void) {
        char *nil = NULL; 

        // null pointer
        TS_ASSERT_EQUALS( tagd::rank::dotted_str(nil) , "" );        

        tagd::rank r1;
        r1.init(nil);
        TS_ASSERT( r1.empty() );
    }

    void test_rank_init(void) {
        char a1[4] = {1, 2, 5, '\0'};

        // init tests
        tagd::rank r1;
        auto tc = r1.init(a1);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(tc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
        TS_ASSERT_EQUALS( r1.back() , 5 );
        TS_ASSERT_EQUALS( r1.size() , 3 );
        TS_ASSERT(std::memcmp(r1.c_str(), a1, 4) == 0);

        // copy cons
        tagd::rank r2(r1);
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        // copy assignment
        tagd::rank r3;
        TS_ASSERT( r3.empty() );
        r3 = r1;
        TS_ASSERT_EQUALS( r3.dotted_str() , "1.2.5" );
    }
	
	void test_rank_init_int64(void) {
        uint64_t i = 0x0102050000000000;

        // init tests
        tagd::rank r1;
        auto tc = r1.init(i);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(tc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
        TS_ASSERT_EQUALS( r1.back() , 5 );
        TS_ASSERT_EQUALS( r1.size() , 3 );
    }

    void test_rank_clear(void) {
        char a1[4] = {1, 2, 5, '\0'};

        // init tests
        tagd::rank r1;
        r1.init(a1);
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
		r1.clear();
		TS_ASSERT(r1.empty());
	}

    void test_rank_push_pop_order(void) {
        char a1[4] = {1, 2, 5, '\0'};

        // push,pop
        tagd::rank r1;
        r1.init(a1);
        tagd::code rc = r1.push_back(3); 
        TS_ASSERT_EQUALS( rc , tagd::TAGD_OK );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5.3" );

        uint32_t b = r1.pop_back();
        TS_ASSERT_EQUALS( b , 3 );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        rc = r1.push_back(1000);
        TS_ASSERT_EQUALS( rc , tagd::TAGD_OK );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5.1000" );

        r1.pop_back();    
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        // rank order
        tagd::rank r2;
        a1[2] = 2;
        r2.init(a1);
        TS_ASSERT( r2 < r1 );  // 1.2.2 < 1.2.5

        // push unallocated
        tagd::rank r4;
        rc = r4.push_back(1);
        TS_ASSERT_EQUALS( rc , tagd::TAGD_OK );
        TS_ASSERT_EQUALS( r4.dotted_str() , "1" );
        TS_ASSERT_EQUALS( r4.back() , 1 );
        TS_ASSERT_EQUALS( r4.size() , 1 );

        // pop to empty
        b = r4.pop_back();
        TS_ASSERT_EQUALS( b, 1 );
        b = r4.pop_back();
        TS_ASSERT( r4.empty() );
        TS_ASSERT_EQUALS( b, 0 );
        b = r4.pop_back(); // again
        TS_ASSERT_EQUALS( b, 0 );

        // pop unallocated
        tagd::rank r5;
        b = r4.pop_back();
        TS_ASSERT_EQUALS( b, 0 );
        TS_ASSERT_EQUALS( r4.dotted_str() , "" );
        TS_ASSERT_EQUALS( r4.back() , 0 );
        TS_ASSERT_EQUALS( r4.size() , 0 );
    }

    void test_rank_uno_set(void) {
        char a1[2] = {1, '\0'};

        tagd::rank r1;
        r1.init(a1);
        tagd::rank_set R;
        R.insert(r1);

        tagd::rank next;
        tagd::rank::next (next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "2" );
    }

    void test_rank_set(void) {
		std::string a1 {"\x01\xCD\xB6\x05"};

		// 0xCDB6 == 11001101 10110110
		//    value:    01101   110110 == 886

        tagd::rank r1;
        r1.init(a1.c_str());

        // rank set
        tagd::rank_set R;
        R.insert(r1); // "1.886.5" 

        tagd::rank next;
        tagd::rank::next(next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.886.1" );
        R.insert(next);

        tagd::rank::next(next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.886.2" );
        R.insert(next);

        // set order
        tagd::rank_set::iterator it = R.begin();
        TS_ASSERT_EQUALS( it->dotted_str(), "1.886.1" ); 

        it = R.end(); --it;
        TS_ASSERT_EQUALS( it->dotted_str(), "1.886.5" ); 

        // find
        it = R.find(next);  // 1.2.2
        TS_ASSERT_EQUALS( it->dotted_str() , "1.886.2" );

        a1[3] = 0x04;
        next.init(a1.c_str());        
        it = R.find(next);  // 1.886.4 not there
        TS_ASSERT_EQUALS( it , R.end() );

		R.clear();
		tagd::code rc = next.push_back(tagd::UTF8_MAX_CODE_POINT);
		R.insert(next);
        rc = tagd::rank::next(next, R);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_VALUE");
    }

	void test_rank_set_next(void) {
        char a1[4] = {1, 2, 1, '\0'};
        tagd::rank r1;
        tagd::code rc = r1.init(a1);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");

        tagd::rank_set R;
        R.insert(r1);
		int i = 0;
        while (++i < 127) {
            rc = r1.increment();
            if (rc != tagd::TAGD_OK)
                break;
			R.insert(r1);
		}
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.127" );

        tagd::rank next;
        tagd::rank::next(next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.2.128" );
        R.insert(next);

		next.clear();
        tagd::rank::next(next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.2.129" );
        R.insert(next);
    }

	void test_rank_increment(void) {
        char a1[4] = {1, 2, 125, '\0'};
        tagd::rank r1;
        tagd::code rc = r1.init(a1);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");

		// increment past the single utf8 leading byte 0x80 == 128
		// into the multibyte range
        while (r1.back() < 130) {
            rc = r1.increment();
            if (rc != tagd::TAGD_OK)
                break;
        }

        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.130" );

		// utf8 two byte sequence
		//   110xxxxx 10xxxxxx
		//   11011111 10111101 == 0xDFBD
		// val: 11111   111101 == 2045
		std::string a2 {"\x01\x02\xDF\xBD"};
        rc = r1.init(a2.c_str());
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.2045" );

		// increment from two bytes to three
        while (r1.back() < 2050) {
            rc = r1.increment();
            if (rc != tagd::TAGD_OK)
                break;
        }

        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.2050" );

		// utf8 three byte sequence
		//   1110xxxx 10xxxxxx 10xxxxxx
		//   11101111 10111111 10111101 == 0xEFBFBD
		// val:  1111   111111   111101 == 0xFFFD == 65533 
		rc = r1.push_back(0xFFFD);  // invalid replacement sequence
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_ERR");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.2050" );

		rc = r1.push_back(0xFFFD - 1);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "TAGD_OK");
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.2050.65532" );

		// 11110xxx	10xxxxxx	10xxxxxx	10xxxxxx
    }

    void test_rank_maximums(void) {
        // test RANK_MAX_LEN
        char max[tagd::RANK_MAX_LEN+1];
        std::fill(max, (max+tagd::RANK_MAX_LEN+1), 1);

        tagd::rank r1;
        tagd::code rc = r1.init(max);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_LEN");

        for (size_t i=0; i<tagd::RANK_MAX_LEN+1; ++i) {
            rc = r1.push_back(1);
        }
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_LEN");
    }

    void test_tag_rank(void) {
        // tag rank
        char a1[4] = {1, 2, 123, '\0'};
        tagd::tag t;
        tagd::rank r1;
        r1.init(a1);
        t.rank(r1); // assignment operator
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.123" );

        // rank_code passthrough
        a1[2] = 120;
        tagd::code rc = t.rank(a1);
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.120" );

        // error passthrough
        char max[tagd::RANK_MAX_LEN+1];
        std::fill(max, (max+tagd::RANK_MAX_LEN+1), 1);
        rc = t.rank(max);
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);

        // rank should not change on error
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.120" );
    }

    void test_rank_order(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};

        tagd::rank a;
        a.init(a1); // 1

        a1[1] = 1;
        tagd::rank b;
        b.init(a1); // 1.1
        TS_ASSERT( a < b );

        a1[2] = 1;
        tagd::rank c;
        c.init(a1); // 1.1.1
        TS_ASSERT( b < c );
        TS_ASSERT( a < c );

        a1[2] = 2;
        tagd::rank d;
        d.init(a1); // 1.1.2
        TS_ASSERT( c < d );
        TS_ASSERT( a < d );

        tagd::rank e;
        // no rank
        TS_ASSERT( e < d );
        TS_ASSERT( e < a );

        tagd::rank f;
        // no rank vs no rank
        TS_ASSERT( (!(e < f) && !(f < e)) );
    }

    void test_rank_equal(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};

        tagd::rank a, b;
        TS_ASSERT( a == b); // NULL data

        a.init(a1); // 1
        TS_ASSERT( a != b);

        b.init(a1);
        TS_ASSERT( a == b );

        a1[1] = 1;
        b.init(a1); // 1.1
        TS_ASSERT( a != b );  // size differs
    }

    void test_rank_contains(void) {
        // tag rank
        char a1[2] = {1, '\0'};
        char a2[3] = {1, 2, '\0'};
        char a3[4] = {1, 2, 3, '\0'};
        char a4[5] = {1, 2, 3, 4, '\0'};
        char a5[4] = {1, 4, 3, '\0'};

        tagd::rank a, b, c, d, e;
		a.init(a1);  // 1
		b.init(a2);  // 1.2
		TS_ASSERT ( a.contains(a) )	
		TS_ASSERT ( a.contains(b) )	
		TS_ASSERT ( !b.contains(a) )	

        c.init(a3); // 1.2.3
		TS_ASSERT( c.contains(c) )   // ranks contains themselves
		TS_ASSERT( !c.contains(d) )  // empty data in d

        d.init(a4); // 1.2.3.4
        TS_ASSERT( c.contains(d) )
        TS_ASSERT( !d.contains(c) )

		c.init(a5); // 1.4.3
        TS_ASSERT( !c.contains(e) )
        TS_ASSERT( !d.contains(e) )
        TS_ASSERT( !e.contains(c) )
        TS_ASSERT( !e.contains(d) )
    }

    void test_tag_rank_order(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;

        tagd::tag a("animal");
        r1.init(a1); // 1
        a.rank(r1);

        a1[1] = 1;
        r1.init(a1); // 1.1
        tagd::tag b("mammal");
        b.rank(r1);
        TS_ASSERT( a < b );

        a1[2] = 1;
        r1.init(a1); // 1.1.1
        tagd::tag c("dog");
        c.rank(r1);
        TS_ASSERT( b < c );
        TS_ASSERT( a < c );

        a1[2] = 2;
        r1.init(a1); // 1.1.2
        tagd::tag d("cat");
        d.rank(r1);
        TS_ASSERT( c < d );
        TS_ASSERT( a < d );

        tagd::tag e("caca");

		// assertions fail when comparing tags with rank vs w/o ranks
        // TS_ASSERT( e < d );
        // TS_ASSERT( a < e );

        tagd::tag f("mierda");
        // no rank vs no rank, use id
        TS_ASSERT( ((e < f) && !(f < e)) );
    }

    void test_tag_set(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set S;
		std::pair<tagd::tag_set::iterator, bool> pr;
        tagd::tag a("animal");
        a.rank(r1);
        S.insert(a);

        a1[1] = 1;
        r1.init(a1);
        tagd::tag b("mammal");
        b.rank(r1);
        pr = S.insert(b);
        TS_ASSERT( pr.second == true );
        
        a1[2] = 1;
        r1.init(a1);
        tagd::tag c("dog");
        c.rank(r1);
        pr = S.insert(c);
        TS_ASSERT( pr.second == true );

        a1[2] = 2;
        r1.init(a1);
        tagd::tag d("cat");
        d.rank(r1);
        pr = S.insert(d);
        TS_ASSERT( pr.second == true );

        tagd::tag_set::iterator it = S.begin();
        TS_ASSERT_EQUALS( it->id(), "animal" );

        it++;
        TS_ASSERT_EQUALS( it->id(), "mammal" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "dog" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "cat" );
    }

    void test_tag_set_no_ranks(void) {
        tagd::tag_set S;
		std::pair<tagd::tag_set::iterator, bool> pr;
        tagd::tag a("animal");
        S.insert(a);

        tagd::tag b("mammal");
        pr = S.insert(b);
        TS_ASSERT( pr.second == true );
        
        tagd::tag c("dog");
        pr = S.insert(c);
        TS_ASSERT( pr.second == true );

        tagd::tag d("cat");
        pr = S.insert(d);
        TS_ASSERT( pr.second == true );

        tagd::tag_set::iterator it = S.begin();
        TS_ASSERT_EQUALS( it->id(), "animal" );

        it++;
        TS_ASSERT_EQUALS( it->id(), "cat" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "dog" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "mammal" );
    }

    void test_merge_tags(void) {
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set A, B, C, D;
        tagd::abstract_tag animal("animal");
		animal.rank(r1);
        A.insert(animal);  // no relations
		// A: { animal }

        a1[1] = 1;
        r1.init(a1);
        tagd::abstract_tag dog("dog");
		dog.rank(r1);
        A.insert(dog);  // no relations
        dog.relation("has", "legs", "4");
        B.insert(dog); // one relation
		// A: { animal, dog }
		// B: { dog }

        a1[1] = 2;
        r1.init(a1);
        tagd::abstract_tag cat("cat");
		cat.rank(r1);
        cat.relation("has", "legs", "4");
        A.insert(cat); // one relation
        cat.relation("can", "meow");
        B.insert(cat); // two relations
		// A: { animal, dog, cat }
		// B: { dog, cat }

        a1[1] = 3;
        r1.init(a1);
        tagd::abstract_tag bird("bird");
		bird.rank(r1);
        bird.relation("has", "feathers"); 
        bird.relation("has", "wings"); 
        B.insert(bird);  // not in A
		// A: { animal, dog, cat }
		// B: { dog, cat, bird }

        // C is our control
        C.insert(animal);
        C.insert(dog);
        C.insert(cat);
        C.insert(bird);

        // set up D for merge and diff
        cat.relation("has", "whiskers");
        D.insert(cat);
        D.insert(bird);
		// D: { cat, bird }

        tagd::tag_set T;  // temp to hold old A
        T.insert(A.begin(), A.end());
       
        merge_tags(A, B);  // merge B into A
        TS_ASSERT_EQUALS( A.size(), 4 );
        TS_ASSERT( tag_set_equal(A, C) );
		// A: { animal, dog, cat, bird }
		// B: { dog, cat, bird }

        merge_tags_erase_diffs(A, D);
		
        TS_ASSERT_EQUALS( A.size(), 2 );  // contains cat and bird
        tagd::tag_set::iterator it = A.find(cat); // cat
        TS_ASSERT( it != A.end() && *it == cat );  // cat relations merged
    }

    void test_merge_containing_tags(void) {
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set A, B;
        tagd::abstract_tag animal("animal");
		animal.rank(r1);

        a1[1] = 1;
        r1.init(a1);
        tagd::abstract_tag dog("dog");
		dog.rank(r1);

        a1[1] = 2;
        r1.init(a1);
        tagd::abstract_tag cat("cat");
		cat.rank(r1);

        a1[1] = 3;
        r1.init(a1);
        tagd::abstract_tag bird("bird");
		bird.rank(r1);

        a1[0] = 2;
        r1.init(a1);
        tagd::abstract_tag car("car");
		car.rank(r1);

        A.insert(animal);
        A.insert(dog);
        A.insert(cat);
        A.insert(bird);

		B.insert(cat);
		B.insert(bird);

		// std::cerr << "A: " ; print_tag_ids(A);
		// std::cerr << "B: " ; print_tag_ids(B);
		// std::cerr << "merge_containing_tags:" << std::endl;
        merge_containing_tags(A, B);
		// std::cerr << "A: " ; print_tag_ids(A);
		// std::cerr << "-----\n";
        TS_ASSERT_EQUALS( tag_ids_str(A), "cat, bird" )

		A.clear();
        A.insert(animal);
        A.insert(dog);

		B.clear();
		B.insert(cat);
		B.insert(bird);

        merge_containing_tags(A, B);

		// because cat and bird are animals
		// and dog not in B
        TS_ASSERT_EQUALS( tag_ids_str(A), "cat, bird" )

		A.clear();
		A.insert(cat);
		A.insert(bird);

		B.clear();
        B.insert(animal);

        merge_containing_tags(A, B);

		// because cat and bird are animals
        TS_ASSERT_EQUALS( tag_ids_str(A), "cat, bird" )

		animal.super_object("living_thing");
		animal.relation("has", "homeostasis");

		cat.relation("has", "whiskers");

		bird.relation("has", "feathers");

		A.clear();
        A.insert(animal);
		A.insert(cat);
		A.insert(bird);

		animal.relation("has", "metabolism");

		B.clear();
        B.insert(animal);

		// TODO merge_containing_tags() fails to merge the predicates of contained tags
		// for example, the animal predicate "has metabolism" will not be in the resulting tag_set
		// std::cerr << "A:" << std::endl;
		// tagd::print_tags(A, std::cerr);
		// std::cerr << "B:" << std::endl;
		// tagd::print_tags(B, std::cerr);

        merge_containing_tags(A, B);

		// std::cerr << "A:" << std::endl;
		// tagd::print_tags(A, std::cerr);

		// because cat and bird are animals
        TS_ASSERT_EQUALS( tag_ids_str(A), "animal, cat, bird" )
    }

    void test_merge_erase_diffs(void) {
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set A, B;

        tagd::abstract_tag dog("dog");
		dog.rank(r1);
        A.insert(dog);  // no relations
        dog.relation("has", "legs", "4");
        B.insert(dog); // one relation
		// A: { dog }
		// B: { dog }

        a1[1] = 1;
        r1.init(a1);
        tagd::abstract_tag cat("cat");
		cat.rank(r1);
        cat.relation("has", "legs", "4");
        cat.relation("can", "meow");
        B.insert(cat); // two relations
		// A: { dog }
		// B: { dog, cat }

        merge_tags_erase_diffs(A, B);
        TS_ASSERT_EQUALS( A.size(), 1 );  // contains dog
        tagd::tag_set::iterator it = A.find(dog);
        TS_ASSERT( it != A.end() && *it == dog );
    }

	void test_tag(void) {
		tagd::tag dog("dog", "animal");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super_relator() == HARD_TAG_IS_A )
		TS_ASSERT( dog.super_object() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
	}
	
	void test_super_relator(void) {
		tagd::tag dog("perro", "es_un", "animal");
		TS_ASSERT( dog.id() == "perro" )
		TS_ASSERT( dog.super_relator() == "es_un" )
		TS_ASSERT( dog.super_object() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
	}

	void test_tag_clear(void) {
		tagd::tag dog("dog", "animal");
		dog.relation("has", "teeth");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super_object() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
		TS_ASSERT( dog.related("has", "teeth")  )
		dog.clear();
		TS_ASSERT( dog.id().empty() )
		TS_ASSERT( dog.super_object().empty() ) 
		// POS doesn't get set to UNKNOWN
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
		TS_ASSERT( !dog.related("has", "teeth")  )
		TS_ASSERT( dog.empty() )
	}

	void test_tag_eq(void) {
		tagd::tag a("dog", "is_a", "animal");
		tagd::tag b("dog", "is_a", "mammal");

		TS_ASSERT( a.id() == b.id() )
		TS_ASSERT( a != b )

        a.super_object("mammal");
		TS_ASSERT( a == b );

		a.super_relator("es_un");
		TS_ASSERT( a != b );
		a.super_relator("is_a");

        b.relation("has", "legs");
		TS_ASSERT( a != b );

        a.relation("has", "legs");
		TS_ASSERT( a == b );
	}

    void test_relation(void) {
		tagd::tag dog("dog", "animal");
        dog.relation("has","teeth");
        dog.relation("has","legs","4");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super_object() == "animal" )
	}

    void test_related(void) {
		tagd::tag dog("dog", "animal");
        TS_ASSERT_EQUALS( dog.relation("has","teeth"), tagd::TAGD_OK )
        TS_ASSERT_EQUALS( dog.relation("has","legs", "4"), tagd::TAGD_OK )
		TS_ASSERT( dog.related("has", "teeth")  )
		TS_ASSERT( dog.related("legs") )
		TS_ASSERT( dog.related("has", "legs", "4") )
		TS_ASSERT( !dog.related("has", "legs", "5") )
		TS_ASSERT( dog.related(tagd::make_predicate("has", "legs", "4")) )
		TS_ASSERT( !dog.related(tagd::make_predicate("has", "legs", "5")) )

        tagd::predicate_set how;
		TS_ASSERT_EQUALS( dog.related("legs", how), 1 )
		auto it = how.begin();
		TS_ASSERT( it != how.end() );
		if (it != how.end()) {
			TS_ASSERT_EQUALS( it->relator, "has" )
			TS_ASSERT_EQUALS( it->object, "legs" )
			TS_ASSERT_EQUALS( it->modifier, "4" )
		}

		TS_ASSERT( !dog.related("has", "fins") )
	}

    void test_insert_relation(void) {
        tagd::tag fish("fish", "animal");
        fish.relation(tagd::make_predicate("has", "fins"));
		TS_ASSERT( fish.related("has", "fins") )
    }

    void test_insert_predicate_set(void) {
        tagd::tag fish("fish");
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        pr = P.insert(tagd::make_predicate("has", "fins"));
        pr = P.insert(tagd::make_predicate("breaths", "water"));
        fish.predicates(P);

		TS_ASSERT( fish.related("has", "fins") )
		TS_ASSERT( fish.related("breaths", "water") )
    }

	void test_insert_relation_relator(void) {
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        tagd::interrogator q(HARD_TAG_INTERROGATOR);
		tagd::code tc = q.relation(tagd::make_predicate("has",""));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        tc = q.relation(tagd::make_predicate("breaths",""));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( q.relations.size() , 2 );

		TS_ASSERT( q.has_relator("has") )
		TS_ASSERT( q.has_relator("breaths") )
    }

	void test_insert_predicate_set_relator(void) {
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        pr = P.insert(tagd::make_predicate("has",""));
		TS_ASSERT( pr.second )
        pr = P.insert(tagd::make_predicate("breaths",""));
		TS_ASSERT( pr.second )
		TS_ASSERT_EQUALS( P.size() , 2 )

        tagd::interrogator q(HARD_TAG_INTERROGATOR);
        q.predicates(P);
		TS_ASSERT_EQUALS( q.relations.size() , 2 );

		TS_ASSERT( q.has_relator("has") )
		TS_ASSERT( q.has_relator("breaths") )
    }

    void test_tag_copy(void) {
        tagd::tag fish("fish", "is_a", "animal");
        fish.relation(tagd::make_predicate("has", "fins"));

        tagd::abstract_tag a(fish);
		TS_ASSERT_EQUALS( a.id(), "fish" );
		TS_ASSERT_EQUALS( a.super_relator(), "is_a" );
		TS_ASSERT_EQUALS( a.super_object(), "animal" );
		TS_ASSERT( a.related("has", "fins") )

        tagd::abstract_tag b;
        b = fish;
		TS_ASSERT_EQUALS( b.id(), "fish" );
		TS_ASSERT_EQUALS( b.super_relator(), "is_a" );
		TS_ASSERT_EQUALS( b.super_object(), "animal" );
		TS_ASSERT( b.related("has", "fins") )
    }

    void test_relator(void) {
		tagd::relator r1("has");
		TS_ASSERT( r1.id() == "has" )
		TS_ASSERT( r1.id() == "has" )
		TS_ASSERT( r1.super_relator() == HARD_TAG_TYPE_OF )
		TS_ASSERT( r1.super_object() == "_relator" )
		TS_ASSERT( r1.pos() == tagd::POS_RELATOR )

		tagd::relator r2("can", "verb");
		TS_ASSERT( r2.id() == "can" )
		TS_ASSERT( r2.super_object() == "verb" )
		TS_ASSERT( r2.pos() == tagd::POS_RELATOR )
	}

	// test url relations like other tags
	// parsing tests go in UrlTester
    void test_url(void) {
		tagd::url u("http://hypermega.com");
        u.relation("has","links", "4");
        u.relation("about", "computer_security");
		TS_ASSERT_EQUALS( u.hduri() , "com:hypermega:http" )
		TS_ASSERT_EQUALS( u.super_object() , "_url" )
		TS_ASSERT( u.pos() == tagd::POS_URL )
		TS_ASSERT( u.related("has", "links") )
		TS_ASSERT( u.related("about", "computer_security") )
	}

    void test_interrogator(void) {
		tagd::interrogator a("what");
		TS_ASSERT_EQUALS( a.id() , "what" )
		TS_ASSERT( a.super_object().empty() )
		TS_ASSERT( a.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator b("what", "mammal");
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT( b.super_relator() == HARD_TAG_TYPE_OF )
		TS_ASSERT( b.super_object() == "mammal" )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator c("what");
		c.relation("has", "legs", "2", tagd::OP_GT);
		c.relation("has", "legs", "8", tagd::OP_LT);
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT_EQUALS( c.relations.size() , 2 )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )

	}

	void test_referent(void) {
		tagd::referent a("is_a", "_is_a");
		TS_ASSERT_EQUALS( a.refers() , "is_a" )
		TS_ASSERT_EQUALS( a.super_relator() , HARD_TAG_REFERS_TO )
		TS_ASSERT_EQUALS( a.refers_to() , "_is_a" )
		TS_ASSERT( a.pos() == tagd::POS_REFERENT )
		TS_ASSERT( a.context().empty() )

		tagd::referent b("perro", "dog", "spanish");
		TS_ASSERT_EQUALS( b.refers() , "perro" )
		TS_ASSERT_EQUALS( a.super_relator() , HARD_TAG_REFERS_TO )
		TS_ASSERT_EQUALS( b.refers_to() , "dog" )
		TS_ASSERT( b.pos() == tagd::POS_REFERENT )
		TS_ASSERT_EQUALS( b.context() , "spanish" )
	}

	void test_referent_set(void) {
		tagd::referent a("thing", HARD_TAG_ENTITY);
		tagd::referent b("thing", "monster", "movies");
		tagd::referent c("thing", "physical_object", "substance");

		tagd::tag_set R;
		R.insert(a);
		R.insert(b);
		R.insert(c);

		TS_ASSERT_EQUALS( R.size() , 3 ) 
	}

    void test_error(void) {
        tagd::error err(tagd::TAGD_ERR);
		TS_ASSERT_EQUALS( err.id(), "TAGD_ERR" ) 
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR ) 
    }

    void test_error_msg(void) {
        tagd::error err(tagd::TAGD_ERR, "bad tag: oops");
		TS_ASSERT_EQUALS( err.id(), "TAGD_ERR" ) 
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR ) 
		TS_ASSERT_EQUALS( err.message(), "bad tag: oops" ) 
    }

    void test_errorable(void) {
		tagd::errorable R;
		TS_ASSERT( R.ok() )
		TS_ASSERT( !R.has_error() )
		TS_ASSERT( R.last_error().empty() )

		TS_ASSERT_EQUALS( R.code(tagd::TS_NOT_FOUND) , tagd::TS_NOT_FOUND )
		TS_ASSERT_EQUALS( R.code() , tagd::TS_NOT_FOUND )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_error() )  // TS_NOT_FOUND considered an error

		TS_ASSERT_EQUALS( R.ferror(tagd::TAGD_ERR, "bad tag: %s", "oops") , tagd::TAGD_ERR );
		TS_ASSERT_EQUALS( R.size() , 1 );
		TS_ASSERT_EQUALS( R.code() , tagd::TAGD_ERR )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_error() )
		TS_ASSERT_EQUALS( R.last_error().id(), "TAGD_ERR" )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR )
		TS_ASSERT_EQUALS( R.last_error().message(), std::string("bad tag: oops") )

        tagd::error err(tagd::TS_MISUSE);
		err.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah");
		R.error(err);
		TS_ASSERT_EQUALS( R.size() , 2 );
		TS_ASSERT_EQUALS( R.code() , tagd::TS_MISUSE )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_error() )
		TS_ASSERT_EQUALS( R.last_error().id(), "TS_MISUSE" )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR )
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah") )

		R.error( tagd::TAGD_ERR,
			tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, "imsobad") );
		TS_ASSERT_EQUALS( R.size() , 3 );
		TS_ASSERT_EQUALS( R.code() , tagd::TAGD_ERR )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_error() )
		TS_ASSERT_EQUALS( R.last_error().id() , "TAGD_ERR" )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR )
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, "imsobad") )

		R.last_error_relation(tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, "23"));
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, "23") )

		TS_ASSERT_EQUALS( R.most_severe() , tagd::TS_MISUSE )
		TS_ASSERT_EQUALS( R.most_severe(tagd::TAGL_ERR) , tagd::TAGL_ERR )

		tagd::errorable R1;
		R1.copy_errors(R);

		// test iterating errors_t
		size_t num_rel = 0;
		const tagd::errors_t E	= R1.errors();
		for (auto e : E) {
			for (auto p : e.relations) {
				num_rel++;
			}
		}
		TS_ASSERT_EQUALS( num_rel, 4 )

		R1.clear_errors();
		TS_ASSERT_EQUALS( R1.size() , 0 );

		tagd::errorable R2;
		R1.share_errors(R2);

		TS_ASSERT_EQUALS( R1.size() , 0 );
		TS_ASSERT_EQUALS( R2.size() , 0 );

		TS_ASSERT_EQUALS( R1.ferror(tagd::TAG_ILLEGAL, "bad tag: %s", "oops") , tagd::TAG_ILLEGAL );
		TS_ASSERT_EQUALS( R1.last_error().id(), "TAG_ILLEGAL" )
		TS_ASSERT_EQUALS( R1.size() , 1 );
		TS_ASSERT_EQUALS( R2.size() , 1 );

		// should have no effect
		R2.share_errors(R1);
		R1.share_errors(R2);
		TS_ASSERT_EQUALS( R1.size() , 1 );
		TS_ASSERT_EQUALS( R2.size() , 1 );

		R1.clear_errors();
		TS_ASSERT_EQUALS( R1.size() , 0 );
		TS_ASSERT_EQUALS( R2.size() , 0 );

		tagd::errorable R3, R4;
		TS_ASSERT_EQUALS( R3.ferror(tagd::TAG_UNKNOWN, "bad tag: %s", "oops") , tagd::TAG_UNKNOWN );
		TS_ASSERT_EQUALS( R3.last_error().id(), "TAG_UNKNOWN" )
		TS_ASSERT_EQUALS( R3.size() , 1 );
		TS_ASSERT_EQUALS( R4.size() , 0 );

		R.share_errors(R3).share_errors(R4);

		TS_ASSERT_EQUALS( R.size() , 4 );
		TS_ASSERT_EQUALS( R3.size() , 4 );
		TS_ASSERT_EQUALS( R4.size() , 4 );

		TS_ASSERT_EQUALS( R.ferror(tagd::TAGD_ERR, "another bad tag: %s", "oops_again") , tagd::TAGD_ERR );

		TS_ASSERT_EQUALS( R.size() , 5 );
		TS_ASSERT_EQUALS( R.last_error().id(), "TAGD_ERR" )
		TS_ASSERT_EQUALS( R3.size() , 5 );
		TS_ASSERT_EQUALS( R3.last_error().id(), "TAGD_ERR" )
		TS_ASSERT_EQUALS( R4.size() , 5 );
		TS_ASSERT_EQUALS( R4.last_error().id(), "TAGD_ERR" )
    }

    void test_modifier_comma_quotes(void) {
		tagd::tag t("table");
		t.relation("has", "idlist", "47,72,43");
		std::stringstream ss;
		ss << t;

		TS_ASSERT_EQUALS( ss.str() , "table has idlist = \"47,72,43\"");
	}

	void test_modifier_esc_quotes(void) {
		tagd::tag t("my_message");
		t.relation("has", "message", "quoted \"string\" hey\" yo \\ ");
		std::stringstream ss;
		ss << t;

		TS_ASSERT_EQUALS( ss.str() , "my_message has message = \"quoted \\\"string\\\" hey\\\" yo \\ \"");
	}

	void test_predicate_ostream_op(void) {
		std::stringstream ss;
		tagd::predicate p{ "has" , "legs", "4" };
		ss << p;
		TS_ASSERT_EQUALS( ss.str() , "has legs = 4" );
	}
};
