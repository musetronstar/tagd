#pragma once

#include "tagdb.h"

namespace tagd::filepile {

inline constexpr flags_t canonical_flags = F_NO_TRANSFORM_REFERENTS | F_NO_NOT_FOUND_ERROR;

// The three-digit retained revision suffix bounds allocation; deleted suffixes may be reused.
inline constexpr unsigned last_revision = 999;

struct association {
	id_string link, source, stored;
	unsigned revision = 0;
};
[[nodiscard]] code link_id(errorable&, std::string&, const url&, unsigned revision);
[[nodiscard]] code read_association(errorable&, association&, const abstract_tag&);

struct record_roles {
	bool stored = false;
	bool link = false;
	bool event = false;
};

id_string canonical_id(id_view);
[[nodiscard]] code classify(record_roles&, tagdb::tagdb&, id_view);
void log_event(event&, code result, const std::string& message);
void log_event(session*, id_view type, code result, const std::string& message);

}
