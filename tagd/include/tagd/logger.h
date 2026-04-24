#ifndef TAGD_LOGGER_H_INCLUDED
#define TAGD_LOGGER_H_INCLUDED

#include <iosfwd>
#include <map>
#include <string>

namespace tagd {

/*\
|*| Log levels classify impact, not subsystem or event type.
|*|
|*| EMERGENCY: the system is unusable.
|*| ALERT: operator action is required immediately.
|*| CRITICAL: a major subsystem is broken.
|*| ERROR: the current operation failed.
|*| WARNING: something abnormal happened, but processing continued.
|*| NOTICE: a significant normal state change occurred.
|*| INFO: a routine operational fact is worth reporting.
|*| DEBUG: development and diagnosis detail.
\*/
enum class log_level {
	EMERGENCY = 0,
	ALERT,
	CRITICAL,
	ERROR,
	WARNING,
	NOTICE,
	INFO,
	DEBUG
};

class logger;
typedef bool (*log_role_validator)(const std::string&);

/*\
|*| Log level spec
|*|
|*| Levels follow syslog severity order: lower values are more severe.
|*| A logger emits a message when message_level <= configured_level.
|*|
|*| Grammar:
|*|
|*|   log_level_spec ::= log_level_item ("," log_level_item)*
|*|   log_level_item ::= level | role ":" level
|*|   level          ::= emergency | alert | critical | error
|*|                   | warning | notice | info | debug
|*|   role           ::= hard_tag_role_id
|*|
|*| Examples:
|*|
|*|   warning
|*|   _role:scanner:debug
|*|   warning,_role:scanner:debug,_role:parser:debug,_role:tagdb:emergency
|*|
|*| A bare level sets the default. A role-specific level overrides the default
|*| for that hard tag role id. Role ids are validated by an installed hard tag
|*| lookup adapter; the logger does not own hard tag naming policy.
\*/
const char* log_level_name(log_level);
bool parse_log_level(const std::string&, log_level&);
bool parse_log_level_spec(const std::string&, logger&);
void set_log_role_validator(log_role_validator);
bool valid_log_role(const std::string&);

class event;
class errorable;

class logger {
	private:
		std::ostream *_os;
		log_level _level;
		std::map<std::string, log_level> _role_levels;
		std::map<std::string, std::ostream*> _role_streams;

	public:
		logger();
		explicit logger(std::ostream&);

		void stream(std::ostream&);
		void stream(const std::string&, std::ostream&);
		void stream(std::string_view role, std::ostream& os) { this->stream(std::string(role), os); }
		std::ostream& stream() const;
		std::ostream& stream(const std::string&) const;
		std::ostream& stream(std::string_view role) const { return this->stream(std::string(role)); }

		void level(log_level);
		void level(const std::string&, log_level);
		void level(std::string_view role, log_level lvl) { this->level(std::string(role), lvl); }
		log_level level() const;
		log_level level(const std::string&) const;
		log_level level(std::string_view role) const { return this->level(std::string(role)); }

		void log(log_level, const std::string&);
		void log(const std::string&, log_level, const std::string&);
		void log(std::string_view role, log_level lvl, const std::string& msg) { this->log(std::string(role), lvl, msg); }
		void log(const std::string&, log_level, const event&);
		void log(std::string_view role, log_level lvl, const event& ev) { this->log(std::string(role), lvl, ev); }
		void log(log_level, const event&);
		void log(log_level, const errorable&);
};

} // namespace tagd

#endif
