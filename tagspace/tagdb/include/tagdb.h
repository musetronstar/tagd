#pragma once

#include <cassert>
#include <span>
#include <sstream>
#include <stdint.h>
#include "tagd.h"
#include "tagdb/storage.h"
#include "tagdb/canonical-cache.h"
#include "tagd/tagspace.h"
#include "tagd/hard-tags.h"
#include "tagd/logger.h"

void TAGDB_SET_LOGGER(tagd::logger *);
bool TAGDB_LOG_ENABLED(const std::string&, tagd::log_level);
void TAGDB_LOG(const std::string&, tagd::log_level, const std::string&);
void TAGDB_LOG_EVENT(tagd::session *, tagd::log_level, const std::string&);

#define TAGDB_LOG_DEBUG(MSG) \
	do { \
		if (TAGDB_LOG_ENABLED(std::string(HARD_TAG_ROLE_TAGDB), tagd::log_level::DEBUG)) { \
			std::ostringstream _tagdb_debug_os; \
			_tagdb_debug_os << MSG; \
			TAGDB_LOG(std::string(HARD_TAG_ROLE_TAGDB), tagd::log_level::DEBUG, _tagdb_debug_os.str()); \
		} \
	} while (0)

namespace tagdb {

// The backend uses the same session and flags as the public tagspace.
using session = tagd::tagspace_session;
using tagd::flags_t;
using tagd::ts_flags;
using tagd::TS_FLAGS_END;
using tagd::flag_util;
using tagd::F_NO_POS_CAST;
using tagd::F_NO_TRANSFORM_REFERENTS;
using tagd::F_NO_NOT_FOUND_ERROR;
using tagd::F_IGNORE_DUPLICATES;
using tagd::F_NO_RESET;
using tagd::F_INCLUDE_INHERITED_PREDICATES;

// Internal hard-tag table identifiers; these are not persistent backend row addresses.
typedef long long int rowid_t;

class hard_tag {
	public:
		static tagd::part_of_speech pos(tagd::id_view id);
		static tagd::code get(tagd::abstract_tag&, tagd::id_view id);
		static tagd::part_of_speech term_pos(tagd::id_view, rowid_t* = nullptr);
			static tagd::part_of_speech term_id_pos(rowid_t, tagd::id_string* = nullptr);
			static void install_logger_validator();

			static const char ** rows();
		static size_t rows_end();
};

// Abstract storage interface; shared defaults do not depend on a concrete backend.
class tagdb : public tagd::errorable {
	protected:
		// rest tagdb and session to OK state
		void reset(session *ssn) {
			_code = tagd::TAGD_OK;
			if (ssn) ssn->code(tagd::TAGD_OK);
		}

	public:
		tagdb() : tagd::errorable(tagd::TS_INIT) {}
		virtual ~tagdb() {}

		[[nodiscard]] virtual tagd::code open(const storage_location&) = 0;
		[[nodiscard]] virtual tagd::code open_named(const std::string& name,
			const std::string& home, open_mode) = 0;
		virtual void close() = 0;
		// Backend diagnostics and test controls; tagspace callers need not manage caching.
		using cache_metrics = canonical_cache::cache_metrics;
		virtual cache_metrics cache_status() const = 0;
		virtual void configure_cache(size_t bytes, size_t entries,
			canonical_cache::cache_clock::duration idle,
			std::function<canonical_cache::cache_clock::time_point()> now) = 0;
		[[nodiscard]] virtual tagd::code lookup_rank(tagd::rank&, tagd::id_view) = 0;
		// Return the requested sources reached through structural and type edges, including self.
		// Unknown IDs do not match; one call observes one backend snapshot.
		[[nodiscard]] virtual tagd::code inherited_sources(std::set<tagd::id_string>&,
			tagd::id_view, std::span<const tagd::id_view>) {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend cannot check the integrated order");
		}
		[[nodiscard]] tagd::code inherits(bool& result, tagd::id_view subject, tagd::id_view source) {
			std::set<tagd::id_string> matches;
			const auto rc = inherited_sources(matches, subject, {&source, 1});
			result = rc == tagd::TAGD_OK && !matches.empty();
			return rc;
		}
		[[nodiscard]] virtual tagd::code order_predicates(tagd::predicate_set&) = 0;
		[[nodiscard]] virtual tagd::code resolve_referents(tagd::abstract_tag&,
			const tagd::abstract_tag&, session*) = 0;

		void attach_errors(tagd::errorable& owner) {
			// Rebinding must leave lists still shared by other callers intact.
			_errors.reset();
			owner.share_errors(*this);
		}

		// Open database handles have one owner; copying or moving this object is unsafe.
		tagdb(const tagdb&)            = delete;
		tagdb& operator=(const tagdb&) = delete;
		tagdb(tagdb&&)                 = delete;
		tagdb& operator=(tagdb&&)      = delete;

		// The caller supplies a session created by its tagspace.

		/*
		 * when implemented, the follow methods should begin with a
		 * call to this->reset() that respect the F_NO_RESET flag
		 * such as:  if (!(flags & F_NO_RESET)) this->reset();
		 */

		// get into tag from db, given id
		[[nodiscard]] virtual tagd::code get(tagd::abstract_tag&, tagd::id_view, session*, flags_t = 0) = 0;

		/*
		 * Type-specific get overloads; default delegates to abstract_tag get().
		 * Backends with type-specific storage (such as a separate URL representation) override these.
		 */
		[[nodiscard]] virtual tagd::code get(tagd::url& u, tagd::id_view id, session* ssn, flags_t f = 0) {
			return this->get(static_cast<tagd::abstract_tag&>(u), id, ssn, f);
		}

		virtual bool supports_atomic_put() const { return false; }
		// Validate ordinary PUT semantics against current state without committing.
		[[nodiscard]] virtual tagd::code check_put(const tagd::abstract_tag&, flags_t) {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend cannot validate PUT without mutation");
		}
		[[nodiscard]] virtual tagd::code prepare_store() {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend cannot provide durable atomic PUT");
		}
		// Borrowed tags keep their dynamic type and must outlive this synchronous operation.
		[[nodiscard]] virtual tagd::code put_batch(
			std::span<const tagd::abstract_tag* const>, session*, flags_t = 0) {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend does not support atomic PUT");
		}

		virtual bool commit_unknown() const { return false; }
		[[nodiscard]] virtual tagd::code prepare_delete() {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend cannot prepare managed DELETE");
		}
		[[nodiscard]] virtual tagd::code del_batch(std::span<const tagd::id_string>, session*) {
			return error(tagd::TS_NOT_IMPLEMENTED, "backend does not support atomic DELETE");
		}

		// put into db given tag
		[[nodiscard]] virtual tagd::code put(const tagd::abstract_tag&, session*, flags_t = 0) = 0;

		// Type-specific put overloads; default delegates to abstract_tag put().
		[[nodiscard]] virtual tagd::code put(const tagd::url& u, session* ssn, flags_t f = 0) {
			return this->put(static_cast<const tagd::abstract_tag&>(u), ssn, f);
		}
		[[nodiscard]] virtual tagd::code put(const tagd::referent& r, session* ssn, flags_t f = 0) {
			return this->put(static_cast<const tagd::abstract_tag&>(r), ssn, f);
		}

		// delete from db given tag
		[[nodiscard]] virtual tagd::code del(const tagd::abstract_tag&, session*, flags_t = 0) = 0;

		// Type-specific del overloads; default delegates to abstract_tag del().
		[[nodiscard]] virtual tagd::code del(const tagd::url& u, session* ssn, flags_t f = 0) {
			return this->del(static_cast<const tagd::abstract_tag&>(u), ssn, f);
		}
		[[nodiscard]] virtual tagd::code del(const tagd::referent& r, session* ssn, flags_t f = 0) {
			return this->del(static_cast<const tagd::abstract_tag&>(r), ssn, f);
		}

		// query db given interrogator, populate set of tag ids
		[[nodiscard]] virtual tagd::code query(tagd::tag_set&, const tagd::interrogator&, session*, flags_t = 0) = 0;

		// return a tag::pos given a tag id
		virtual tagd::part_of_speech pos(tagd::id_view, session*, flags_t = 0) = 0;

		// returns whether a tag id exists
		virtual bool exists(tagd::id_view, flags_t = 0) = 0;

		virtual tagd::code dump(std::ostream& os = std::cout) = 0;
		virtual tagd::code dump_grid(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
		// Diagnostics describe vocabulary/POS and searchable content, not backend tables.
		virtual tagd::code dump_terms(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
		virtual tagd::code dump_search(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
};

struct util {
	// users default db
	static std::string user_db();
};

} // namespace tagdb
