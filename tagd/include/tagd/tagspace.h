#pragma once

#include "tagd.h"
#include "tagd/url.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <iostream>
#include <ostream>
#include <utility>

namespace tagd {

// Valid only on TAGD_OK or TS_DUPLICATE; errors leave the outcome unspecified.
enum class replace_outcome { created, replaced, unchanged };


/* These flags control tag operations without changing rank or containment rules. */
typedef enum {
	F_NO_POS_CAST =            1 << 0,
	F_NO_TRANSFORM_REFERENTS = 1 << 1,
	F_NO_NOT_FOUND_ERROR =     1 << 2,
	F_IGNORE_DUPLICATES =      1 << 3,
	F_NO_RESET =               1 << 4,
	// Expand ordinary relations on GET and QUERY results without changing storage.
	F_INCLUDE_INHERITED_PREDICATES = 1 << 5
} ts_flags;

// First unused flag bit; update this when adding a flag.
const int TS_FLAGS_END = 1 << 6;
typedef uint32_t flags_t;

struct flag_util {
	static std::string flag_str(flags_t f) {
		if (f == 0)
			return "EMPTY_FLAGS";

		switch (f) {
			case F_NO_POS_CAST:
				return "F_NO_POS_CAST";
			case F_NO_TRANSFORM_REFERENTS:
				return "F_NO_TRANSFORM_REFERENTS";
			case F_NO_NOT_FOUND_ERROR:
				return "F_NO_NOT_FOUND_ERROR";
			case F_IGNORE_DUPLICATES:
				return "F_IGNORE_DUPLICATES";
			case F_NO_RESET:
				return "F_NO_RESET";
			case F_INCLUDE_INHERITED_PREDICATES:
				return "F_INCLUDE_INHERITED_PREDICATES";
			default:
				return "FLAG_UNKNOWN";
		}
	}

	static std::string flag_list_str(flags_t f) {
		if (f == 0)
			return "EMPTY_FLAGS";

		std::string s;
		int i = 0;
		ts_flags flag = (ts_flags)(1 << i);
		if ((f & flag) == flag)
			s.append(flag_str(flag));
		while ((flag = (ts_flags)(1 << (++i))) < TS_FLAGS_END) {
			if ((f & flag) == flag) {
				if (s.size() > 0)
					s.append(",");
				s.append(flag_str(flag));
			}
		}

		return s;
	}
};

/*\
|*| tagspace_session adds referent context to the core tagd::session.
|*| A tagspace creates it and supplies the function used to check tag membership.
|*| The tagspace must outlive the session, including copies of the session.
|*| The backend receives this same session from the caller.
\*/
class tagspace_session : public session {
	id_vec _context;
	std::function<bool(id_view)> _exists;

public:
	explicit tagspace_session(std::function<bool(id_view)> exists)
		: _exists(std::move(exists)) {}
	// Switching changes membership checks while preserving the caller's context IDs.
	void membership(std::function<bool(id_view)> exists) {
		_exists = std::move(exists);
	}
	tagd::code push_context(id_view);
	// Restore saved contexts or project them for selector lookup before a target is known.
	// User-supplied SET and tagdurl contexts require push_context membership checks.
	void lookup_contexts(const id_vec& ids) { _context = ids; }
	tagd::code pop_context();
	tagd::code clear_context();
	void print_context();
	const id_vec& context() const {
		return _context;
	}
};

/*\
|*| const_tagspace defines how to read tags, ranks, and parts of speech.
|*| It also creates sessions for operations on this tagspace.
|*| The shared containment and ordering methods use the derived class to read
|*| tags, so they work with both stored tags and compile-time hard tags.
|*| Shared-owned instances let sessions retain their lifetime; stack-owned instances
|*| must outlive their sessions. Borrowing an address does not confer ownership.
\*/
class const_tagspace : public tagd::errorable, public std::enable_shared_from_this<const_tagspace> {
public:
	virtual ~const_tagspace() = default;

	[[nodiscard]] virtual tagd::rank lookup_rank(id_view id) const;
	[[nodiscard]] virtual tagd::code lookup_rank(rank&, id_view) const;
	[[nodiscard]] virtual tagd::code order_predicates(predicate_set&) const;
	[[nodiscard]] virtual part_of_speech lookup_pos(id_view id) const;
	[[nodiscard]] virtual bool contains(id_view ancestor, id_view descendant) const;

	[[nodiscard]] virtual std::function<bool(const predicate&, const predicate&)> rank_comparator() const;

	[[nodiscard]] virtual tagd::code get(abstract_tag& t, id_view id, tagspace_session* ssn = nullptr, flags_t flags = 0) = 0;

	[[nodiscard]] virtual tagd::code get(url& u, id_view id, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		return this->get(static_cast<abstract_tag&>(u), id, ssn, flags);
	}

	[[nodiscard]] virtual tagd::code query(tag_set& result, const interrogator& q, tagspace_session* ssn = nullptr, flags_t flags = 0) = 0;

	[[nodiscard]] virtual part_of_speech pos(id_view id, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		(void)ssn;
		(void)flags;
		return this->lookup_pos(id);
	}

	[[nodiscard]] virtual bool exists(id_view id, flags_t flags = 0) const = 0;

	[[nodiscard]] virtual tagd::code dump(std::ostream& os = std::cout) const = 0;
	[[nodiscard]] virtual tagd::code dump_grid(std::ostream& os = std::cout) const {
		(void)os;
		return tagd::TS_NOT_IMPLEMENTED;
	}
	[[nodiscard]] virtual tagd::code dump_terms(std::ostream& os = std::cout) const {
		(void)os;
		return tagd::TS_NOT_IMPLEMENTED;
	}
	[[nodiscard]] virtual tagd::code dump_search(std::ostream& os = std::cout) const {
		(void)os;
		return tagd::TS_NOT_IMPLEMENTED;
	}

	/*
	 * get_session() returns a value for stack use. The caller owns the pointer
	 * returned by new_session() and releases it with release_session().
	 * Collection-owned storage is borrowed by the membership closure, including
	 * session copies. Stack-owned storage must still outlive its sessions.
	 */
	tagspace_session get_session() {
		auto owner = weak_from_this().lock();
		return tagspace_session([this, owner = std::move(owner)](id_view id) {
			return this->exists(id);
		});
	}
	virtual tagspace_session* new_session() {
		return new tagspace_session(get_session());
	}
	virtual void release_session(tagspace_session* ssn) {
		delete ssn;
	}

protected:
	const_tagspace() : tagd::errorable(tagd::TS_INIT) {}

	const_tagspace(const const_tagspace&) = delete;
	const_tagspace& operator=(const const_tagspace&) = delete;
	const_tagspace(const_tagspace&&) = delete;
	const_tagspace& operator=(const_tagspace&&) = delete;
};

enum class store_outcome { none, new_revision, unchanged };

struct store_result {
	url source;
	url stored;
	id_string link;
	unsigned revision = 0;
	store_outcome outcome = store_outcome::none;
	bool changed = false;
	bool commit_unknown = false;
	// Advisory error objects never participate in tagspace/session failure state.
	std::vector<tagd::error> warnings;
};

enum class cleanup_outcome { not_attempted, retained, removed, already_missing, failed };

struct cleanup_result {
	url stored;
	cleanup_outcome outcome = cleanup_outcome::not_attempted;
	tagd::code code = TAGD_OK;
};

struct filepile_delete_result {
	bool committed = false;
	bool commit_unknown = false;
	// Physical results remain meaningful after a committed deletion returns a cleanup error.
	std::vector<cleanup_result> content;
	std::vector<tagd::error> warnings;
};

/*\
|*| tagspace adds put, del, and merge to the read-only tagspace interface.
|*| Applications use this type to work with memory or persistent tagspaces
|*| without needing to know which storage backend they use.
\*/
class tagspace : public const_tagspace {
public:
	class memory;
	class persistent;

	~tagspace() override = default;
	/*
	 * TODO(filepile-integrity): Verification before duplicate reporting remains
	 * a future optimization site; every reused or duplicate-reported object is fully hashed.
	 * F_IGNORE_DUPLICATES must not bypass verification or suppress integrity
	 * errors. See workspace
	 * TASKS.d/2026-10-23-filepile-integrity-and-verification-cache.md.
	 */
	/*
	 * TODO(tagd-io): Filepile initially requires one process and one store at a
	 * time per shared home. Concurrent writers can race revision allocation and
	 * content publication; a broader tagd I/O design must coordinate them.
	 * See workspace TASKS.d/2026-09-28-filepile-storage.md.
	 */
	/*
	 * TODO(filepile-http): Add encapsulated httagd-client HTTP/HTTPS acquisition
	 * behind store without exposing transport details in the declaration interface.
	 * Reuse the client implementation without exposing transport types here.
	 * See workspace TASKS.d/2026-10-23-http-filepile-storage.md.
	 */
	/*
	 * TODO(filepile-permissions): Once roles and permissions exist, restrict
	 * unauthorized operations and provide a superuser repair path for damaged
	 * managed records. Generated fields are protected independently of roles;
	 * descriptive predicates remain editable. Roles and permissions are follow-on
	 * work: workspace TASKS.d/2026-10-23-filepile-link-permissions.md.
	 */

	/*
	 * TODO(filepile-foreign-tagspaces): Only one persistent tagspace participates
	 * in a filepile. Reference checks must cover every participating tagspace before
	 * enabling shared use; see TASKS.d/2026-10-28-foreign-tagspaces.md.
	 */
	[[nodiscard]] virtual tagd::code store(store_result& result, const abstract_tag&,
		tagspace_session* ssn = nullptr, flags_t flags = 0) {
		result = {};
		if (!(flags & F_NO_RESET))
			reset(ssn);
		// TODO(persistent-config): Enable this only when configuration is durably stored.
		const auto rc = error(TS_MISUSE,
			"filepile storage requires a persistent tagspace; the active tagspace is volatile.");
		if (ssn)
			ssn->code(rc);
		return rc;
	}

	[[nodiscard]] virtual tagd::code delete_storage(filepile_delete_result& result,
		const abstract_tag&, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		result = {};
		if (!(flags & F_NO_RESET))
			reset(ssn);
		const auto rc = error(TS_NOT_IMPLEMENTED, "managed DELETE is not supported by this tagspace");
		if (ssn)
			ssn->code(rc);
		return rc;
	}

	virtual size_t merge(tag_set& A, const tag_set& B) {
		return merge_containing_tags(A, B);
	}

	[[nodiscard]] virtual tagd::code replace(replace_outcome&, const abstract_tag&,
		tagspace_session* = nullptr, flags_t = 0) {
		return error(TS_NOT_IMPLEMENTED, "replace is not supported by this tagspace");
	}

	[[nodiscard]] virtual tagd::code put(const abstract_tag& t, tagspace_session* ssn = nullptr, flags_t flags = 0) = 0;

	[[nodiscard]] virtual tagd::code put(const url& u, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		return this->put(static_cast<const abstract_tag&>(u), ssn, flags);
	}

	[[nodiscard]] virtual tagd::code put(const referent& r, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		return this->put(static_cast<const abstract_tag&>(r), ssn, flags);
	}

	[[nodiscard]] virtual tagd::code del(const abstract_tag& t, tagspace_session* ssn = nullptr, flags_t flags = 0) = 0;

	[[nodiscard]] virtual tagd::code del(const url& u, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		return this->del(static_cast<const abstract_tag&>(u), ssn, flags);
	}

	[[nodiscard]] virtual tagd::code del(const referent& r, tagspace_session* ssn = nullptr, flags_t flags = 0) {
		return this->del(static_cast<const abstract_tag&>(r), ssn, flags);
	}

protected:
	void reset(tagspace_session* ssn) {
		// A new operation resets status without erasing shared diagnostic history.
		_code = TAGD_OK;
		if (ssn)
			ssn->code(TAGD_OK);
	}

	tagspace() = default;
	tagspace(const tagspace&) = delete;
	tagspace& operator=(const tagspace&) = delete;
	tagspace(tagspace&&) = delete;
	tagspace& operator=(tagspace&&) = delete;
};

} // namespace tagd
