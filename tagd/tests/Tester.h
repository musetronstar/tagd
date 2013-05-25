// Test suites

#include <cxxtest/TestSuite.h>

#include "tagd.h"

class Tester : public CxxTest::TestSuite {
	public:

    void test_rank_nil(void) {
        tagd::byte_t *nil = NULL; 

        // null pointer
        TS_ASSERT_EQUALS( tagd::rank::dotted_str(nil) , "" );        

        tagd::rank r1;
        r1.init(nil);
        TS_ASSERT( r1.empty() );
    }

    void test_rank_init(void) {
        tagd::byte_t a1[4] = {1, 2, 5, '\0'};

        // init tests
        tagd::rank r1;
        r1.init(a1);
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
        TS_ASSERT_EQUALS( r1.last() , 5 );
        TS_ASSERT_EQUALS( r1.size() , 3 );
        TS_ASSERT(std::memcmp(r1.c_str(), a1, 4) == 0);
        TS_ASSERT(std::memcmp(r1.uc_str(), a1, 4) == 0);

        // copy cons
        tagd::rank r2(r1);
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        // copy assignment
        tagd::rank r3;
        TS_ASSERT( r3.empty() );
        r3 = r1;
        TS_ASSERT_EQUALS( r3.dotted_str() , "1.2.5" );
    }

    void test_rank_clear(void) {
        tagd::byte_t a1[4] = {1, 2, 5, '\0'};

        // init tests
        tagd::rank r1;
        r1.init(a1);
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
		r1.clear();
		TS_ASSERT(r1.empty());
	}

    void test_rank_push_pop_order(void) {
        tagd::byte_t a1[4] = {1, 2, 5, '\0'};

        // push,pop
        tagd::rank r1;
        r1.init(a1);
        tagd::rank_code rc = r1.push(3); 
        TS_ASSERT_EQUALS( rc , tagd::RANK_OK );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5.3" );

        tagd::byte_t b = r1.pop();
        TS_ASSERT_EQUALS( b , 3 );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        rc = r1.push();
        TS_ASSERT_EQUALS( rc , tagd::RANK_OK );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5.1" );

        r1.pop();    
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        // rank order
        tagd::rank r2;
        a1[2] = 2;
        r2.init(a1);
        TS_ASSERT( r2 < r1 );  // 1.2.2 < 1.2.5

        // push unallocated
        tagd::rank r4;
        rc = r4.push();
        TS_ASSERT_EQUALS( rc , tagd::RANK_OK );
        TS_ASSERT_EQUALS( r4.dotted_str() , "1" );
        TS_ASSERT_EQUALS( r4.last() , 1 );
        TS_ASSERT_EQUALS( r4.size() , 1 );

        // pop to empty
        b = r4.pop();
        TS_ASSERT_EQUALS( b, 1 );
        b = r4.pop();
        TS_ASSERT( r4.empty() );
        TS_ASSERT_EQUALS( b, '\0' );
        b = r4.pop(); // again
        TS_ASSERT_EQUALS( b, '\0' );

        // pop unallocated
        tagd::rank r5;
        b = r4.pop();
        TS_ASSERT_EQUALS( b, '\0' );
        TS_ASSERT_EQUALS( r4.dotted_str() , "" );
        TS_ASSERT_EQUALS( r4.last() , '\0' );
        TS_ASSERT_EQUALS( r4.size() , 0 );
    }

    void test_rank_uno_set(void) {
        tagd::byte_t a1[2] = {1, '\0'};

        tagd::rank r1;
        r1.init(a1);
        tagd::rank_set R;
        R.insert(r1);

        tagd::rank next;
        tagd::rank::next (next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "2" );
    }

    void test_rank_set(void) {
        tagd::byte_t a1[4] = {1, 2, 5, '\0'};

        tagd::rank r1;
        r1.init(a1);

        // rank set
        tagd::rank_set R;
        R.insert(r1); // "1.2.5" 

        tagd::rank next;
        tagd::rank::next (next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.2.1" );
        R.insert(next);

        tagd::rank::next (next, R);
        TS_ASSERT_EQUALS( next.dotted_str() , "1.2.2" );
        R.insert(next);

        // set order
        tagd::rank_set::iterator it = R.begin();
        TS_ASSERT_EQUALS( it->dotted_str(), "1.2.1" ); 

        it = R.end(); --it;
        TS_ASSERT_EQUALS( it->dotted_str(), "1.2.5" ); 

        // find
        it = R.find(next);  // 1.2.2
        TS_ASSERT_EQUALS( it->dotted_str() , "1.2.2" );

        a1[2] = 4;
        next.init(a1);        
        it = R.find(next);  // 1.2.4 not there
        TS_ASSERT_EQUALS( it , R.end() );

        // test maximum byte value 
        tagd::rank_code rc;
        a1[2] = 1;
        while (a1[2]++ < 255) {
            rc = r1.init(a1);
            if (rc == tagd::RANK_OK)
                R.insert(r1);
        }

        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_BYTE);
        TS_ASSERT_EQUALS( R.size() , 254 );
    }

    void test_rank_maximums(void) {
        // test RANK_MAX_LEN
        tagd::byte_t max[tagd::RANK_MAX_LEN+1];
        std::fill(max, (max+tagd::RANK_MAX_LEN+1), 1);

        tagd::rank r1;
        tagd::rank_code rc = r1.init(max);
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);

        for (size_t i=0; i<tagd::RANK_MAX_LEN+1; ++i) {
            rc = r1.push();
        }
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);
    }

    void test_tag_rank(void) {
        // tag rank
        tagd::byte_t a1[4] = {1, 2, 123, '\0'};
        tagd::tag t;
        tagd::rank r1;
        r1.init(a1);
        t.rank(r1); // assignment operator
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.123" );

        // rank_code passthrough
        a1[2] = 120;
        tagd::rank_code rc = t.rank(a1);
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.120" );

        // error passthrough
        tagd::byte_t max[tagd::RANK_MAX_LEN+1];
        std::fill(max, (max+tagd::RANK_MAX_LEN+1), 1);
        rc = t.rank(max);
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);

        // rank should not change on error
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.120" );
    }

    void test_rank_order(void) {
        // tag rank
        tagd::byte_t a1[4] = {1, '\0', '\0', '\0'};

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
        tagd::byte_t a1[4] = {1, '\0', '\0', '\0'};

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

    void test_tag_rank_order(void) {
        // tag rank
        tagd::byte_t a1[4] = {1, '\0', '\0', '\0'};
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
        // no rank
        TS_ASSERT( e < d );
        TS_ASSERT( e < a );

        tagd::tag f("mierda");
        // no rank vs no rank
        TS_ASSERT( (!(e < f) && !(f < e)) );
    }

    void test_tag_set(void) {
        // tag rank
        tagd::byte_t a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set S;
        tagd::tag_set_pair pr;
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

        tagd::tag e("caca");
        pr = S.insert(e); // no rank, occupies S[0]
        TS_ASSERT( pr.second == true );

        tagd::tag f("mierda");
        pr = S.insert(f); // no rank, not inserted, seen as dup of S[0]
        TS_ASSERT( pr.second == false );

        tagd::tag_set::iterator it = S.begin();
        TS_ASSERT_EQUALS( it->id(), "caca" );

        it++;
        TS_ASSERT_EQUALS( it->id(), "animal" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "mammal" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "dog" );
        it++;
        TS_ASSERT_EQUALS( it->id(), "cat" );
    }

    void test_merge_tags(void) {
        tagd::byte_t a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        r1.init(a1);

        tagd::tag_set A, B, C, D;
        tagd::abstract_tag t1("animal", r1);
        A.insert(t1);  // no relations

        a1[1] = 1;
        r1.init(a1);
        tagd::abstract_tag t2("dog", r1);
        A.insert(t2);  // no relations
        t2.relation("has", "legs", "4");
        B.insert(t2); // one relation

        a1[1] = 2;
        r1.init(a1);
        tagd::abstract_tag t3("cat", r1);
        t3.relation("has", "legs", "4");
        A.insert(t3); // one relation
        t3.relation("can", "meow");
        B.insert(t3); // two relations

        a1[1] = 3;
        r1.init(a1);
        tagd::abstract_tag t4("bird", r1);
        t4.relation("has", "feathers"); 
        t4.relation("has", "wings"); 
        B.insert(t4);  // not in A

        // C is our control
        C.insert(t1);
        C.insert(t2);
        C.insert(t3);
        C.insert(t4);

        // set up D for merge and diff
        t3.relation("has", "whiskers");
        D.insert(t3);
        D.insert(t4);

        tagd::tag_set T;  // temp to hold old A
        T.insert(A.begin(), A.end());
       
        merge_tags(A, B);  // merge B into A
        TS_ASSERT_EQUALS( A.size(), 4 );
        TS_ASSERT( tag_set_equal(A, C) );

        merge_tags_erase_diffs(A, D);
        TS_ASSERT_EQUALS( A.size(), 2 );  // contains cat and bird
        tagd::tag_set::iterator it = A.find(t3); // cat
        TS_ASSERT( it != A.end() && *it == t3 );  // cat relations merged
    }

//    void test_abstract_tag(void) {
//		tagd::abstract_tag e("_entity", "_entity");
//		TS_ASSERT_EQUALS( e.id() , "_entity" )
//		TS_ASSERT( e.super().empty() )  // override, _entity super is always empty
//		TS_ASSERT_EQUALS( e.pos() , tagd::POS_NOUN )
//	}

	void test_tag(void) {
		tagd::tag dog("dog", "animal");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.is_a() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_NOUN )
	}

	void test_tag_clear(void) {
		tagd::tag dog("dog", "animal");
		dog.relation("has", "teeth");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.is_a() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_NOUN )
		TS_ASSERT( dog.related("has", "teeth")  )
		dog.clear();
		TS_ASSERT( dog.id().empty() )
		TS_ASSERT( dog.is_a().empty() ) 
		// POS doesn't get set to UNKNOWN
		TS_ASSERT( dog.pos() == tagd::POS_NOUN )
		TS_ASSERT( !dog.related("has", "teeth")  )
		TS_ASSERT( dog.empty() )
	}

	void test_tag_eq(void) {
		tagd::tag a("dog", "animal");
		tagd::tag b("dog", "mammal");

		TS_ASSERT( a.id() == b.id() )
		TS_ASSERT( a != b )

        a.is_a("mammal");
		TS_ASSERT( a == b );

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
		TS_ASSERT( dog.is_a() == "animal" )
	}

    void test_relator_is_a(void) {
		tagd::tag dog("dog");
        dog.relation("is_a","animal");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.is_a() == "animal" )
	}

    void test_related(void) {
		tagd::tag dog("dog", "animal");
        TS_ASSERT_EQUALS( dog.relation("has","teeth"), tagd::TAG_OK )
        TS_ASSERT_EQUALS( dog.relation("has","legs", "4"), tagd::TAG_OK )
		TS_ASSERT( dog.related("has", "teeth")  )

        tagd::id_type how;
		TS_ASSERT( dog.related("legs", &how) )
        TS_ASSERT_EQUALS( how, "has" )

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
        pr = P.insert(tagd::make_predicate("is_a", "animal"));
        pr = P.insert(tagd::make_predicate("has", "fins"));
        pr = P.insert(tagd::make_predicate("breaths", "water"));
        fish.predicates(P);

		TS_ASSERT_EQUALS( fish.is_a(), "animal");
		TS_ASSERT( fish.related("has", "fins") )
		TS_ASSERT( fish.related("breaths", "water") )
    }

    void test_tag_copy(void) {
        tagd::tag fish("fish", "animal");
        fish.relation(tagd::make_predicate("has", "fins"));

        tagd::abstract_tag a(fish);
		TS_ASSERT_EQUALS( a.id(), "fish" );
		TS_ASSERT_EQUALS( a.super(), "animal" );
		TS_ASSERT( a.related("has", "fins") )

        tagd::abstract_tag b;
        b = fish;
		TS_ASSERT_EQUALS( b.id(), "fish" );
		TS_ASSERT_EQUALS( b.super(), "animal" );
		TS_ASSERT( b.related("has", "fins") )
    }

    void test_relator(void) {
		tagd::relator r1("has");
		TS_ASSERT( r1.id() == "has" )
		TS_ASSERT( r1.type_of() == "_relator" )
		TS_ASSERT( r1.pos() == tagd::POS_VERB )

		tagd::relator r2("can", "verb");
		TS_ASSERT( r2.id() == "can" )
		TS_ASSERT( r2.type_of() == "verb" )
		TS_ASSERT( r2.pos() == tagd::POS_VERB )
	}

	// test url relations like other tags
	// parsing tests go in UrlTester
    void test_url(void) {
		tagd::url u("http://hypermega.com");
        u.relation("has","links", "4");
        u.relation("about", "computer_security");
        // id() an alias for hduri
		TS_ASSERT_EQUALS( u.id() , "com:hypermega:http" )
		TS_ASSERT_EQUALS( u.super() , "_url" )
		TS_ASSERT( u.pos() == tagd::POS_URL )
		TS_ASSERT( u.related("has", "links") )
		TS_ASSERT( u.related("about", "computer_security") )
	}

    void test_interrogator(void) {
		tagd::interrogator a("what");
		TS_ASSERT( a.id() == "what" )
		TS_ASSERT( a.type_of().empty() )
		TS_ASSERT( a.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator b("what", "mammal");
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT( b.type_of() == "mammal" )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )
	}

    void test_ostream_operator(void) {
        tagd::tag dog("dog","animal");
        dog.relation("has", "tail");
        dog.relation("has", "fur");
        dog.relation("can", "bark");
    }

};
