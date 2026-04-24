#ifndef TAGD_H_INCLUDED
#include "tagd.h"
#else
#ifndef TAGD_EVENT_H_INCLUDED
#define TAGD_EVENT_H_INCLUDED

/*\
|*| Event URI (EVURI)
|*|
|*|   0    1    2         3       4          5        6
|*| time!host!principal!program!session_id!sequence!event_type_tag
|*|
|*| time is UTC/Zulu ISO 8601 with milliseconds:
|*| YYYY-MM-DDTHH:MM:SS.mmmZ
\*/
const std::string EVURI_SCHEME{"ev:"};
const std::string ERRURI_SCHEME{"err:"};
const char EVURI_DELIM = '!';
const std::string EVURI_DELIM_ENCODED{"%21"};
const size_t EVURI_NUM_ELEMS = 7;

std::string encode_evuri_delim(std::string);
std::string decode_evuri_delim(std::string);
std::string system_hostname();
std::string system_principal();

class session;

class event : public abstract_tag {
	private:
		id_string _time;
		id_string _host;
		id_string _principal;
		id_string _program;
		id_string _session_id;
		id_string _sequence;
		id_string _event_type_tag;

		tagd::code init_evuri(const std::string&);
		void init_id();

	public:
		event();
		event(const std::string&);
		event(session&, const id_string&, const id_string&);

		const id_string& time() const { return _time; }
		const id_string& host() const { return _host; }
		const id_string& principal() const { return _principal; }
		const id_string& program() const { return _program; }
		const id_string& session_id() const { return _session_id; }
		const id_string& sequence() const { return _sequence; }
		const id_string& event_type_tag() const { return _event_type_tag; }

		std::string evuri() const { return _id; }
};

class EVURI : public event {
	public:
		EVURI(const std::string& e) : event(e) {}
};

#endif
#endif
