#include <sstream>
#include <vector>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "tagd/event.h"

namespace tagd {

std::string encode_evuri_delim(std::string elem) {
	size_t pos = 0;
	while ((pos = elem.find(EVURI_DELIM, pos)) != std::string::npos) {
		elem.replace(pos, 1, EVURI_DELIM_ENCODED);
		pos += EVURI_DELIM_ENCODED.length();
	}
	return elem;
}

std::string decode_evuri_delim(std::string elem) {
	size_t pos = 0;
	while ((pos = elem.find(EVURI_DELIM_ENCODED, pos)) != std::string::npos) {
		elem.replace(pos, EVURI_DELIM_ENCODED.length(), 1, EVURI_DELIM);
		pos++;
	}
	return elem;
}

std::string system_hostname() {
	char buf[256];
	if (gethostname(buf, sizeof(buf)) != 0)
		return std::string();
	buf[sizeof(buf)-1] = '\0';
	return std::string(buf);
}

std::string system_principal() {
	uid_t uid = geteuid();
	struct passwd pwd;
	struct passwd *result = nullptr;
	char buf[16384];
	if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &result) == 0 && result != nullptr)
		return std::string(result->pw_name);
	return std::to_string(uid);
}

event::event() :
	abstract_tag("", POS_TAG)
{}

event::event(const std::string& evuri) :
	abstract_tag("", HARD_TAG_TYPE_OF, HARD_TAG_EVENT, POS_TAG)
{
	init_evuri(evuri);
}

event::event(session& ssn, const id_string& program, const id_string& event_type_tag) :
	abstract_tag("", HARD_TAG_TYPE_OF, event_type_tag, POS_TAG),
	_time(ssn.started_at()),
	_host(system_hostname()),
	_principal(system_principal()),
	_program(program),
	_session_id(ssn.id()),
	_sequence(std::to_string(ssn.next_sequence())),
	_event_type_tag(event_type_tag)
{
	init_id();
}

void event::init_id() {
	std::stringstream ss;
	ss << EVURI_SCHEME
	   << encode_evuri_delim(_time) << EVURI_DELIM
	   << encode_evuri_delim(_host) << EVURI_DELIM
	   << encode_evuri_delim(_principal) << EVURI_DELIM
	   << encode_evuri_delim(_program) << EVURI_DELIM
	   << encode_evuri_delim(_session_id) << EVURI_DELIM
	   << encode_evuri_delim(_sequence) << EVURI_DELIM
	   << encode_evuri_delim(_event_type_tag);
	_id = ss.str();
	_sub_relator = HARD_TAG_TYPE_OF;
	_super_object = _event_type_tag.empty() ? HARD_TAG_EVENT : _event_type_tag;
}

tagd::code event::init_evuri(const std::string& evuri) {
	if (evuri.substr(0, EVURI_SCHEME.size()) != EVURI_SCHEME)
		return code(URI_ERR_SCHEME);

	const std::string& s = evuri.substr(EVURI_SCHEME.size());
	size_t i = 0;
	std::vector<id_string> elems;
	while (i <= s.size()) {
		size_t j = s.find(EVURI_DELIM, i);
		if (j == std::string::npos) {
			elems.push_back(decode_evuri_delim(s.substr(i)));
			break;
		}
		elems.push_back(decode_evuri_delim(s.substr(i, j - i)));
		i = j + 1;
	}

	if (elems.size() != EVURI_NUM_ELEMS)
		return code(TS_MISUSE);

	_time = elems[0];
	_host = elems[1];
	_principal = elems[2];
	_program = elems[3];
	_session_id = elems[4];
	_sequence = elems[5];
	_event_type_tag = elems[6];
	init_id();
	return code(TAGD_OK);
}

} // namespace tagd
