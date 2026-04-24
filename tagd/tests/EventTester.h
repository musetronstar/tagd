// Test suites

#include <cxxtest/TestSuite.h>

#include <cctype>
#include <regex>

#include "tagd.h"
#include "tagd/event.h"
#include "tagd/ulid.h"

inline tagd::id_string owned_id(std::string_view sv) {
	return tagd::id_string(sv);
}

class Tester : public CxxTest::TestSuite {
	public:

	void test_ulid_generate(void) {
		ulid_generator g;
		ulid_generator_init(&g, 0);

		char ulid[27];
		unsigned char decoded[16];
		ulid_generate(&g, ulid);
		TS_ASSERT_EQUALS(std::string(ulid).size(), (size_t)26);
		TS_ASSERT_EQUALS(ulid_decode(decoded, ulid), 0);
	}

	void test_ulid_generator_monotonic(void) {
		ulid_generator g;
		ulid_generator_init(&g, 0);

		char a[27];
		char b[27];
		unsigned char decoded[16];
		ulid_generate(&g, a);
		ulid_generate(&g, b);

		TS_ASSERT_EQUALS(ulid_decode(decoded, a), 0);
		TS_ASSERT_EQUALS(ulid_decode(decoded, b), 0);
		TS_ASSERT(std::string(a) < std::string(b));
	}

	void test_session_sequence(void) {
		tagd::session ssn;
		TS_ASSERT_DIFFERS(ssn.id().find("ssn_"), (size_t)0);
		unsigned char decoded[16];
		TS_ASSERT_EQUALS(ssn.id().size(), (size_t)26);
		TS_ASSERT_EQUALS(ulid_decode(decoded, ssn.id().c_str()), 0);
		TS_ASSERT(std::regex_match(ssn.started_at(), std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z")));
		TS_ASSERT_EQUALS(ssn.sequence(), (uint64_t)0);
		TS_ASSERT_EQUALS(ssn.next_sequence(), (uint64_t)1);
		TS_ASSERT_EQUALS(ssn.next_sequence(), (uint64_t)2);
	}

	void test_session_factory(void) {
		tagd::session_factory factory;
		tagd::session a = factory.create();
		tagd::session b = factory.create();

		TS_ASSERT_DIFFERS(a.id(), b.id());
		TS_ASSERT_EQUALS(a.sequence(), (uint64_t)0);
		TS_ASSERT_EQUALS(b.sequence(), (uint64_t)0);
		TS_ASSERT_EQUALS(a.next_sequence(), (uint64_t)1);
		TS_ASSERT_EQUALS(b.next_sequence(), (uint64_t)1);
	}

	void test_evuri_round_trip(void) {
		const std::string evuri = "ev:2026-04-09T04:00:56.738Z!host!principal!tagsh!01HV6Y9QW7J9K3QF7M8Z2N4P6R!1!_event";
		tagd::event ev(evuri);

		TS_ASSERT(ev.ok());
		TS_ASSERT_EQUALS(ev.id(), evuri);
		TS_ASSERT_EQUALS(ev.evuri(), evuri);
		TS_ASSERT_EQUALS(ev.time(), "2026-04-09T04:00:56.738Z");
		TS_ASSERT_EQUALS(ev.host(), "host");
		TS_ASSERT_EQUALS(ev.principal(), "principal");
		TS_ASSERT_EQUALS(ev.program(), "tagsh");
		TS_ASSERT_EQUALS(ev.session_id(), "01HV6Y9QW7J9K3QF7M8Z2N4P6R");
		TS_ASSERT_EQUALS(ev.sequence(), "1");
		TS_ASSERT_EQUALS(ev.event_type_tag(), "_event");

		tagd::EVURI rt(ev.evuri());
		TS_ASSERT(rt.ok());
		TS_ASSERT_EQUALS(rt.evuri(), ev.evuri());
	}

	void test_evuri_delim_encoding(void) {
		const std::string evuri = "ev:2026-04-09T04:00:56.738Z!host%21name!principal%21name!tagsh!01HV6Y9QW7J9K3QF7M8Z2N4P6R!1!custom%21event";
		tagd::event ev(evuri);

		TS_ASSERT(ev.ok());
		TS_ASSERT_EQUALS(ev.host(), "host!name");
		TS_ASSERT_EQUALS(ev.principal(), "principal!name");
		TS_ASSERT_EQUALS(ev.event_type_tag(), "custom!event");
		TS_ASSERT_EQUALS(ev.evuri(), evuri);
	}

	void test_event_from_session(void) {
		tagd::session ssn;
		tagd::event ev(ssn, "tagsh", owned_id(HARD_TAG_EVENT));

		TS_ASSERT(ev.ok());
		TS_ASSERT_EQUALS(ev.id(), ev.evuri());
		TS_ASSERT_EQUALS(ev.program(), "tagsh");
		TS_ASSERT(!ev.principal().empty());
		TS_ASSERT_EQUALS(ev.session_id(), ssn.id());
		TS_ASSERT_EQUALS(ev.sequence(), "1");
		TS_ASSERT_EQUALS(ssn.sequence(), (uint64_t)1);
		TS_ASSERT_EQUALS(ev.event_type_tag(), HARD_TAG_EVENT);
		TS_ASSERT(ev.evuri().find("ev:") == 0);
	}
};
