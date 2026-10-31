#include <sstream>
#include <vector>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "tagd/event.h"
#include "tagd/datetime.h"

namespace tagd {

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
	abstract_tag("", HARD_TAG_SUB, HARD_TAG_EVENT, POS_TAG)
{
	init_evuri(evuri);
}

event::event(session& ssn, const id_string& program, const id_string& event_type_tag) :
	abstract_tag("", HARD_TAG_SUB, event_type_tag, POS_TAG),
	_host(system_hostname()),
	_principal(system_principal()),
	_program(program),
	_session_id(ssn.id()),
	_sequence(std::to_string(ssn.next_sequence())),
	_event_type_tag(event_type_tag)
{
	// Failure leaves no fabricated EVURI and must not construct another timed error.
	_time_code = current_datetime(_time);
	if (_time_code != TAGD_OK) {
		code(_time_code);
		return;
	}

	init_id();
}

void event::init_id() {
	// EVURI borrows HDURI field escaping and its delimiter for collation.
	std::stringstream ss;
	ss << EVURI_SCHEME
	   << encode_hduri_delim(_time) << HDURI_DELIM
	   << encode_hduri_delim(_host) << HDURI_DELIM
	   << encode_hduri_delim(_principal) << HDURI_DELIM
	   << encode_hduri_delim(_program) << HDURI_DELIM
	   << encode_hduri_delim(_session_id) << HDURI_DELIM
	   << encode_hduri_delim(_sequence) << HDURI_DELIM
	   << encode_hduri_delim(_event_type_tag);
	_id = ss.str();
	_sub_relator = HARD_TAG_SUB;
	_super_object = _event_type_tag.empty() ? HARD_TAG_EVENT : _event_type_tag;
}

tagd::code event::init_evuri(const std::string& evuri) {
	if (std::string_view(evuri).substr(0, EVURI_SCHEME.size()) != EVURI_SCHEME)
		return code(URI_ERR_SCHEME);

	const auto s = std::string_view(evuri).substr(EVURI_SCHEME.size());
	size_t i = 0;
	std::vector<id_string> elems;
	while (i <= s.size()) {
		size_t j = s.find(HDURI_DELIM, i);
		std::string field;
		if (!decode_hduri_delim(field, s.substr(i, j == std::string::npos ? j : j - i)))
			return code(TS_MISUSE);
		elems.push_back(std::move(field));
		if (j == std::string::npos)
			break;
		i = j + 1;
	}

	if (elems.size() != EVURI_NUM_ELEMS)
		return code(TS_MISUSE);

	_time = elems[0];
	_time_code = TAGD_OK;
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
