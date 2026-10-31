// Test suites

#include <cxxtest/TestSuite.h>

#include <cctype>
#include <regex>
#include <cerrno>
#include <ctime>
#include <sstream>

#include "tagd.h"
#include "tagd/event.h"
#include "tagd/ulid.h"
#include "tagd/logger.h"
#include "tagd/datetime.h"

namespace {
bool event_clock_controlled = false;
bool event_clock_failed = false;
timespec event_clock_value{};
}

extern "C" int __real_clock_gettime(clockid_t, timespec*);
extern "C" int __wrap_clock_gettime(clockid_t clock, timespec* value) {
	if (clock != CLOCK_REALTIME || !event_clock_controlled)
		return __real_clock_gettime(clock, value);

	if (event_clock_failed) {
		errno = EIO;
		return -1;
	}

	*value = event_clock_value;
	return 0;
}

inline tagd::id_string owned_id(std::string_view sv) {
	return tagd::id_string(sv);
}

class Tester : public CxxTest::TestSuite {
	public:
	void test_evuri_percent_and_delimiter_identity() {
		tagd::session ssn;
		for (const auto value : {"alarm!urgent", "alarm%21urgent", "alarm%urgent"}) {
			tagd::event source(ssn, value, value);
			tagd::event parsed(source.evuri());
			TS_ASSERT_EQUALS(parsed.code(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(parsed.program(), value);
			TS_ASSERT_EQUALS(parsed.super_object(), value);
			TS_ASSERT_EQUALS(parsed.evuri(), source.evuri());
		}
	}

	void tearDown() override {
		event_clock_controlled = false;
		event_clock_failed = false;
	}

	void test_first_error_can_survive_clock_failure() {
		event_clock_controlled = true;
		event_clock_failed = true;
		tagd::error err(tagd::TS_NOT_FOUND, "original missing resource");
		TS_ASSERT_EQUALS(err.code(), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(err.time_code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(err.id().empty());
		TS_ASSERT_EQUALS(err.message(), "original missing resource");
		std::ostringstream output;
		tagd::logger log(output);
		log.log(tagd::log_level::ERROR, err);
		TS_ASSERT_DIFFERS(output.str().find("timestamp unavailable"), std::string::npos);
		TS_ASSERT_DIFFERS(output.str().find("original missing resource"), std::string::npos);
	}

	void test_occurrence_time_preserves_session_sequence_and_identity() {
		event_clock_controlled = true;
		event_clock_value = {100, 1};
		tagd::session ssn;
		const auto started = ssn.started_at();

		for (const auto seconds : {200, 200, 150}) {
			event_clock_value = {seconds, 123};
			tagd::event ev(ssn, "test!program", owned_id(HARD_TAG_EVENT));
			std::string expected;
			TS_ASSERT_EQUALS(tagd::current_datetime(expected), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ev.time(), expected);
			TS_ASSERT_EQUALS(ev.time_code(), tagd::TAGD_OK);
			TS_ASSERT_EQUALS(ev.session_id(), ssn.id());
			TS_ASSERT_EQUALS(ev.sequence(), std::to_string(ssn.sequence()));
			TS_ASSERT_EQUALS(ssn.started_at(), started);

			const auto identity = ev.evuri();
			event_clock_value = {500, 0};
			const auto copy = ev;
			tagd::event parsed(identity);
			TS_ASSERT_EQUALS(copy.evuri(), identity);
			TS_ASSERT_EQUALS(parsed.evuri(), identity);
			TS_ASSERT_EQUALS(parsed.program(), "test!program");
		}
		TS_ASSERT_EQUALS(ssn.sequence(), 3u);
	}

	void test_errors_capture_their_own_times_and_preserve_details() {
		event_clock_controlled = true;
		event_clock_value = {300, 42};
		tagd::error first(tagd::TS_NOT_FOUND, "first missing resource");
		event_clock_value = {400, 43};
		tagd::error second(tagd::TS_MISUSE, "second operation");
		TS_ASSERT_EQUALS(first.time(), "1970-01-01T00:05:00.000000042Z");
		TS_ASSERT_EQUALS(second.time(), "1970-01-01T00:06:40.000000043Z");
		TS_ASSERT_EQUALS(first.session_id(), second.session_id());
		TS_ASSERT(first.id().starts_with("err:"));
		TS_ASSERT_EQUALS(first.code(), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(second.code(), tagd::TS_MISUSE);
		tagd::error parsed(first.id());
		TS_ASSERT_EQUALS(parsed.time(), first.time());
	}

	void test_clock_failure_does_not_fabricate_identity_or_lose_error() {
		tagd::session ssn;
		event_clock_controlled = true;
		event_clock_failed = true;
		tagd::event ev(ssn, "test", owned_id(HARD_TAG_EVENT));
		TS_ASSERT_EQUALS(ev.code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT_EQUALS(ev.time_code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(ev.time().empty());
		TS_ASSERT(ev.evuri().empty());

		tagd::error err(tagd::TS_NOT_FOUND, "original missing resource");
		TS_ASSERT_EQUALS(err.code(), tagd::TS_NOT_FOUND);
		TS_ASSERT_EQUALS(err.time_code(), tagd::TS_INTERNAL_ERR);
		TS_ASSERT(err.id().empty());
		TS_ASSERT_EQUALS(err.message(), "original missing resource");
	}

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
		TS_ASSERT(std::regex_match(ssn.started_at(), std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{9}Z")));
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
