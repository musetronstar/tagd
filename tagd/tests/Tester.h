// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>
#include "tagd.h"

#define TAGD_CODE_STRING(c)	std::string(tagd_code_str(c))

class Tester : public CxxTest::TestSuite {
	public:

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
        r1.init(a1);
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

        char b = r1.pop_back();
        TS_ASSERT_EQUALS( b , 3 );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        rc = r1.push_back();
        TS_ASSERT_EQUALS( rc , tagd::TAGD_OK );
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5.1" );

        r1.pop_back();    
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );

        // rank order
        tagd::rank r2;
        a1[2] = 2;
        r2.init(a1);
        TS_ASSERT( r2 < r1 );  // 1.2.2 < 1.2.5

        // push unallocated
        tagd::rank r4;
        rc = r4.push_back();
        TS_ASSERT_EQUALS( rc , tagd::TAGD_OK );
        TS_ASSERT_EQUALS( r4.dotted_str() , "1" );
        TS_ASSERT_EQUALS( r4.back() , 1 );
        TS_ASSERT_EQUALS( r4.size() , 1 );

        // pop to empty
        b = r4.pop_back();
        TS_ASSERT_EQUALS( b, 1 );
        b = r4.pop_back();
        TS_ASSERT( r4.empty() );
        TS_ASSERT_EQUALS( b, '\0' );
        b = r4.pop_back(); // again
        TS_ASSERT_EQUALS( b, '\0' );

        // pop unallocated
        tagd::rank r5;
        b = r4.pop_back();
        TS_ASSERT_EQUALS( b, '\0' );
        TS_ASSERT_EQUALS( r4.dotted_str() , "" );
        TS_ASSERT_EQUALS( r4.back() , '\0' );
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
        char a1[4] = {1, 2, 5, '\0'};

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
        tagd::code rc;
        a1[2] = 1;
        while ((unsigned char)a1[2]++ < 255) {
            rc = r1.init(a1);
            if (rc == tagd::TAGD_OK)
                R.insert(r1);
        }

        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_BYTE);
        TS_ASSERT_EQUALS( R.size() , 254 );
    }

	void test_rank_increment(void) {
        char a1[4] = {1, 2, 1, '\0'};

        tagd::rank r1;
        r1.init(a1);

        tagd::code rc;
        while (r1.back() <= tagd::RANK_MAX_BYTE) {
            rc = r1.increment();
            if (rc != tagd::TAGD_OK)
                break;
        }

        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_BYTE");
    }

    void test_rank_maximums(void) {
        // test RANK_MAX_LEN
        char max[tagd::RANK_MAX_LEN+1];
        std::fill(max, (max+tagd::RANK_MAX_LEN+1), 1);

        tagd::rank r1;
        tagd::code rc = r1.init(max);
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);

        for (size_t i=0; i<tagd::RANK_MAX_LEN+1; ++i) {
            rc = r1.push_back();
        }
        TS_ASSERT_EQUALS (rc , tagd::RANK_MAX_LEN);
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

		A.clear();
        A.insert(animal);
		A.insert(cat);
		A.insert(bird);

		B.clear();
        B.insert(animal);

        merge_containing_tags(A, B);

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

//    void test_abstract_tag(void) {
//		tagd::abstract_tag e("_entity", "_entity");
//		TS_ASSERT_EQUALS( e.id() , "_entity" )
//		TS_ASSERT( e.super().empty() )  // override, _entity super is always empty
//		TS_ASSERT_EQUALS( e.pos() , tagd::POS_TAG )
//	}

	void test_tag(void) {
		tagd::tag dog("dog", "animal");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
	}

	void test_tag_clear(void) {
		tagd::tag dog("dog", "animal");
		dog.relation("has", "teeth");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
		TS_ASSERT( dog.related("has", "teeth")  )
		dog.clear();
		TS_ASSERT( dog.id().empty() )
		TS_ASSERT( dog.super().empty() ) 
		// POS doesn't get set to UNKNOWN
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
		TS_ASSERT( !dog.related("has", "teeth")  )
		TS_ASSERT( dog.empty() )
	}

	void test_tag_eq(void) {
		tagd::tag a("dog", "animal");
		tagd::tag b("dog", "mammal");

		TS_ASSERT( a.id() == b.id() )
		TS_ASSERT( a != b )

        a.super("mammal");
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
		TS_ASSERT( dog.super() == "animal" )
	}

    void test_related(void) {
		tagd::tag dog("dog", "animal");
        TS_ASSERT_EQUALS( dog.relation("has","teeth"), tagd::TAGD_OK )
        TS_ASSERT_EQUALS( dog.relation("has","legs", "4"), tagd::TAGD_OK )
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
        pr = P.insert(tagd::make_predicate("has", "fins"));
        pr = P.insert(tagd::make_predicate("breaths", "water"));
        fish.predicates(P);

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
		TS_ASSERT( r1.super() == "_relator" )
		TS_ASSERT( r1.pos() == tagd::POS_RELATOR )

		tagd::relator r2("can", "verb");
		TS_ASSERT( r2.id() == "can" )
		TS_ASSERT( r2.super() == "verb" )
		TS_ASSERT( r2.pos() == tagd::POS_RELATOR )
	}

	// test url relations like other tags
	// parsing tests go in UrlTester
    void test_url(void) {
		tagd::url u("http://hypermega.com");
        u.relation("has","links", "4");
        u.relation("about", "computer_security");
		TS_ASSERT_EQUALS( u.hduri() , "com:hypermega:http" )
		TS_ASSERT_EQUALS( u.super() , "_url" )
		TS_ASSERT( u.pos() == tagd::POS_URL )
		TS_ASSERT( u.related("has", "links") )
		TS_ASSERT( u.related("about", "computer_security") )
	}

    void test_interrogator(void) {
		tagd::interrogator a("what");
		TS_ASSERT( a.id() == "what" )
		TS_ASSERT( a.super().empty() )
		TS_ASSERT( a.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator b("what", "mammal");
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT( b.super() == "mammal" )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )
	}

	void test_referent(void) {
		tagd::referent a("is_a", "_is_a");
		TS_ASSERT_EQUALS( a.refers() , "is_a" )
		TS_ASSERT_EQUALS( a.refers_to() , "_is_a" )
		TS_ASSERT( a.pos() == tagd::POS_REFERENT )
		TS_ASSERT( a.context().empty() )

		tagd::referent b("perro", "dog", "spanish");
		TS_ASSERT_EQUALS( b.refers() , "perro" )
		TS_ASSERT_EQUALS( b.refers_to() , "dog" )
		TS_ASSERT( b.pos() == tagd::POS_REFERENT )
		TS_ASSERT_EQUALS( b.context() , "spanish" )
	}

	void test_referent_set(void) {
		tagd::referent a("thing", "_entity");
		tagd::referent b("thing", "monster", "movies");
		tagd::referent c("thing", "physical_object", "substance");

		tagd::tag_set R;
		R.insert(a);
		R.insert(b);
		R.insert(c);

		TS_ASSERT_EQUALS( R.size() , 3 ) 
	}

    void test_error(void) {
        tagd::error err(tagd::TAG_ILLEGAL);
		TS_ASSERT_EQUALS( err.code(), tagd::TAG_ILLEGAL ) 
		TS_ASSERT_EQUALS( err.id(), "TAG_ILLEGAL" ) 
		TS_ASSERT_EQUALS( err.super(), HARD_TAG_ERROR ) 
    }

};
