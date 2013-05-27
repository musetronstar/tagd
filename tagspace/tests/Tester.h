// Test suites

#include <cxxtest/TestSuite.h>

#include "tagd.h"
#include "tagspace/sqlite.h"

//const std::string db_fname = "tagspace.sqlite";
const std::string db_fname = ":memory:";

// _entity             _is_a     _entity                                 
// physical_object     _is_a     _entity             1                   
// living_thing        _is_a     physical_object     1.1                 
// animal              _is_a     living_thing        1.1.1               
// vertibrate          _is_a     animal              1.1.1.1             
// mammal              _is_a     vertibrate          1.1.1.1.1           
// dog                 _is_a     mammal              1.1.1.1.1.1         
// cat                 _is_a     mammal              1.1.1.1.1.2         
// bird                _is_a     vertibrate          1.1.1.1.2           
// canary              _is_a     bird                1.1.1.1.2.1         
// invertebrate        _is_a     animal              1.1.1.2             
// arthropod           _is_a     invertebrate        1.1.1.2.1           
// insect              _is_a     arthropod           1.1.1.2.1.1         
// spider              _is_a     insect              1.1.1.2.1.1.1       
// relation            _is_a     _entity             2                   
// word                _is_a     relation            2.1                 
// verb                _is_a     word                2.1.1

// TODO we may wish to define this in a config.h
// generated by the Makefile, so that these tests
// can be used by other tagspace implementation
typedef tagspace::sqlite space_type;

// populate tags minimally so we can test inserting, etc
void populate_tags_minimal(space_type& TS) {
    tagd::tag physical_object("physical_object", "_entity");
    TS.put(physical_object);
    tagd::tag living_thing("living_thing", "physical_object");
    TS.put(living_thing);
    tagd::tag animal("animal", "living_thing");
    TS.put(animal);
    tagd::tag vertibrate("vertibrate", "animal");
    TS.put(vertibrate);
    tagd::tag mammal("mammal","vertibrate");
    TS.put(mammal);
    tagd::tag dog("dog","mammal");
    TS.put(dog);
    tagd::tag cat("cat","mammal");
    TS.put(cat);
    tagd::tag bird("bird","vertibrate");
    TS.put(bird);
    tagd::tag canary("canary","bird");
    TS.put(canary);
    tagd::tag invertebrate("invertebrate","animal");
    TS.put(invertebrate);
    tagd::tag arthropod("arthropod","invertebrate");
    TS.put(arthropod);
    tagd::tag insect("insect","arthropod");
    TS.put(insect);
    tagd::tag n("spider","insect");

    // TODO change plural referents when referents in place
    tagd::tag body_part("body_part","physical_object");
    TS.put(body_part);
    tagd::tag teeth("teeth","body_part");  // plural referent => tooth, for now, what the hell
    TS.put(teeth);
    tagd::tag tail("tail","body_part");
    TS.put(tail);
    tagd::tag legs("legs","body_part");  // plural referent
    TS.put(legs);
    tagd::tag beak("beak","body_part");
    TS.put(beak);
    tagd::tag wings("wings","body_part");  // plural referent
    TS.put(wings);
    tagd::tag feathers("feathers","body_part");  // plural referent
    TS.put(feathers);
    tagd::tag fins("fins","body_part");  // plural referent
    TS.put(fins);

    tagd::tag event("event","_entity");
    TS.put(event);
    tagd::tag sound("sound","event");
    TS.put(sound);
    tagd::tag action("action","event");
    TS.put(action);
    tagd::tag utterance("utterance","sound");
    TS.put(utterance);
    tagd::tag bark("bark","utterance");
    TS.put(bark);
    tagd::tag meow("meow","utterance");
    TS.put(meow);
    // TODO these are verbs, how do we deal with them?
    tagd::tag move("move","action");  // to_move
    TS.put(move);
    tagd::tag fly("fly","move");    // to_fly
    TS.put(fly);
    tagd::tag swim("swim","move");    // to_swim
    TS.put(swim);

    tagd::tag _relator("_relator","_entity");
    TS.put(_relator);
    tagd::tag word("word","_relator");
    TS.put(word);
    tagd::tag verb("verb","word");
    TS.put(verb);

    tagd::relator has("has","verb");
    TS.put(has);
    tagd::relator can("can","verb");
    TS.put(can);
}


void populate_tags(space_type& TS) {
    tagd::tag physical_object("physical_object", "_entity");
    TS.put(physical_object);
    tagd::tag living_thing("living_thing", "physical_object");
    TS.put(living_thing);
    // TODO change plural referents when referents in place
    tagd::tag body_part("body_part","physical_object");
    TS.put(body_part);
    tagd::tag teeth("teeth","body_part");  // plural referent => tooth, for now, what the hell
    TS.put(teeth);
    tagd::tag fangs("fangs","teeth");
    TS.put(fangs);
    tagd::tag tail("tail","body_part");
    TS.put(tail);
    tagd::tag legs("legs","body_part");  // plural referent
    TS.put(legs);
    tagd::tag beak("beak","body_part");
    TS.put(beak);
    tagd::tag wings("wings","body_part");  // plural referent
    TS.put(wings);
    tagd::tag feathers("feathers","body_part");  // plural referent
    TS.put(feathers);
    tagd::tag fins("fins","body_part");  // plural referent
    TS.put(fins);

    tagd::tag mind("mind", "_entity");
    TS.put(mind);

    tagd::tag knowledge("knowledge", "mind");
    TS.put(knowledge);
    tagd::tag concept("concept", "knowledge");
    TS.put(concept);
    tagd::tag science("science", "knowledge");
    TS.put(science);
	tagd::tag cs("computer_security", "science");
	TS.put(cs);

	tagd::tag creativity("creativity", "mind");
	TS.put(creativity);
	tagd::tag art("art", "creativity");
	TS.put(art);
	tagd::tag visual_art("visual_art", "art");
	TS.put(visual_art);
	tagd::tag movie("movie", "visual_art");
	TS.put(movie);
	tagd::tag tv_show("tv_show", "visual_art");
	TS.put(tv_show);

    tagd::tag event("event","_entity");
    TS.put(event);
    tagd::tag sound("sound","event");
    TS.put(sound);
    tagd::tag action("action","event");
    TS.put(action);
    tagd::tag utterance("utterance","sound");
    TS.put(utterance);
    tagd::tag bark("bark","utterance");
    TS.put(bark);
    tagd::tag meow("meow","utterance");
    TS.put(meow);
    // TODO these are verbs, how do we deal with them?
    tagd::tag move("move","action");  // to_move
    TS.put(move);
    tagd::tag fly("fly","move");    // to_fly
    TS.put(fly);
    tagd::tag swim("swim","move");    // to_swim
    TS.put(swim);

    tagd::tag _relator("_relator","_entity");
    TS.put(_relator);
    tagd::tag word("word","_relator");
    TS.put(word);
    tagd::tag verb("verb","word");
    TS.put(verb);
    tagd::tag preposition("preposition","word");
    TS.put(preposition);

    tagd::relator has("has","verb");
    TS.put(has);
    tagd::relator can("can","verb");
    TS.put(can);
    tagd::relator about("about","preposition");
    TS.put(about);

    tagd::tag animal("animal", "living_thing");
    TS.put(animal);
    tagd::tag vertibrate("vertibrate", "animal");
    TS.put(vertibrate);

    tagd::tag mammal("mammal","vertibrate");
    mammal.relation("has", "teeth");
    TS.put(mammal);

    tagd::tag reptile("reptile","vertibrate");
    TS.put(reptile);
    tagd::tag invertebrate("invertebrate","animal");
    TS.put(invertebrate);
    tagd::tag arthropod("arthropod","invertebrate");
    TS.put(arthropod);
    tagd::tag insect("insect","arthropod");
    TS.put(insect);
    tagd::tag n("spider","insect");

    tagd::tag dog("dog", "mammal");
    dog.relation("has", "legs", "4");
    dog.relation("has", "tail");
    dog.relation("can", "bark");
    TS.put(dog);

    tagd::tag cat("cat", "mammal");
    cat.relation("has", "legs", "4");
    cat.relation("has", "tail");
    cat.relation("can", "meow");
    TS.put(cat);

    tagd::tag whale("whale", "mammal");
    whale.relation("has", "fins");
    whale.relation("can", "swim");
    TS.put(whale);

    tagd::tag bat("bat", "mammal");
    bat.relation("has", "wings");
    bat.relation("can", "fly");
    TS.put(bat);

    tagd::tag bird("bird", "vertibrate");
    bird.relation("has", "wings");
    bird.relation("has", "feathers");
    bird.relation("can", "fly");
    TS.put(bird);

    tagd::tag canary("canary","bird");
    TS.put(canary);

    tagd::tag spider("spider", "insect");
    spider.relation("has", "fangs");
    TS.put(spider);

    tagd::tag snake("snake", "reptile");
    snake.relation("has", "fangs");
    TS.put(snake);
}

bool tag_set_exists(const tagd::tag_set& S, tagd::id_type id) {
    for(tagd::tag_set::iterator it = S.begin(); it != S.end(); ++it)
        if (it->id() == id) return true;

    return false;
}

class Tester : public CxxTest::TestSuite {
    public:

    void test_init(void) {
        space_type TS;
        ts_res_code ts_rc = TS.init(db_fname);
        TS_ASSERT_EQUALS(ts_rc, TS_OK);

        tagd::tag t;
        ts_rc = TS.get(t, "_entity");
        TS_ASSERT_EQUALS(ts_rc, TS_OK);
        TS_ASSERT_EQUALS(t.id(), "_entity");
        TS_ASSERT_EQUALS(t.super(), "_entity");
    }

    void test_put_get_rank(void) {
        space_type TS;
        TS.init(db_fname);

        tagd::tag a("physical_object", "_entity");
        ts_res_code ts_rc = TS.put(a);

        tagd::tag b;
        ts_rc = TS.get(b, "physical_object");
        TS_ASSERT_EQUALS(ts_rc, TS_OK);
        TS_ASSERT_EQUALS(b.id(), "physical_object");
        TS_ASSERT_EQUALS(b.super(), "_entity");

		std::string r_dotted = b.rank().dotted_str();
		// living_thing will be first child
		r_dotted.append(".1");

        tagd::tag c("living_thing", "physical_object");
        ts_rc = TS.put(c);
        TS_ASSERT_EQUALS(ts_rc, TS_OK);
        // rank updated
        TS_ASSERT_EQUALS(c.rank().dotted_str(), r_dotted);
    }

    void test_exists(void) {
        space_type TS;
        TS.init(db_fname);
        populate_tags(TS);

        ts_res_code ts_rc = TS.exists("dog");
        TS_ASSERT_EQUALS( ts_rc, TS_OK );

        ts_rc = TS.exists("unicorn");
        TS_ASSERT_EQUALS( ts_rc, TS_NOT_FOUND );
    }

    void test_insert_statements(void) {
        space_type TS;
        TS.init(db_fname);
        populate_tags_minimal(TS);

        tagd::tag a("dog", "mammal");  // existing
        a.relation("has", "teeth");
        a.relation("has", "tail");
        a.relation("has", "legs", "4");
        ts_res_code ts_rc = TS.put(a);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

        tagd::tag b("robin", "bird");  // non-existing
        b.relation("has", "beak");
        b.relation("has", "wings");
        b.relation("has", "feathers");
        ts_rc = TS.put(b);
        TS_ASSERT_EQUALS(ts_rc, TS_OK);

        // already existing, only put relations
        tagd::tag c("dog");
        TS_ASSERT_EQUALS(c.super(), "");
        c.relation("can", "bark");
        ts_rc = TS.put(c);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");
    }

    void test_duplicate(void) {
        space_type TS;
        TS.init(db_fname);
        populate_tags_minimal(TS);

        tagd::tag dog("dog", "mammal");
        ts_res_code ts_rc = TS.put(dog); // duplicate tag, no statements
        TS_ASSERT_EQUALS( ts_rc, TS_DUPLICATE );

        dog.relation("has", "tail");
        ts_rc = TS.put(dog); // not duplicate, statement was inserted
        TS_ASSERT_EQUALS( ts_rc, TS_OK );

        ts_rc = TS.put(dog); // tag duplicate, statement duplicate
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_DUPLICATE" );

        dog.relation("can", "bark");
        ts_rc = TS.put(dog); // one duplicate (has tail), one insert (can bark)
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );

        tagd::tag a("dog");
        a.relation("has", "tail");
        ts_rc = TS.put(a);  // existing tag, empty super, existing relation
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_DUPLICATE" );

        a.relation("has", "teeth");
        ts_rc = TS.put(a);  // existing tag, empty super, one new relation
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
    }

    void test_undef_tag_refs(void) {
        space_type TS;
        TS.init(db_fname);
        populate_tags_minimal(TS);

        tagd::tag dog("dog", "nosuchthing");
        ts_res_code ts_rc = TS.put(dog);
        TS_ASSERT_EQUALS( ts_rc, TS_SUPER_UNK );

        dog.super("mammal");
        dog.relation("has", "tailandwings");
        ts_rc = TS.put(dog);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OBJECT_UNK" );

        dog.relations.clear();
        TS_ASSERT_EQUALS( tag_code_str(dog.relation("flobdapperbates", "tail")), "TAG_OK" );
        ts_rc = TS.put(dog);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_RELATOR_UNK" );

        tagd::tag a("dog");  // existing no super, no relations
        ts_rc = TS.put(a);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_DUPLICATE" );
    }

    void test_related(void) {
        space_type TS;
        TS.init(db_fname);
        //TS.init("tagspace.sqlite");
        populate_tags(TS);

        //TS.dump();
        //TS.dump_relators();
		ts_res_code ts_rc;
		tagd::tag t;

        tagd::tag_set S;
        ts_rc = TS.related(S, tagd::make_predicate("has", "legs")); 
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 2 );

        S.empty();
        ts_rc = TS.related(S, tagd::make_predicate("_relator", "body_part")); 
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 8 );
        //for(tagd::tag_set::iterator it = S.begin(); it != S.end(); ++it) {
        //   std::cout << *it << std::endl;
        //}

        // TODO test related with existing and not existing supers

        ts_rc = TS.related(S, tagd::make_predicate("snarfs", "cockamamy")); 
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_NOT_FOUND" );
        TS_ASSERT_EQUALS( S.size(), 0 );
    }

    // TODO
    void test_query(void) {
        space_type TS;
        TS.init(db_fname);
        populate_tags(TS);

        // TODO "what" is not actually used in the query
        // this will change over time, as different types
        // of interrogators will denote different queries
        tagd::interrogator q("what", "mammal");
        q.relation("can", "swim");

        tagd::tag_set S;
        ts_res_code ts_rc = TS.query(S, q);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 1 );
        TS_ASSERT( tag_set_exists(S, "whale") );

        S.clear();
        tagd::interrogator r("what");
        r.relation("has", "fangs");
        ts_rc = TS.query(S, r);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 2 );
        TS_ASSERT( tag_set_exists(S, "spider") );
        TS_ASSERT( tag_set_exists(S, "snake") );

        S.clear();
        tagd::interrogator s("what");
        s.relation("has", "teeth");
        ts_rc = TS.query(S, s);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 3 );
        TS_ASSERT( tag_set_exists(S, "mammal") );
        TS_ASSERT( tag_set_exists(S, "spider") );  // fangs are teeth
        TS_ASSERT( tag_set_exists(S, "snake") );

        // for(tagd::tag_set::iterator it = S.begin(); it != S.end(); ++it) {
        //    std::cout << *it << std::endl;
        // }
    }

    void test_relations(void) {
		space_type TS;
        TS.init(db_fname);
        populate_tags(TS);

        tagd::tag dog;
        ts_res_code ts_rc = TS.get(dog, "dog");

        TS_ASSERT_EQUALS( dog.relations.size(), 3 )
		TS_ASSERT( dog.related(tagd::make_predicate("has", "legs", "4"))  )
		TS_ASSERT( dog.related("has", "tail")  )

        tagd::id_type how;
		TS_ASSERT( dog.related("bark", &how) )
        TS_ASSERT_EQUALS( how, "can" )

		TS_ASSERT( !dog.related("has", "fins") )
	}

/// URI TESTS ///

    void test_put_url(void) {
        // std::cout << "\n#################### test_put_uri ####################\n";
        space_type TS;
        ts_res_code ts_rc = TS.init(db_fname);
        //ts_res_code ts_rc = TS.init("tagspace.db");
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

        populate_tags(TS);

        tagd::url a("http://www.hypermega.com/a/b/c?x=1&y=2#here");
        a.relation("about", "computer_security");
        ts_rc = TS.put(a);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

        tagd::url b;
        ts_rc = TS.get(b, a.id()); 
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");
		TS_ASSERT_EQUALS(b.str(), "http://www.hypermega.com/a/b/c?x=1&y=2#here"); 
        TS_ASSERT( b.relations.size() > 1 )
        TS_ASSERT( b.related("about", "computer_security") )
        TS_ASSERT( b.related("_has", HARD_TAG_SCHEME, "http") )
        TS_ASSERT( b.related("_has", HARD_TAG_HOST, "hypermega.com") )
        TS_ASSERT( b.related("_has", HARD_TAG_PRIV_LABEL, "hypermega") )
        TS_ASSERT( b.related("_has", HARD_TAG_PUB, "com") )
        TS_ASSERT( b.related("_has", HARD_TAG_SUB, "www") )
        TS_ASSERT( b.related("_has", HARD_TAG_PATH, "/a/b/c") )
        TS_ASSERT( b.related("_has", HARD_TAG_QUERY, "?x=1&y=2") )
        TS_ASSERT( b.related("_has", HARD_TAG_FRAGMENT, "here") )

        // TS.dump_uridb();
    }

    // TODO
    void test_query_url(void) {
        space_type TS;
		ts_res_code ts_rc;
        TS.init(db_fname);
        // TS.init("tagspace.sqlite");
        populate_tags(TS);

TS.trace_on();
        tagd::url a("http://en.wikipedia.org/wiki/Dog");
        a.relation("about", "dog");
        ts_rc = TS.put(a);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

		tagd::tag starwars("starwars", "movie");
		TS.put(starwars);
        tagd::url b("http://starwars.wikia.com/wiki/Dog");
        b.relation("about", "starwars");
        ts_rc = TS.put(b);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

        tagd::url c("http://animal.discovery.com/tv-shows/dogs-101");
        c.relation("about", "dog");
        ts_rc = TS.put(c);
        TS_ASSERT_EQUALS(ts_res_str(ts_rc), "TS_OK");

        // TODO "what" is not actually used in the query
        tagd::interrogator q("what", "_url");
        q.relation("about", "dog");

		// TS.dump();

        tagd::tag_set S;
        ts_rc = TS.query(S, q);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 2 );
        TS_ASSERT( tag_set_exists(S, "org:wikipedia:en:/wiki/Dog:http") );
        TS_ASSERT( tag_set_exists(S, "com:discovery:animal:/tv-shows/dogs-101:http") );

        S.clear();
        tagd::interrogator r("what", "_url");
        r.relation("_has", "_path", "/wiki/Dog");
        ts_rc = TS.query(S, r);
        TS_ASSERT_EQUALS( ts_res_str(ts_rc), "TS_OK" );
        TS_ASSERT_EQUALS( S.size(), 2 );
        TS_ASSERT( tag_set_exists(S, "org:wikipedia:en:/wiki/Dog:http") );
        TS_ASSERT( tag_set_exists(S, "com:wikia:starwars:/wiki/Dog:http") );
TS.trace_off();
    }
};
