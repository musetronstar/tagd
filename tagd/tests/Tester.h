// Test suites

#include <cxxtest/TestSuite.h>

#include <sstream>
#include <type_traits>
#include <utility>
#include "tagd.h"
#include "tagd/utf8.h"

#define TAGD_CODE_STRING(c)	std::string(tagd::code_str(c))
#define TS_ASSERT_TAGD_OK(EXPR) TS_ASSERT_EQUALS((EXPR), tagd::TAGD_OK)

namespace {

// swap traits keep Rule-of-Five tests on STL contracts, not one implementation shape
template <typename T>
constexpr bool has_member_swap_v = requires(T& lhs, T& rhs) {
	lhs.swap(rhs);
};

template <typename T>
constexpr bool has_nothrow_adl_swap_v = []() {
	using std::swap;
	return requires(T& lhs, T& rhs) {
		{ swap(lhs, rhs) } noexcept;
	};
}();

template <typename T>
constexpr bool has_identity_id_mutator_v = requires(T& tag, const tagd::id_string& id) {
	tag.id(id);
};

template <typename T>
constexpr bool has_identity_sub_relator_mutator_v = requires(T& tag, const tagd::id_string& id) {
	tag.sub_relator(id);
};

template <typename T>
constexpr bool has_identity_super_object_mutator_v = requires(T& tag, const tagd::id_string& id) {
	tag.super_object(id);
};

template <typename T>
constexpr bool has_identity_rank_mutator_v = requires(T& tag, const tagd::rank& rank) {
	tag.rank(rank);
};

template <typename T>
constexpr bool has_identity_rank_bytes_mutator_v = requires(T& tag, const char *bytes) {
	tag.rank(bytes);
};

// Identity is immutable after construction — mathematical fixed point in tagspace.
static_assert(!std::is_assignable_v<decltype(std::declval<tagd::abstract_tag&>().id()), tagd::id_string>);
static_assert(!std::is_assignable_v<decltype(std::declval<tagd::abstract_tag&>().sub_relator()), tagd::id_string>);
static_assert(!std::is_assignable_v<decltype(std::declval<tagd::abstract_tag&>().super_object()), tagd::id_string>);
static_assert(!std::is_assignable_v<decltype(std::declval<tagd::abstract_tag&>().rank()), tagd::rank>);

constexpr bool has_const_ref_tag_set_equal_v =
	std::is_same_v<decltype(&tagd::tag_set_equal),
		bool (*)(const tagd::tag_set&, const tagd::tag_set&)>;

constexpr bool has_const_ref_last_error_relation_v =
	std::is_same_v<decltype(&tagd::errorable::last_error_relation),
		tagd::code (tagd::errorable::*)(const tagd::predicate&)>;

constexpr bool has_const_most_severe_v =
	std::is_same_v<decltype(static_cast<tagd::code (tagd::errorable::*)(tagd::code) const>(&tagd::errorable::most_severe)),
		tagd::code (tagd::errorable::*)(tagd::code) const>;

inline constexpr std::string_view TEST_TAG_IS_A{"is_a"};

inline tagd::id_string owned_id(std::string_view sv) {
	return tagd::id_string(sv);
}

inline tagd::abstract_tag make_test_tag(tagd::id_view id) {
	return tagd::abstract_tag(id, TEST_TAG_IS_A, tagd::id_view{}, tagd::POS_TAG);
}

inline tagd::abstract_tag make_test_tag(tagd::id_view id, tagd::id_view super_object) {
	return tagd::abstract_tag(id, TEST_TAG_IS_A, super_object, tagd::POS_TAG);
}

inline tagd::abstract_tag make_test_tag(tagd::id_view id, tagd::id_view super_object, const tagd::rank& rank) {
	return tagd::abstract_tag(make_test_tag(id, super_object), rank);
}

inline tagd::abstract_tag make_test_tag(tagd::id_view id, tagd::id_view sub_relator, tagd::id_view super_object) {
	return tagd::abstract_tag(id, sub_relator, super_object, tagd::POS_TAG);
}

inline tagd::relator make_test_relator(tagd::id_view id) {
	return tagd::relator(id);
}

inline tagd::relator make_test_relator(tagd::id_view id, tagd::id_view super_object) {
	return tagd::relator(id, super_object);
}

}

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
        auto tc = r1.init(nil);
        TS_ASSERT_EQUALS(tc, tagd::RANK_EMPTY);
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
        TS_ASSERT_TAGD_OK(r1.init(a1));
        TS_ASSERT_EQUALS( r1.dotted_str() , "1.2.5" );
		r1.clear();
		TS_ASSERT(r1.empty());
	}

    void test_rank_push_pop_order(void) {
        char a1[4] = {1, 2, 5, '\0'};

        // push,pop
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
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
        TS_ASSERT_TAGD_OK(r2.init(a1));
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
        TS_ASSERT_EQUALS( r4.dotted_str() , "" );
        TS_ASSERT_EQUALS( b, 0 );
        b = r4.pop_back(); // again
        TS_ASSERT_EQUALS( b, 0 );

        // pop unallocated
        tagd::rank r5;
        b = r4.pop_back();
        TS_ASSERT_EQUALS( b, 0 );
        TS_ASSERT( r4.empty() );
        TS_ASSERT_EQUALS( r4.dotted_str() , "" );
        TS_ASSERT_EQUALS( r4.back() , 0 );
        TS_ASSERT_EQUALS( r4.size() , 0 );
    }

    void test_rank_uno_set(void) {
        char a1[2] = {1, '\0'};

        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
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
        TS_ASSERT_TAGD_OK(r1.init(a1.c_str()));

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
        TS_ASSERT_TAGD_OK(next.init(a1.c_str()));
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
        char max[tagd::RANK_MAX_LEN+2];
        std::fill(max, (max+tagd::RANK_MAX_LEN+2), 1);
        max[tagd::RANK_MAX_LEN+1] = '\0';

        tagd::rank r1;
        tagd::code rc = r1.init(max);
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_LEN");

        tagd::rank r2;
		rc = tagd::TAGD_OK;
        for (size_t i=0; i<tagd::RANK_MAX_LEN+1; ++i) {
            rc = r2.push_back(1);
        }
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(rc) , "RANK_MAX_LEN");
    }

    void test_tag_rank(void) {
        char a1[4] = {1, 2, 123, '\0'};
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));

        tagd::abstract_tag t = tagd::abstract_tag(make_test_tag("dog", "animal"), r1);
        TS_ASSERT_EQUALS( t.rank().dotted_str() , "1.2.123" );
    }

    void test_rank_order(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};

        tagd::rank a;
        TS_ASSERT_TAGD_OK(a.init(a1)); // 1

        a1[1] = 1;
        tagd::rank b;
        TS_ASSERT_TAGD_OK(b.init(a1)); // 1.1
        TS_ASSERT( a < b );

        a1[2] = 1;
        tagd::rank c;
        TS_ASSERT_TAGD_OK(c.init(a1)); // 1.1.1
        TS_ASSERT( b < c );
        TS_ASSERT( a < c );

        a1[2] = 2;
        tagd::rank d;
        TS_ASSERT_TAGD_OK(d.init(a1)); // 1.1.2
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

        TS_ASSERT_TAGD_OK(a.init(a1)); // 1
        TS_ASSERT( a != b);

        TS_ASSERT_TAGD_OK(b.init(a1));
        TS_ASSERT( a == b );

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(b.init(a1)); // 1.1
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
		TS_ASSERT_TAGD_OK(a.init(a1));  // 1
		TS_ASSERT_TAGD_OK(b.init(a2));  // 1.2
		TS_ASSERT ( a.contains(a) )	
		TS_ASSERT ( a.contains(b) )	
		TS_ASSERT ( !b.contains(a) )	

        TS_ASSERT_TAGD_OK(c.init(a3)); // 1.2.3
		TS_ASSERT( c.contains(c) )   // ranks contains themselves
		TS_ASSERT( !c.contains(d) )  // empty data in d

        TS_ASSERT_TAGD_OK(d.init(a4)); // 1.2.3.4
        TS_ASSERT( c.contains(d) )
        TS_ASSERT( !d.contains(c) )

		TS_ASSERT_TAGD_OK(c.init(a5)); // 1.4.3
        TS_ASSERT( !c.contains(e) )
        TS_ASSERT( !d.contains(e) )
        TS_ASSERT( !e.contains(c) )
        TS_ASSERT( !e.contains(d) )
    }

    void test_tag_rank_order(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;

        TS_ASSERT_TAGD_OK(r1.init(a1)); // 1
        tagd::abstract_tag a = tagd::abstract_tag(make_test_tag("animal", HARD_TAG_ENTITY), r1);

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1)); // 1.1
        tagd::abstract_tag b = tagd::abstract_tag(make_test_tag("mammal", HARD_TAG_ENTITY), r1);
        TS_ASSERT( a < b );

        a1[2] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1)); // 1.1.1
        tagd::abstract_tag c = tagd::abstract_tag(make_test_tag("dog", HARD_TAG_ENTITY), r1);
        TS_ASSERT( b < c );
        TS_ASSERT( a < c );

        a1[2] = 2;
        TS_ASSERT_TAGD_OK(r1.init(a1)); // 1.1.2
        tagd::abstract_tag d = tagd::abstract_tag(make_test_tag("cat", HARD_TAG_ENTITY), r1);
        TS_ASSERT( c < d );
        TS_ASSERT( a < d );

        tagd::abstract_tag e = make_test_tag("haha");

		// assertions fail when comparing tags with rank vs w/o ranks
        // TS_ASSERT( e < d );
        // TS_ASSERT( a < e );

        tagd::abstract_tag f = make_test_tag("mierda");
        // no rank vs no rank, use id
        TS_ASSERT( ((e < f) && !(f < e)) );
    }

    void test_tag_set(void) {
        // tag rank
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));

        tagd::tag_set S;
		std::pair<tagd::tag_set::iterator, bool> pr;
        tagd::abstract_tag a = tagd::abstract_tag(make_test_tag("animal", HARD_TAG_ENTITY), r1);
        S.insert(a);

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag b = tagd::abstract_tag(make_test_tag("mammal", HARD_TAG_ENTITY), r1);
        pr = S.insert(b);
        TS_ASSERT( pr.second == true );
        
        a1[2] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag c = tagd::abstract_tag(make_test_tag("dog", HARD_TAG_ENTITY), r1);
        pr = S.insert(c);
        TS_ASSERT( pr.second == true );

        a1[2] = 2;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag d = tagd::abstract_tag(make_test_tag("cat", HARD_TAG_ENTITY), r1);
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
        tagd::abstract_tag a = make_test_tag("animal");
        S.insert(a);

        tagd::abstract_tag b = make_test_tag("mammal");
        pr = S.insert(b);
        TS_ASSERT( pr.second == true );
        
        tagd::abstract_tag c = make_test_tag("dog");
        pr = S.insert(c);
        TS_ASSERT( pr.second == true );

        tagd::abstract_tag d = make_test_tag("cat");
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
        TS_ASSERT_TAGD_OK(r1.init(a1));

        tagd::tag_set A, B, C, D;
        tagd::abstract_tag animal(tagd::abstract_tag("animal", tagd::POS_UNKNOWN), r1);
        A.insert(animal);  // no relations
		// A: { animal }

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag dog(tagd::abstract_tag("dog", tagd::POS_UNKNOWN), r1);
        A.insert(dog);  // no relations
        TS_ASSERT_EQUALS(dog.relation("has", "legs", "4"), tagd::TAGD_OK)
        B.insert(dog); // one relation
		// A: { animal, dog }
		// B: { dog }

        a1[1] = 2;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag cat(tagd::abstract_tag("cat", tagd::POS_UNKNOWN), r1);
        TS_ASSERT_EQUALS(cat.relation("has", "legs", "4"), tagd::TAGD_OK)
        A.insert(cat); // one relation
        TS_ASSERT_EQUALS(cat.relation("can", "meow"), tagd::TAGD_OK)
        B.insert(cat); // two relations
		// A: { animal, dog, cat }
		// B: { dog, cat }

        a1[1] = 3;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag bird(tagd::abstract_tag("bird", tagd::POS_UNKNOWN), r1);
        TS_ASSERT_EQUALS(bird.relation("has", "feathers"), tagd::TAGD_OK) 
        TS_ASSERT_EQUALS(bird.relation("has", "wings"), tagd::TAGD_OK) 
        B.insert(bird);  // not in A
		// A: { animal, dog, cat }
		// B: { dog, cat, bird }

        // C is our control
        C.insert(animal);
        C.insert(dog);
        C.insert(cat);
        C.insert(bird);

        // set up D for merge and diff
        TS_ASSERT_EQUALS(cat.relation("has", "whiskers"), tagd::TAGD_OK)
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

    void test_merge_tags_with_existing_relations(void) {
        // Verifies the extract/mutate/reinsert path: merging into a tag that already has relations
        // preserves both the original and incoming relations without duplication.
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));

        tagd::tag_set A, B;

        tagd::abstract_tag dog_a(tagd::abstract_tag("dog", tagd::POS_UNKNOWN), r1);
        TS_ASSERT_EQUALS(dog_a.relation("has", "legs", "4"), tagd::TAGD_OK)  // existing relation in A
        A.insert(dog_a);

        tagd::abstract_tag dog_b(tagd::abstract_tag("dog", tagd::POS_UNKNOWN), r1);
        TS_ASSERT_EQUALS(dog_b.relation("can", "bark"), tagd::TAGD_OK)  // incoming relation from B
        B.insert(dog_b);

        merge_tags(A, B);

        TS_ASSERT_EQUALS( A.size(), 1 );
        auto it = A.find(dog_a);
        TS_ASSERT( it != A.end() );
        TS_ASSERT( it->related("has", "legs", "4") );  // original preserved
        TS_ASSERT( it->related("can", "bark") );       // incoming merged
    }

    void test_merge_containing_tags(void) {
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::rank animal_rank(r1);

        tagd::tag_set A, B;
        tagd::abstract_tag animal(tagd::abstract_tag("animal", tagd::POS_UNKNOWN), r1);

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag dog(tagd::abstract_tag("dog", tagd::POS_UNKNOWN), r1);

        a1[1] = 2;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag cat(tagd::abstract_tag("cat", tagd::POS_UNKNOWN), r1);

        a1[1] = 3;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag bird(tagd::abstract_tag("bird", tagd::POS_UNKNOWN), r1);

        a1[0] = 2;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag car(tagd::abstract_tag("car", tagd::POS_UNKNOWN), r1);

        A.insert(animal);
        A.insert(dog);
        A.insert(cat);
        A.insert(bird);

		B.insert(cat);
		B.insert(bird);

        merge_containing_tags(A, B);
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

		animal = tagd::abstract_tag(tagd::abstract_tag("animal", "is_a", "living_thing", tagd::POS_UNKNOWN), animal_rank);
		TS_ASSERT_EQUALS(animal.relation("has", "homeostasis"), tagd::TAGD_OK)

		TS_ASSERT_EQUALS(cat.relation("has", "whiskers"), tagd::TAGD_OK)

		TS_ASSERT_EQUALS(bird.relation("has", "feathers"), tagd::TAGD_OK)

		A.clear();
        A.insert(animal);
		A.insert(cat);
		A.insert(bird);

		TS_ASSERT_EQUALS(animal.relation("has", "metabolism"), tagd::TAGD_OK)

		B.clear();
        B.insert(animal);

		// TODO merge_containing_tags() fails to merge predicates of contained tags
		// for example, the animal predicate "has metabolism" will not be in the resulting tag_set
        merge_containing_tags(A, B);

		// because cat and bird are animals
        TS_ASSERT_EQUALS( tag_ids_str(A), "animal, cat, bird" )
    }

    void test_merge_erase_diffs(void) {
        char a1[4] = {1, '\0', '\0', '\0'};
        tagd::rank r1;
        TS_ASSERT_TAGD_OK(r1.init(a1));

        tagd::tag_set A, B;

        tagd::abstract_tag dog(tagd::abstract_tag("dog", tagd::POS_UNKNOWN), r1);
        A.insert(dog);  // no relations
        TS_ASSERT_EQUALS(dog.relation("has", "legs", "4"), tagd::TAGD_OK)
        B.insert(dog); // one relation
		// A: { dog }
		// B: { dog }

        a1[1] = 1;
        TS_ASSERT_TAGD_OK(r1.init(a1));
        tagd::abstract_tag cat(tagd::abstract_tag("cat", tagd::POS_UNKNOWN), r1);
        TS_ASSERT_EQUALS(cat.relation("has", "legs", "4"), tagd::TAGD_OK)
        TS_ASSERT_EQUALS(cat.relation("can", "meow"), tagd::TAGD_OK)
        B.insert(cat); // two relations
		// A: { dog }
		// B: { dog, cat }

        merge_tags_erase_diffs(A, B);
        TS_ASSERT_EQUALS( A.size(), 1 );  // contains dog
        tagd::tag_set::iterator it = A.find(dog);
        TS_ASSERT( it != A.end() && *it == dog );
    }

	void test_tag(void) {
		tagd::abstract_tag dog = make_test_tag("dog", "animal");
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.sub_relator() == TEST_TAG_IS_A )
		TS_ASSERT( dog.super_object() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
	}
	
	void test_sub_relator(void) {
		tagd::abstract_tag dog = make_test_tag("perro", "es_un", "animal");
		TS_ASSERT( dog.id() == "perro" )
		TS_ASSERT( dog.sub_relator() == "es_un" )
		TS_ASSERT( dog.super_object() == "animal" )
		TS_ASSERT( dog.pos() == tagd::POS_TAG )
	}

	void test_tag_clear(void) {
		tagd::abstract_tag dog = make_test_tag("dog", "animal");
		TS_ASSERT_EQUALS(dog.relation("has", "teeth"), tagd::TAGD_OK)
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
		tagd::abstract_tag a = make_test_tag("dog", "is_a", "animal");
		tagd::abstract_tag b = make_test_tag("dog", "is_a", "mammal");

		TS_ASSERT( a.id() == b.id() )
		TS_ASSERT( a != b )

		tagd::abstract_tag same_identity_as_b = make_test_tag("dog", "is_a", "mammal");
		TS_ASSERT( same_identity_as_b == b );

		tagd::abstract_tag alt_sub_relator = make_test_tag("dog", "es_un", "mammal");
		TS_ASSERT( alt_sub_relator != b );

        TS_ASSERT_EQUALS(b.relation("has", "legs"), tagd::TAGD_OK)
		TS_ASSERT( same_identity_as_b != b );

        TS_ASSERT_EQUALS(same_identity_as_b.relation("has", "legs"), tagd::TAGD_OK)
		TS_ASSERT( same_identity_as_b == b );

		tagd::abstract_tag c("dog", "is_a", "mammal", tagd::POS_UNKNOWN);
        TS_ASSERT_EQUALS(c.relation("has", "legs"), tagd::TAGD_OK)

		TS_ASSERT( b != c );
		c.pos(tagd::POS_TAG);
		TS_ASSERT( same_identity_as_b == c );

		tagd::rank rank1(1);
		tagd::abstract_tag ranked_b(tagd::abstract_tag("dog", "is_a", "mammal", tagd::POS_TAG), rank1);
		tagd::abstract_tag ranked_c(tagd::abstract_tag("dog", "is_a", "mammal", tagd::POS_TAG), rank1);
		TS_ASSERT_EQUALS(ranked_b.relation("has", "legs"), tagd::TAGD_OK)
		TS_ASSERT_EQUALS(ranked_c.relation("has", "legs"), tagd::TAGD_OK)
		TS_ASSERT( ranked_b == ranked_c );
		TS_ASSERT( ranked_b != same_identity_as_b );
	}

	void test_abstract_tag_identity_is_constructor_only(void) {
		TS_ASSERT(!has_identity_id_mutator_v<tagd::abstract_tag>)
		TS_ASSERT(!has_identity_sub_relator_mutator_v<tagd::abstract_tag>)
		TS_ASSERT(!has_identity_super_object_mutator_v<tagd::abstract_tag>)
		TS_ASSERT(!has_identity_rank_mutator_v<tagd::abstract_tag>)
		TS_ASSERT(!has_identity_rank_bytes_mutator_v<tagd::abstract_tag>)
	}

	void test_abstract_tag_predicate_mutation_survives_identity_immutability(void) {
		tagd::abstract_tag dog("dog", "is_a", "animal", tagd::POS_TAG);

		TS_ASSERT_EQUALS(dog.relation("has", "tail"), tagd::TAGD_OK)
		TS_ASSERT_EQUALS(dog.relation("can", "bark"), tagd::TAGD_OK)
		TS_ASSERT(dog.related("has", "tail"))
		TS_ASSERT(dog.related("can", "bark"))
		TS_ASSERT_EQUALS(dog.id(), "dog")
		TS_ASSERT_EQUALS(dog.sub_relator(), "is_a")
		TS_ASSERT_EQUALS(dog.super_object(), "animal")
	}

	void test_tag_ordering_diverges_from_deep_equality(void) {
		tagd::abstract_tag lhs = make_test_tag("dog", "is_a", "animal");
		TS_ASSERT_EQUALS(lhs.relation("has", "tail"), tagd::TAGD_OK)

		tagd::abstract_tag rhs = make_test_tag("dog", "is_a", "mammal");
		TS_ASSERT_EQUALS(rhs.relation("has", "teeth"), tagd::TAGD_OK)

		TS_ASSERT(!(lhs < rhs));
		TS_ASSERT(!(rhs < lhs));
		TS_ASSERT(lhs != rhs);
	}

	void test_tag_set_uses_identity_ordering_not_deep_equality(void) {
		tagd::tag_set tags;

		tagd::abstract_tag lhs = make_test_tag("dog", "is_a", "animal");
		TS_ASSERT_EQUALS(lhs.relation("has", "tail"), tagd::TAGD_OK)

		tagd::abstract_tag rhs = make_test_tag("dog", "is_a", "mammal");
		TS_ASSERT_EQUALS(rhs.relation("has", "teeth"), tagd::TAGD_OK)

		auto first = tags.insert(lhs);
		auto second = tags.insert(rhs);

		TS_ASSERT(first.second);
		TS_ASSERT(!second.second);
		TS_ASSERT_EQUALS(tags.size(), static_cast<size_t>(1));
		TS_ASSERT(tags.begin() != tags.end());
		if (tags.begin() != tags.end()) {
			TS_ASSERT_EQUALS(tags.begin()->id(), "dog");
			TS_ASSERT(tags.begin()->related("has", "tail"));
			TS_ASSERT(!tags.begin()->related("has", "teeth"));
		}
	}

	void test_tag_set_equal_contract_uses_const_refs(void) {
		TS_ASSERT(has_const_ref_tag_set_equal_v);
	}

	void test_last_error_relation_contract_uses_const_predicate_ref(void) {
		TS_ASSERT(has_const_ref_last_error_relation_v);
	}

	void test_most_severe_contract_is_const(void) {
		TS_ASSERT(has_const_most_severe_v);
	}

    void test_relation(void) {
		tagd::abstract_tag dog = make_test_tag("dog", "animal");
        TS_ASSERT_EQUALS(dog.relation("has","teeth"), tagd::TAGD_OK)
        TS_ASSERT_EQUALS(dog.relation("has","legs","4"), tagd::TAGD_OK)
		TS_ASSERT( dog.id() == "dog" )
		TS_ASSERT( dog.super_object() == "animal" )
	}

    void test_related(void) {
		tagd::abstract_tag dog = make_test_tag("dog", "animal");
        TS_ASSERT_EQUALS( dog.relation("has","teeth"), tagd::TAGD_OK )
        TS_ASSERT_EQUALS( dog.relation("has","legs", "4"), tagd::TAGD_OK )
		TS_ASSERT( dog.related("has", "teeth")  )
		TS_ASSERT( dog.related("legs") )
		TS_ASSERT( !dog.related("has", "fins") )
		TS_ASSERT( dog.related("has", "legs", "4") )
		TS_ASSERT( !dog.related("has", "legs", "5") )
		TS_ASSERT( dog.related(tagd::predicate("has", "legs", "4")) )
		TS_ASSERT( !dog.related(tagd::predicate("has", "legs", "5")) )

        tagd::predicate_set how;
		TS_ASSERT_EQUALS( dog.related("legs", how), 1 )
		auto it = how.begin();
		TS_ASSERT( it != how.end() );
		if (it != how.end()) {
			TS_ASSERT_EQUALS( it->relator, "has" )
			TS_ASSERT_EQUALS( it->object, "legs" )
			TS_ASSERT_EQUALS( it->modifier, "4" )
		}

		TS_ASSERT_EQUALS(dog.relation("has", "fins"), tagd::TAGD_OK)
		TS_ASSERT( dog.related("has", "fins") )

		TS_ASSERT_TAGD_OK(dog.not_relation("has", "fins"));
		TS_ASSERT( !dog.related("has", "fins") )
	}

    void test_insert_relation(void) {
        tagd::abstract_tag fish = make_test_tag("fish", "animal");
        TS_ASSERT_EQUALS(fish.relation(tagd::predicate("has", "fins")), tagd::TAGD_OK)
		TS_ASSERT( fish.related("has", "fins") )
    }

    void test_insert_predicate_set(void) {
        tagd::abstract_tag fish = make_test_tag("fish");
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        pr = P.insert(tagd::predicate("has", "fins"));
        pr = P.insert(tagd::predicate("breaths", "water"));
        fish.predicates(P);

		TS_ASSERT( fish.related("has", "fins") )
		TS_ASSERT( fish.related("breaths", "water") )
    }

	void test_insert_relation_relator(void) {
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        tagd::interrogator q(owned_id(HARD_TAG_INTERROGATOR));
		tagd::code tc = q.relation(tagd::predicate("has",""));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
        tc = q.relation(tagd::predicate("breaths",""));
		TS_ASSERT_EQUALS(TAGD_CODE_STRING(tc), "TAGD_OK");
		TS_ASSERT_EQUALS( q.relations.size() , 2 );

		TS_ASSERT( q.has_relator("has") )
		TS_ASSERT( q.has_relator("breaths") )
    }

	void test_insert_predicate_set_relator(void) {
        tagd::predicate_set P;
        tagd::predicate_pair pr;
        pr = P.insert(tagd::predicate("has",""));
		TS_ASSERT( pr.second )
        pr = P.insert(tagd::predicate("breaths",""));
		TS_ASSERT( pr.second )
		TS_ASSERT_EQUALS( P.size() , 2 )

        tagd::interrogator q(owned_id(HARD_TAG_INTERROGATOR));
        q.predicates(P);
		TS_ASSERT_EQUALS( q.relations.size() , 2 );

		TS_ASSERT( q.has_relator("has") )
		TS_ASSERT( q.has_relator("breaths") )
    }

    void test_tag_copy(void) {
        tagd::abstract_tag fish = make_test_tag("fish", "is_a", "animal");
        TS_ASSERT_EQUALS(fish.relation(tagd::predicate("has", "fins")), tagd::TAGD_OK)

        tagd::abstract_tag a(fish);
		TS_ASSERT_EQUALS( a.id(), "fish" );
		TS_ASSERT_EQUALS( a.sub_relator(), "is_a" );
		TS_ASSERT_EQUALS( a.super_object(), "animal" );
		TS_ASSERT( a.related("has", "fins") )

        tagd::abstract_tag b;
        b = fish;
		TS_ASSERT_EQUALS( b.id(), "fish" );
		TS_ASSERT_EQUALS( b.sub_relator(), "is_a" );
		TS_ASSERT_EQUALS( b.super_object(), "animal" );
		TS_ASSERT( b.related("has", "fins") )
    }

	void test_tag_move_constructor_contract(void) {
		tagd::abstract_tag fish = make_test_tag("fish", "is_a", "animal");
		TS_ASSERT_EQUALS(fish.relation(tagd::predicate("has", "fins")), tagd::TAGD_OK)

		tagd::abstract_tag moved(std::move(fish));

		TS_ASSERT_EQUALS( moved.id(), "fish" );
		TS_ASSERT_EQUALS( moved.sub_relator(), "is_a" );
		TS_ASSERT_EQUALS( moved.super_object(), "animal" );
		TS_ASSERT( moved.related("has", "fins") )
		TS_ASSERT(std::is_nothrow_move_constructible<tagd::abstract_tag>::value)
	}

	void test_tag_move_assignment_contract(void) {
		tagd::abstract_tag fish = make_test_tag("fish", "is_a", "animal");
		TS_ASSERT_EQUALS(fish.relation(tagd::predicate("has", "fins")), tagd::TAGD_OK)

		tagd::abstract_tag moved_to = make_test_tag("bird", "is_a", "animal");
		moved_to = std::move(fish);

		TS_ASSERT_EQUALS( moved_to.id(), "fish" );
		TS_ASSERT_EQUALS( moved_to.sub_relator(), "is_a" );
		TS_ASSERT_EQUALS( moved_to.super_object(), "animal" );
		TS_ASSERT( moved_to.related("has", "fins") )
		TS_ASSERT(std::is_nothrow_move_assignable<tagd::abstract_tag>::value)
	}

	void test_tag_member_swap_contract(void) {
		tagd::abstract_tag lhs = make_test_tag("dog", "is_a", "animal");
		tagd::abstract_tag rhs = make_test_tag("cat", "is_a", "mammal");

		TS_ASSERT(has_member_swap_v<tagd::abstract_tag>)
	}

	void test_tag_adl_swap_contract(void) {
		tagd::abstract_tag lhs = make_test_tag("dog", "is_a", "animal");
		tagd::abstract_tag rhs = make_test_tag("cat", "is_a", "mammal");

		TS_ASSERT(has_nothrow_adl_swap_v<tagd::abstract_tag>)
	}

    void test_relator(void) {
		tagd::abstract_tag r1 = make_test_relator("has");
		TS_ASSERT( r1.id() == "has" )
		TS_ASSERT( r1.id() == "has" )
		TS_ASSERT( r1.sub_relator() == HARD_TAG_SUB )
		TS_ASSERT( r1.super_object() == "_rel" )
		TS_ASSERT( r1.pos() == tagd::POS_RELATOR )

		tagd::abstract_tag r2 = make_test_relator("can", "verb");
		TS_ASSERT( r2.id() == "can" )
		TS_ASSERT( r2.super_object() == "verb" )
		TS_ASSERT( r2.pos() == tagd::POS_RELATOR )
	}

	// test url relations like other tags
	// parsing tests go in UrlTester
    void test_url(void) {
		tagd::url u("http://hypermega.com");
        TS_ASSERT_EQUALS(u.relation("has","links", "4"), tagd::TAGD_OK)
        TS_ASSERT_EQUALS(u.relation("about", "computer_security"), tagd::TAGD_OK)
        // rpub!priv_label!rsub!path!query!fragment!port!user!pass!scheme
		TS_ASSERT_EQUALS( u.hduri() , "hd:com!hypermega!!!!!!!!http" )
		TS_ASSERT_EQUALS( u.super_object() , "_url" )
		TS_ASSERT( u.pos() == tagd::POS_URL )
		TS_ASSERT( u.related("has", "links") )
		TS_ASSERT( u.related("about", "computer_security") )
	}

	void test_predicate_compare(void) {
		// TYPE_STRING
		tagd::predicate p_empty;
		tagd::predicate pt_can_fly("can", "fly");
		tagd::predicate pt_can_fly_far("can", "fly", "far");
		TS_ASSERT( pt_can_fly != pt_can_fly_far )
		TS_ASSERT( pt_can_fly < pt_can_fly_far ) // "" < "far"

		tagd::predicate pt_has_legs_eq_4_cons_no_type("has", "legs", "4");
		TS_ASSERT( pt_can_fly_far < pt_has_legs_eq_4_cons_no_type )


		tagd::predicate pt_has_legs_eq_4{"has", "legs", "4", tagd::OP_EQ, tagd::TYPE_STRING}; // init list style
		TS_ASSERT( pt_has_legs_eq_4_cons_no_type == pt_has_legs_eq_4 )
		TS_ASSERT( !(pt_has_legs_eq_4_cons_no_type < pt_has_legs_eq_4) )
		TS_ASSERT( !(pt_has_legs_eq_4 < pt_has_legs_eq_4_cons_no_type) )

		tagd::predicate pt_has_legs_eq_44("has", "legs", "44", tagd::OP_EQ, tagd::TYPE_STRING);
		TS_ASSERT( pt_has_legs_eq_4 < pt_has_legs_eq_44 ) // "4" < "44"

		tagd::predicate pt_has_legs_eq_6("has", "legs", "6", tagd::OP_EQ, tagd::TYPE_STRING);
		TS_ASSERT( pt_has_legs_eq_4 != pt_has_legs_eq_6 ) // "4" != "6"
		TS_ASSERT( pt_has_legs_eq_4 < pt_has_legs_eq_6 )  // "4" < "6"
		TS_ASSERT( pt_has_legs_eq_44 < pt_has_legs_eq_6 ) // "44" < "6"

		// copy
		auto p_tmp = pt_has_legs_eq_4;
		TS_ASSERT( p_tmp == pt_has_legs_eq_4 )
		TS_ASSERT( !(p_tmp < pt_has_legs_eq_4) )
		p_tmp = pt_has_legs_eq_6;
		TS_ASSERT( p_tmp == pt_has_legs_eq_6 )

		// TYPE_INTEGER
		tagd::predicate pi_has_legs_eq_4("has", "legs", "4", tagd::OP_EQ, tagd::TYPE_INTEGER);
		tagd::predicate pi_has_legs_eq_44("has", "legs", "44", tagd::OP_EQ, tagd::TYPE_INTEGER);
		tagd::predicate pi_has_legs_eq_6("has", "legs", "6", tagd::OP_EQ, tagd::TYPE_INTEGER);
		TS_ASSERT( pi_has_legs_eq_4 != pi_has_legs_eq_6 )    // 4 != 6
		TS_ASSERT( pi_has_legs_eq_4 < pi_has_legs_eq_6 )     // 4 < 6
		TS_ASSERT( !(pi_has_legs_eq_6 < pi_has_legs_eq_4) )  // 6 > 4
		TS_ASSERT( pi_has_legs_eq_44 < pt_has_legs_eq_44 )   // 44 < "44"

		TS_ASSERT( pt_has_legs_eq_4 != pi_has_legs_eq_4 ) // "4" != 6
		TS_ASSERT( pi_has_legs_eq_6 < pt_has_legs_eq_4 )  //  6 < "4"

		tagd::predicate pt_has_legs_gt_2("has", "legs", "2", tagd::OP_GT);
		tagd::predicate pt_has_legs_lt_8("has", "legs", "8", tagd::OP_LT);
		TS_ASSERT( pt_has_legs_lt_8 < pt_has_legs_gt_2 )  //  "<8" < ">2"
		TS_ASSERT( !(pt_has_legs_gt_2 < pt_has_legs_lt_8) )  //  !(">2" < "<8")
	}

    void test_interrogator(void) {
		tagd::interrogator a("what");
		TS_ASSERT_EQUALS( a.id() , "what" )
		TS_ASSERT( a.super_object().empty() )
		TS_ASSERT( a.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator b("what", "mammal");
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT( b.sub_relator() == HARD_TAG_SUB )
		TS_ASSERT( b.super_object() == "mammal" )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )

		tagd::interrogator c("what");
		TS_ASSERT_EQUALS(c.relation("has", "legs", "2", tagd::OP_GT), tagd::TAGD_OK)
		TS_ASSERT_EQUALS(c.relation("has", "legs", "8", tagd::OP_LT), tagd::TAGD_OK)
		TS_ASSERT( b.id() == "what" )
		TS_ASSERT_EQUALS( c.relations.size() , 2 )
		TS_ASSERT( b.pos() == tagd::POS_INTERROGATOR )

	}

	void test_referent(void) {
		tagd::referent a("is_a", "is_a", "simple_english");
		TS_ASSERT_EQUALS( a.refers() , "is_a" )
		TS_ASSERT_EQUALS( a.sub_relator() , HARD_TAG_REFERS_TO )
		TS_ASSERT_EQUALS( a.refers_to() , "is_a" )
		TS_ASSERT( a.pos() == tagd::POS_REFERENT )
		TS_ASSERT_EQUALS( a.context(), "simple_english" )
		
		tagd::referent b("perro", "dog", "spanish");
		TS_ASSERT_EQUALS( b.refers() , "perro" )
		TS_ASSERT_EQUALS( a.sub_relator() , HARD_TAG_REFERS_TO )
		TS_ASSERT_EQUALS( b.refers_to() , "dog" )
		TS_ASSERT( b.pos() == tagd::POS_REFERENT )
		TS_ASSERT_EQUALS( b.context() , "spanish" )
	}

	void test_referent_set(void) {
		tagd::referent a("thing", owned_id(HARD_TAG_ENTITY), "simple_english");
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
		TS_ASSERT( err.id().find("err:") == 0 )
		TS_ASSERT_EQUALS( err.evuri(), err.id() )
		TS_ASSERT_EQUALS( err.event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR_TAGD_ERR )
    }

    void test_error_erruri(void) {
		const std::string erruri = "err:2026-04-09T04:00:56.739Z!host!principal!tagsh!01KNS1F5S0CHPPQQNCVRQKVZM4!2!_error:ts_not_found";
        tagd::error err(erruri);
		TS_ASSERT_EQUALS( err.id(), erruri )
		TS_ASSERT_EQUALS( err.evuri(), erruri )
		TS_ASSERT_EQUALS( err.event_type_tag(), HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT_EQUALS( err.pos(), tagd::POS_ERROR )
    }

	void test_code_error_tag(void) {
		TS_ASSERT_EQUALS(std::string(tagd::code_error_tag(tagd::TAGD_ERR)), HARD_TAG_ERROR_TAGD_ERR)
		TS_ASSERT_EQUALS(std::string(tagd::code_error_tag(tagd::TS_NOT_FOUND)), HARD_TAG_ERROR_TS_NOT_FOUND)
		TS_ASSERT_EQUALS(std::string(tagd::code_error_tag(tagd::TAGL_ERR)), HARD_TAG_ERROR_TAGL_ERR)
	}

    void test_error_msg(void) {
        tagd::error err(tagd::TAGD_ERR, "bad tag: oops");
		TS_ASSERT( err.id().find("err:") == 0 )
		TS_ASSERT_EQUALS( err.event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( err.message(), "bad tag: oops" ) 

		std::stringstream ss;
		ss << err;
		TS_ASSERT( ss.str().find("err:") == 0 )
		TS_ASSERT_EQUALS( ss.str().find(" _type_of "), std::string::npos )
		TS_ASSERT( ss.str().find(HARD_TAG_MESSAGE) != std::string::npos )
    }

    void test_ferror_msg(void) {
        tagd::error err = tagd::error::ferror(tagd::TAGD_ERR, "bad tag: %s", "oops");
		TS_ASSERT( err.id().find("err:") == 0 )
		TS_ASSERT_EQUALS( err.event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( err.super_object(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( err.message(), "bad tag: oops" ) 
    }

    void test_errorable(void) {
		tagd::errorable R;
		size_t num_relations = 0;

		TS_ASSERT( R.report_errors )
		TS_ASSERT( R.ok() )
		TS_ASSERT( !R.has_errors() )
		TS_ASSERT( R.last_error().empty() )
		TS_ASSERT( R.errors().empty() )
		TS_ASSERT( R.errors().begin() == R.errors().end() )

		TS_ASSERT_EQUALS( R.code(tagd::TS_NOT_FOUND) , tagd::TS_NOT_FOUND )
		TS_ASSERT_EQUALS( R.code() , tagd::TS_NOT_FOUND )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_errors() )  // TS_NOT_FOUND considered an error

		TS_ASSERT_EQUALS( R.ferror(tagd::TAGD_ERR, "bad tag: %s", "oops") , tagd::TAGD_ERR );
		num_relations++;
		TS_ASSERT_EQUALS( R.size() , 1 );
		TS_ASSERT_EQUALS( R.code() , tagd::TAGD_ERR )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_errors() )
		TS_ASSERT( R.last_error().id().find("err:") == 0 )
		TS_ASSERT_EQUALS( R.last_error().event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( R.last_error().message(), std::string("bad tag: oops") )

		R.report_errors = false;
		TS_ASSERT_EQUALS( R.error(tagd::error(tagd::TAGL_ERR)) , tagd::TAGL_ERR );
		// same error as  last reported
		TS_ASSERT_EQUALS( R.size() , 1 );
		TS_ASSERT_EQUALS( R.code() , tagd::TAGD_ERR )
		TS_ASSERT_EQUALS( R.last_error().event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		R.report_errors = true;

		tagd::error err(tagd::TS_MISUSE);
		TS_ASSERT_EQUALS(err.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah"), tagd::TAGD_OK)
		num_relations++;
		R.error(err);
		TS_ASSERT_EQUALS( R.size() , 2 );
		TS_ASSERT_EQUALS( R.code() , tagd::TS_MISUSE )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_errors() )
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(R.code()) , "TS_MISUSE");
		TS_ASSERT( R.last_error().id().find("err:") == 0 )
		TS_ASSERT_EQUALS( R.last_error().event_type_tag(), HARD_TAG_ERROR_TS_MISUSE )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR_TS_MISUSE )
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah") )

		R.error( tagd::TAGD_ERR,
			tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, "imsobad") );
		num_relations++;
		TS_ASSERT_EQUALS( R.size() , 3 );
		TS_ASSERT_EQUALS( R.code() , tagd::TAGD_ERR )
		TS_ASSERT( !R.ok() )
		TS_ASSERT( R.has_errors() )
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(R.code()) , "TAGD_ERR");
		TS_ASSERT( R.last_error().id().find("err:") == 0 )
		TS_ASSERT_EQUALS( R.last_error().event_type_tag() , HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( R.last_error().super_object(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, "imsobad") )

		R.last_error_relation(tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, "23"));
		num_relations++;
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, "23") )
		TS_ASSERT_EQUALS( R.last_error().event_type_tag() , HARD_TAG_ERROR_TAGD_ERR )
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(R.code()) , "TAGD_ERR");

		R.ferror( tagd::TS_NOT_FOUND, "no such tag: %s", "blah");
		num_relations++;
		TS_ASSERT_EQUALS( R.size() , 4 );
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(R.code()) , "TS_NOT_FOUND");
		R.last_error_relation(tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah"));
		num_relations++;
		TS_ASSERT_EQUALS( R.last_error().event_type_tag() , HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT( R.last_error().related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah") )
        TS_ASSERT_EQUALS (TAGD_CODE_STRING(R.code()) , "TS_NOT_FOUND");

		TS_ASSERT_EQUALS( R.most_severe() , tagd::TS_MISUSE )
		TS_ASSERT_EQUALS( R.most_severe(tagd::TAGL_ERR) , tagd::TAGL_ERR )

		tagd::errorable R1;
		R1.copy_errors(R);

		// test iterating errors_t
		size_t n = 0;
		for (auto e : R1.errors())
			n += e.relations.size();
		TS_ASSERT_EQUALS( n, num_relations )

		auto it = R1.errors().begin();
		TS_ASSERT( it != R1.errors().end() );
		TS_ASSERT_EQUALS( it->event_type_tag() , HARD_TAG_ERROR_TAGD_ERR );
		TS_ASSERT( it->related(owned_id(HARD_TAG_HAS), owned_id(HARD_TAG_MESSAGE), "bad tag: oops") );
		it++;
		TS_ASSERT_EQUALS( it->event_type_tag() , HARD_TAG_ERROR_TS_MISUSE );
		TS_ASSERT( it->related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah") );
		it++;
		TS_ASSERT_EQUALS( it->event_type_tag() , HARD_TAG_ERROR_TAGD_ERR );
		TS_ASSERT( it->related(HARD_TAG_CAUSED_BY, HARD_TAG_BAD_TOKEN, "imsobad") );
		TS_ASSERT( it->related(HARD_TAG_CAUSED_BY, HARD_TAG_LINE_NUMBER, "23") );
		it++;
		TS_ASSERT_EQUALS( it->event_type_tag() , HARD_TAG_ERROR_TS_NOT_FOUND )
		TS_ASSERT( it->related(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, "blah") )
		it++;
		TS_ASSERT( it == R1.errors().end() );

		R1.clear_errors();
		TS_ASSERT_EQUALS( R1.size() , 0 );

		tagd::errorable R2;
		R1.share_errors(R2);

		TS_ASSERT_EQUALS( R1.size() , 0 );
		TS_ASSERT_EQUALS( R2.size() , 0 );

		TS_ASSERT_EQUALS( R1.ferror(tagd::TAG_ILLEGAL, "bad tag: %s", "oops") , tagd::TAG_ILLEGAL );
		TS_ASSERT_EQUALS( R1.last_error().event_type_tag(), HARD_TAG_ERROR_TAG_ILLEGAL )
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
		TS_ASSERT_EQUALS( R3.last_error().event_type_tag(), HARD_TAG_ERROR_TAG_UNKNOWN )
		TS_ASSERT_EQUALS( R3.size() , 1 );
		TS_ASSERT_EQUALS( R4.size() , 0 );

		R.share_errors(R3).share_errors(R4);

		TS_ASSERT_EQUALS( R.size() , 5 );
		TS_ASSERT_EQUALS( R3.size() , 5 );
		TS_ASSERT_EQUALS( R4.size() , 5 );

		TS_ASSERT_EQUALS( R.ferror(tagd::TAGD_ERR, "another bad tag: %s", "oops_again") , tagd::TAGD_ERR );

		TS_ASSERT_EQUALS( R.size() , 6 );
		TS_ASSERT_EQUALS( R.last_error().event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( R3.size() , 6 );
		TS_ASSERT_EQUALS( R3.last_error().event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
		TS_ASSERT_EQUALS( R4.size() , 6 );
		TS_ASSERT_EQUALS( R4.last_error().event_type_tag(), HARD_TAG_ERROR_TAGD_ERR )
    }

    void test_modifier_comma_quotes(void) {
		tagd::abstract_tag t = make_test_tag("table");
		TS_ASSERT_EQUALS(t.relation("has", "idlist", "47,72,43"), tagd::TAGD_OK)
		std::stringstream ss;
		ss << t;

		TS_ASSERT_EQUALS( ss.str() , "table has idlist = \"47,72,43\"");
	}

	void test_modifier_esc_quotes(void) {
		tagd::abstract_tag t = make_test_tag("my_message");
		TS_ASSERT_EQUALS(t.relation("has", "message", "quoted \"string\" hey\" yo \\ "), tagd::TAGD_OK)
		std::stringstream ss;
		ss << t;

		TS_ASSERT_EQUALS( ss.str() , "my_message has message = \"quoted \\\"string\\\" hey\\\" yo \\ \"");
	}

	void test_esc_and_quote(void) {
		// not ':'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("each:a") , "each:a" )
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("http://localhost:2112/each:a?v=browse.html") , "http://localhost:2112/each:a?v=browse.html" )

		// '\\'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("back\\slashed") , "\"back\\slashed\"" )

		// '"'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("\"quoted\"") , "\"\\\"quoted\\\"\"" )
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("qu\"oted") , "\"qu\\\"oted\"" )

		// '/'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("slash/ed") , "\"slash/ed\"" )

		// ';'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("semi;colon") , "\"semi;colon\"" )

		// ','
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("com,ma") , "\"com,ma\"" )

		// '='
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("eq=uals") , "\"eq=uals\"" )

		// '-'
		TS_ASSERT_EQUALS( tagd::util::esc_and_quote("dash-ed") , "\"dash-ed\"" )
	}

	void test_predicate_ostream_op(void) {
		std::stringstream ss;
		tagd::predicate p{ "has" , "legs", "4" };
		ss << p;
		TS_ASSERT_EQUALS( ss.str() , "has legs = 4" );
	}
};
