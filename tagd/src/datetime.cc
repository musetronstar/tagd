#include "tagd/datetime.h"
#include <chrono>
#include <cstdio>
#include <ctime>

namespace tagd {

code format_datetime(std::string& output, int64_t seconds, int32_t nanoseconds) {
	using namespace std::chrono;
	output.clear();
	constexpr int64_t first_second = -62135596800LL;
	constexpr int64_t last_second = 253402300799LL;
	constexpr int32_t nanos_per_second = 1000000000;
	if (seconds < first_second || seconds > last_second
		|| nanoseconds < 0 || nanoseconds >= nanos_per_second) {
		return TS_ERR;
	}

	const sys_seconds instant{std::chrono::seconds{seconds}};
	const auto day = floor<days>(instant);
	const year_month_day date{day};
	const hh_mm_ss time{instant - day};
	char buffer[40];
	// Numeric conversion is locale independent; the range check guarantees four year digits.
	std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02lld:%02lld:%02lld.%09dZ",
		int(date.year()), unsigned(date.month()), unsigned(date.day()),
		static_cast<long long>(time.hours().count()),
		static_cast<long long>(time.minutes().count()),
		static_cast<long long>(time.seconds().count()), nanoseconds);
	output = buffer;
	return TAGD_OK;
}

code current_datetime(std::string& output) {
	timespec now{};
	output.clear();
	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		return TS_INTERNAL_ERR;
	}

	return format_datetime(output, now.tv_sec, now.tv_nsec);
}

}
