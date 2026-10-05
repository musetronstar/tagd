#pragma once

#include "tagd.h"
#include "tagdb.h"
#include "sqlite3.h"
#include <functional>
#include <unordered_map>
#include <utility>

namespace tagdb {

typedef sqlite3_int64 rowid_t;

typedef std::function<tagd::id_string(const tagd::id_string&)> id_transform_func_t;

// each enumerator names one prepared statement; cache key for lazy init
enum class stmt_t {
	GET,
	EXISTS,
	TERM_POS,
	TERM_ID_POS,
	POS,
	REFERS_TO,
	REFERS,
	INSERT_TERM,
	UPDATE_TERM,
	DELETE_TERM,
	INSERT_FTS_TAG,
	UPDATE_FTS_TAG,
	DELETE_FTS_TAG,
	SEARCH,
	INSERT,
	UPDATE_TAG,
	UPDATE_RANKS,
	CHILD_RANKS,
	MAX_CHILD_RANK,
	INSERT_RELATIONS,
	INSERT_REFERENTS,
	DELETE_TAG,
	DELETE_SUBJECT_RELATIONS,
	DELETE_RELATION,
	DELETE_REFERS_TO,
	TERM_POS_OCCURENCE,
	GET_RELATIONS,
	RELATED,
	RELATED_MODIFIER,
	GET_CHILDREN
};

struct stmt_t_hash {
	std::size_t operator()(stmt_t s) const noexcept {
		return std::hash<std::underlying_type_t<stmt_t>>{}(std::to_underlying(s));
	}
};

class sqlite: public tagdb {
	public:
		sqlite() = default;
		// Database handles and cached statements must remain owned by this object.
		sqlite(const sqlite&)            = delete;
		sqlite& operator=(const sqlite&) = delete;
		sqlite(sqlite&&)                 = delete;
		sqlite& operator=(sqlite&&)      = delete;

	protected:
		sqlite3 *_db = nullptr;   // owned here; closed in ~sqlite()
		std::string _db_fname;

	private:
		// performing init() operations
		bool _doing_init = false;
		bool _create_missing = true;

		std::unordered_map<stmt_t, sqlite3_stmt*, stmt_t_hash> _stmts;  // lazy cache — only statements actually used are prepared and held

		// wrapped by init(), sets _doing_init
		tagd::code _init(const std::string&);

		id_transform_func_t  f_encode_referent(session *ssn) {
			return [this, ssn](const tagd::id_string &from) -> tagd::id_string {
				tagd::id_string to;

				/*
				 * This method can return only an ID, so preserve the
				 * original referent unless lookup succeeds in the active context.
				 */
				if (!from.empty() && this->refers(to, from, ssn) == tagd::TAGD_OK)
					return to;

				// refers will not populate 'to' unless there is a referent
				return (to.empty() ? from : to);
			};
		}

	public:
		virtual ~sqlite();

		// init db file
		[[nodiscard]] tagd::code init(const std::string&, bool create_missing = true);

		// idempotent, will only open if not already opened
		[[nodiscard]] tagd::code open();

		// wont fail if already closed
		void close();

		// get into tag given id
		[[nodiscard]] tagd::code get(tagd::abstract_tag&, tagd::id_view, session*, flags_t = 0) override;
		[[nodiscard]] tagd::code get(tagd::url&, tagd::id_view, session*, flags_t = 0) override;

		// put tag, will overrite existing (move + update)
		[[nodiscard]] tagd::code put(const tagd::abstract_tag&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code put(const tagd::url&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code put(const tagd::referent&, session *, flags_t = 0) override;

		// delete tag and/or relations
		[[nodiscard]] tagd::code del(const tagd::abstract_tag&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code del(const tagd::url&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code del(const tagd::referent&, session *, flags_t = 0) override;

// --- tagdb abstraction boundary: methods below are sqlite implementation detail, not part of tagdb::tagdb ---

		tagd::part_of_speech term_pos(tagd::id_view t) {
			return this->term_pos(t, NULL);
		}
		tagd::part_of_speech pos(tagd::id_view, session*, flags_t = 0);
		bool exists(tagd::id_view id, flags_t = 0);

		// get refers_to given refers
		[[nodiscard]] tagd::code refers_to(tagd::id_string&, const tagd::id_string&, session*);
		// get refers given refers_to
		[[nodiscard]] tagd::code refers(tagd::id_string&, const tagd::id_string&, session*);

		[[nodiscard]] tagd::code related(tagd::tag_set&, const tagd::predicate&, const tagd::id_string&, session *, flags_t = 0);
		[[nodiscard]] tagd::code related(tagd::tag_set &T, const tagd::predicate &p, session *ssn, flags_t f = 0) {
			return this->related(T, p, tagd::id_string(), ssn, f);
		}
		[[nodiscard]] tagd::code query(tagd::tag_set&, const tagd::interrogator&, session *, flags_t = 0);

		[[nodiscard]] tagd::code search(tagd::tag_set&, const std::string&, flags_t = 0);
		[[nodiscard]] tagd::code get_children(tagd::tag_set&, const tagd::id_string&, session *, flags_t = 0);
		[[nodiscard]] tagd::code query_referents(tagd::tag_set&, const tagd::interrogator&);

		tagd::code dump(std::ostream& = std::cout);
		tagd::code dump_grid(std::ostream& = std::cout);
		tagd::code dump_terms(std::ostream& = std::cout);
		tagd::code dump_search(std::ostream& = std::cout);

		tagd::code dump_uridb(std::ostream& = std::cout);
		tagd::code dump_uridb_relations(std::ostream& = std::cout);

	protected:
		// returns the part of speech that was inserted or updated, or a duplicate, POS_UNKNOWN on error
		tagd::part_of_speech put_term(const tagd::id_string&, const tagd::part_of_speech);
		tagd::part_of_speech term_pos(tagd::id_view, rowid_t*);
		tagd::part_of_speech term_id_pos(rowid_t, tagd::id_string* = nullptr);

		tagd::code insert_term(const tagd::id_string&, const tagd::part_of_speech);
		tagd::code update_term(const tagd::id_string&, const tagd::part_of_speech);
		tagd::code delete_term(const tagd::id_string&);

		tagd::code insert_fts_tag(const tagd::id_string&, flags_t = 0);
		tagd::code update_fts_tag(const tagd::id_string&, flags_t = 0);
		tagd::code delete_fts_tag(const tagd::id_string&);

		// insert - new, destination (sub of new tag)
		tagd::code insert(const tagd::abstract_tag&, const tagd::abstract_tag&);
		// update - updated, new destination
		tagd::code update(const tagd::abstract_tag&, const tagd::abstract_tag&);

		tagd::code insert_relations(const tagd::abstract_tag&, flags_t = 0);
		tagd::code insert_referent(const tagd::referent&, session *, flags_t = 0);

		void encode_referent(tagd::id_string&, const tagd::id_string&, session*);
		void encode_referents(tagd::predicate_set&, const tagd::predicate_set&, session*);
		tagd::abstract_tag encode_referents(const tagd::abstract_tag&, session*);

		void decode_referent(tagd::id_string&, const tagd::id_string&, session*);
		void decode_referents(tagd::predicate_set&, const tagd::predicate_set&, session*);
		tagd::abstract_tag decode_referents(const tagd::abstract_tag&, session*);
		tagd::interrogator decode_referents(const tagd::interrogator&, session*);

		tagd::code delete_tag(const tagd::id_string&, session*);

		// deletes all relations for given subject
		tagd::code delete_relations(const tagd::id_string&);

		// deletes all relations for given subject and predicates
		tagd::code delete_relations(const tagd::id_string&, const tagd::predicate_set&);

		tagd::code delete_refers_to(const tagd::id_string&);
		tagd::part_of_speech term_pos_occurence(const tagd::id_string&, session*, bool);
		tagd::code update_pos_occurence(const tagd::id_string&);
		tagd::code get_relations(tagd::predicate_set&, const tagd::id_string&, session *, flags_t = 0);

		tagd::code next_rank(tagd::rank&, const tagd::abstract_tag&);
		tagd::code child_ranks(tagd::rank_set&, const tagd::id_string&);
		tagd::code max_child_rank(tagd::rank&, const tagd::id_string&);

		// sqlite3 helper funcs
		tagd::code exec(const char*, const char*label=NULL);
		tagd::code exec_mprintf(const char *, ...);
		tagd::code prepare(stmt_t, const char*, const char*label=NULL);  // prepares on first call; idempotent (reset+rebind) on subsequent calls for same key
		sqlite3_stmt* get_stmt(stmt_t);  // raw pointer accessor for bind/step/column calls after prepare
		tagd::code bind_text(stmt_t, int, const char*, const char*label=NULL);
		tagd::code bind_int(stmt_t, int, int, const char*label=NULL);
		tagd::code bind_rowid(stmt_t, int, rowid_t, const char*label=NULL);
		tagd::code bind_null(stmt_t, int, const char*label=NULL);
		virtual void finalize();  // finalizes all cached statements; called by close() and destructor

	private:
		// init db funcs
		tagd::code create_terms_table();
		tagd::code create_tags_table();
		tagd::code create_relations_table();
		tagd::code create_referents_table();
		tagd::code create_fts_tags_table();

	public:
		// statics
		static const char* sqlite_err_code_str(int code) {
			switch (code) {
				case SQLITE_OK:      return "SQLITE_OK";
				case SQLITE_ERROR:   return "SQLITE_ERROR";
				case SQLITE_INTERNAL:    return "SQLITE_INTERNAL";
				case SQLITE_PERM:    return "SQLITE_PERM";
				case SQLITE_ABORT:   return "SQLITE_ABORT";
				case SQLITE_BUSY:    return "SQLITE_BUSY";
				case SQLITE_LOCKED:  return "SQLITE_LOCKED";
				case SQLITE_NOMEM:   return "SQLITE_NOMEM";
				case SQLITE_READONLY:    return "SQLITE_READONLY";
				case SQLITE_INTERRUPT:   return "SQLITE_INTERRUPT";
				case SQLITE_IOERR:   return "SQLITE_IOERR";
				case SQLITE_CORRUPT: return "SQLITE_CORRUPT";
				case SQLITE_NOTFOUND:    return "SQLITE_NOTFOUND";
				case SQLITE_FULL:    return "SQLITE_FULL";
				case SQLITE_CANTOPEN:    return "SQLITE_CANTOPEN";
				case SQLITE_PROTOCOL:    return "SQLITE_PROTOCOL";
				case SQLITE_EMPTY:   return "SQLITE_EMPTY";
				case SQLITE_SCHEMA:  return "SQLITE_SCHEMA";
				case SQLITE_TOOBIG:  return "SQLITE_TOOBIG";
				case SQLITE_CONSTRAINT:  return "SQLITE_CONSTRAINT";
				case SQLITE_MISMATCH:    return "SQLITE_MISMATCH";
				case SQLITE_MISUSE:  return "SQLITE_MISUSE";
				case SQLITE_NOLFS:   return "SQLITE_NOLFS";
				case SQLITE_AUTH:    return "SQLITE_AUTH";
				case SQLITE_FORMAT:  return "SQLITE_FORMAT";
				case SQLITE_RANGE:   return "SQLITE_RANGE";
				case SQLITE_NOTADB:  return "SQLITE_NOTADB";
				case SQLITE_ROW: return "SQLITE_ROW";
				case SQLITE_DONE:    return "SQLITE_DONE";
				default: return "SQLITE_UNKNOWN";
			}
		}
};

} // tagdb
