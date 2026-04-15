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
		id_type _time;
		id_type _host;
		id_type _principal;
		id_type _program;
		id_type _session_id;
		id_type _sequence;
		id_type _event_type_tag;

		tagd::code init_evuri(const std::string&);
		void init_id();

	public:
		event();
		event(const std::string&);
		event(session&, const id_type&, const id_type&);

		const id_type& time() const { return _time; }
		const id_type& host() const { return _host; }
		const id_type& principal() const { return _principal; }
		const id_type& program() const { return _program; }
		const id_type& session_id() const { return _session_id; }
		const id_type& sequence() const { return _sequence; }
		const id_type& event_type_tag() const { return _event_type_tag; }

		std::string evuri() const { return _id; }
};

class EVURI : public event {
	public:
		EVURI(const std::string& e) : event(e) {}
};

#endif
#endif
