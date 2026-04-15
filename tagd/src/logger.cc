#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>

#include "tagd.h"
#include "tagd/logger.h"
#include "tagd/hard-tags.h"
#include "tagd/event.h"

namespace tagd {

static log_role_validator ROLE_VALIDATOR = nullptr;

const char* log_level_name(log_level lvl) {
	switch (lvl) {
		case log_level::EMERGENCY: return "emergency";
		case log_level::ALERT: return "alert";
		case log_level::CRITICAL: return "critical";
		case log_level::ERROR: return "error";
		case log_level::WARNING: return "warning";
		case log_level::NOTICE: return "notice";
		case log_level::INFO: return "info";
		case log_level::DEBUG: return "debug";
	}

	return "unknown";
}

bool parse_log_level(const std::string& s, log_level& lvl) {
	std::string name{s};
	std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c) { return std::tolower(c); });

	if (name == "emergency") {
		lvl = log_level::EMERGENCY;
	} else if (name == "alert") {
		lvl = log_level::ALERT;
	} else if (name == "critical") {
		lvl = log_level::CRITICAL;
	} else if (name == "error") {
		lvl = log_level::ERROR;
	} else if (name == "warning") {
		lvl = log_level::WARNING;
	} else if (name == "notice") {
		lvl = log_level::NOTICE;
	} else if (name == "info") {
		lvl = log_level::INFO;
	} else if (name == "debug") {
		lvl = log_level::DEBUG;
	} else {
		return false;
	}

	return true;
}

bool valid_log_role(const std::string& role) {
	if (!ROLE_VALIDATOR)
		return false;

	return ROLE_VALIDATOR(role);
}

void set_log_role_validator(log_role_validator validator) {
	ROLE_VALIDATOR = validator;
}

bool parse_log_level_spec(const std::string& s, logger& log) {
	std::stringstream ss{s};
	std::string elem;
	logger parsed = log;

	while (std::getline(ss, elem, ',')) {
		if (elem.empty())
			return false;

		size_t sep = elem.rfind(':');
		log_level lvl;
		if (sep == std::string::npos) {
			if (!parse_log_level(elem, lvl))
				return false;
			parsed.level(lvl);
			continue;
		}

		std::string role = elem.substr(0, sep);
		if (!valid_log_role(role))
			return false;

		if (!parse_log_level(elem.substr(sep + 1), lvl))
			return false;

		parsed.level(role, lvl);
	}

	log = parsed;
	return true;
}

logger::logger() :
	_os(&std::cerr),
	_level(log_level::INFO)
{}

logger::logger(std::ostream& os) :
	_os(&os),
	_level(log_level::INFO)
{}

void logger::stream(std::ostream& os) {
	_os = &os;
}

void logger::stream(const std::string& role, std::ostream& os) {
	_role_streams[role] = &os;
}

std::ostream& logger::stream() const {
	return *_os;
}

std::ostream& logger::stream(const std::string& role) const {
	auto it = _role_streams.find(role);
	if (it == _role_streams.end())
		return *_os;

	return *(it->second);
}

void logger::level(log_level lvl) {
	_level = lvl;
}

void logger::level(const std::string& role, log_level lvl) {
	_role_levels[role] = lvl;
}

log_level logger::level() const {
	return _level;
}

log_level logger::level(const std::string& role) const {
	auto it = _role_levels.find(role);
	if (it == _role_levels.end())
		return _level;

	return it->second;
}

void logger::log(log_level lvl, const std::string& msg) {
	if (static_cast<int>(lvl) > static_cast<int>(_level))
		return;

	this->stream() << msg << std::endl;
}

void logger::log(const std::string& role, log_level lvl, const std::string& msg) {
	if (static_cast<int>(lvl) > static_cast<int>(level(role)))
		return;

	this->stream(role) << msg << std::endl;
}

void logger::log(const std::string& role, log_level lvl, const event& ev) {
	log(role, lvl, ev.evuri());
}

void logger::log(log_level lvl, const event& ev) {
	log(lvl, ev.evuri());
}

void logger::log(log_level lvl, const errorable& e) {
	if (static_cast<int>(lvl) > static_cast<int>(_level))
		return;

	e.print_errors(this->stream());
}

} // namespace tagd
