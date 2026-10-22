#pragma once

#include "tagd.h"

namespace tagdb {

/*
 * Backend hooks pin a consistent read view before enabling canonical-cache reuse.
 * Nested begins share that view; every begin, including failure, receives an end.
 * The final end releases owned resources and invalidates changes since validation.
 * Cleanup failures must disable reuse and prevent writes into an abandoned view.
 */
class snapshot_source {
public:
	virtual ~snapshot_source() = default;
	virtual tagd::code begin_read() = 0;
	virtual void end_read() = 0;
};

// The backend outlives its scopes; cleanup errors are recorded on the backend.
class read_scope {
	snapshot_source& _owner;

public:
	const tagd::code status;
	explicit read_scope(snapshot_source& owner) : _owner(owner), status(owner.begin_read()) {
	}
	~read_scope() {
		_owner.end_read();
	}
	read_scope(const read_scope&) = delete;
	read_scope& operator=(const read_scope&) = delete;
};

} // namespace tagdb
