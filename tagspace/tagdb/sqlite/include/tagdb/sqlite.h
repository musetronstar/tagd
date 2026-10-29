#pragma once

#include "tagd.h"
#include "tagdb.h"
#include "sqlite3.h"
#include "tagdb/canonical-cache.h"
#include "tagdb/read-snapshot.h"
#include "tagdb/relation-resolver.h"
#include <functional>
#include <unordered_map>
#include <utility>
#include <chrono>
#include <list>
#include <map>
#include <optional>

namespace tagdb {

static_assert(std::is_same_v<rowid_t, sqlite3_int64>);

typedef std::function<tagd::id_string(const tagd::id_string&)> id_transform_func_t;

// each enumerator names one prepared statement; cache key for lazy init
enum class stmt_t {
	SCHEMA_TABLES,
	STORAGE_VERSION,
	TYPE_DEPENDENCIES,
	DEPENDENTS,
	INHERITS,
	SUBTREE,
	CYCLE_CHECK,
	REPARENT_ROLES,
	RELATION_CANDIDATES_ALL,
	RELATION_CANDIDATES_RELATOR,
	RELATION_CANDIDATES_OBJECT,
	RELATION_CANDIDATES_PAIR,
	VALUED_CANDIDATES_ALL,
	VALUED_CANDIDATES_RELATOR,
	VALUED_CANDIDATES_OBJECT,
	VALUED_CANDIDATES_PAIR,
	GET_TYPE_RELATIONS,
	INSERT_TYPE_RELATION,
	DELETE_TYPE_RELATION,
	DATA_VERSION,
	READ_BEGIN,
	READ_PIN,
	READ_ROLLBACK,
	GET,
	RANK,
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
	CLEAR_MODIFIER,
	DELETE_REFERS_TO,
	TERM_POS_OCCURENCE,
	GET_RELATIONS,
	GET_INHERITED_RELATIONS,
	RELATED,
	RELATED_MODIFIER,
	GET_CHILDREN
};

struct stmt_t_hash {
	std::size_t operator()(stmt_t s) const noexcept {
		return std::hash<std::underlying_type_t<stmt_t>>{}(std::to_underlying(s));
	}
};

class sqlite: public tagdb, private snapshot_source {
	public:
		explicit sqlite(relation_semantics semantics = relation_semantics::assertions)
			: _relation_semantics(semantics) {
		}
		// Database handles and cached statements must remain owned by this object.
		sqlite(const sqlite&)            = delete;
		sqlite& operator=(const sqlite&) = delete;
		sqlite(sqlite&&)                 = delete;
		sqlite& operator=(sqlite&&)      = delete;

	protected:
		sqlite3 *_db = nullptr;   // owned here; closed in ~sqlite()
		std::string _db_fname;

	private:
		// Storage compatibility includes persisted hard-tag ranks as well as table definitions.
		static constexpr int SCHEMA_VERSION = 3;

		using cache_clock = canonical_cache::cache_clock;
		using canonical_identity = canonical_cache::canonical_identity;
		using canonical_predicate = canonical_cache::canonical_predicate;
		canonical_cache _canonical;

		unsigned _read_depth = 0, _mutation_depth = 0;
		bool _read_transaction = false, _cacheable = false;
		int _cache_data_version = -1;
		sqlite3_int64 _cache_changes = 0;

		// Context-specific substitutions are temporary and never enter canonical cache entries.
		tagd::id_vec _operation_refers_context;
		std::map<tagd::id_string, std::optional<tagd::id_string>, std::less<>> _operation_refers;

		// Detect commits from other connections; -1 means the version could not be read.
		int data_version();

		// Reuse SQL for starting, pinning, or releasing a read snapshot.
		// Each statement key must always identify the same SQL.
		tagd::code snapshot_statement(stmt_t, const char*);

		// Invalidate retained and per-operation data together so no stale lookup survives.
		void clear_cache();

		tagd::code load_identity(canonical_identity&, tagd::rank&, const tagd::id_string&);
		tagd::code load_predicates(std::vector<canonical_predicate>&,
			const tagd::id_string&, session*, flags_t);

		tagd::code begin_read() override;

		// Only the outermost read releases the snapshot; cleanup errors stay on the backend.
		void end_read() override;


		class mutation_scope {
			// The borrowed backend must outlive this guard for invalidation after failed writes too.
			sqlite& _owner;

		public:
			explicit mutation_scope(sqlite& owner);
			~mutation_scope();
			mutation_scope(const mutation_scope&) = delete;
			mutation_scope& operator=(const mutation_scope&) = delete;
		};

		// performing init() operations
		bool _write_poisoned = false;
		bool _checking_put = false;
		bool _doing_init = false;
		bool _create_missing = true;

		std::unordered_map<stmt_t, sqlite3_stmt*, stmt_t_hash> _stmts;  // lazy cache — only statements actually used are prepared and held

		const relation_semantics _relation_semantics;
		[[nodiscard]] tagd::code load_relation_declarations(relation_resolver&,
			const std::set<tagd::id_string>&, const std::set<tagd::id_string>& rank_ids = {});
		[[nodiscard]] tagd::code format_declaration(tagd::abstract_tag&, const tagd::abstract_tag&,
			session*, flags_t);
		[[nodiscard]] tagd::code collect_dependents(std::set<tagd::id_string>&, const tagd::id_string&);
		[[nodiscard]] tagd::code collect_subtree(std::set<tagd::id_string>&, const tagd::id_string&);
		[[nodiscard]] tagd::code relation_candidates(std::set<tagd::id_string>&, const tagd::predicate&);
		[[nodiscard]] tagd::code validate_reparented_roles(const tagd::id_string&);
		[[nodiscard]] tagd::code expand_relations(tagd::abstract_tag&, const tagd::id_string&,
			const relation_resolver&, session*, flags_t);
		[[nodiscard]] tagd::code query_effective(tagd::tag_set&, const tagd::interrogator&, session*, flags_t);
		[[nodiscard]] tagd::code validate_effective(const std::set<tagd::id_string>&);
		static void tag_affected(std::set<tagd::id_string>&, const tagd::abstract_tag&);
		[[nodiscard]] tagd::code check_inherited_deletion(const tagd::abstract_tag&, const tagd::abstract_tag&);
		[[nodiscard]] tagd::code put_impl(const tagd::abstract_tag&, session*, flags_t);
		[[nodiscard]] tagd::code put_transaction(const tagd::abstract_tag&, session*, flags_t,
			std::set<tagd::id_string>* batch_affected);
		[[nodiscard]] tagd::code get_type_relations(tagd::type_predicate_set&, const tagd::id_string&);
		[[nodiscard]] tagd::code insert_type_relations(const tagd::abstract_tag&, size_t&);
		[[nodiscard]] tagd::code delete_type_relations(const tagd::abstract_tag&);
		[[nodiscard]] tagd::code validate_declaration(const tagd::abstract_tag&);
		[[nodiscard]] tagd::code validate_dependencies(const tagd::abstract_tag&);
		[[nodiscard]] tagd::code check_storage_schema();
		[[nodiscard]] tagd::code type_dependency_errors(const tagd::id_string&, session*);

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
		using cache_metrics = canonical_cache::cache_metrics;
		cache_metrics cache_status() const override;
		void configure_cache(size_t bytes, size_t entries,
			std::chrono::steady_clock::duration idle,
			std::function<std::chrono::steady_clock::time_point()> now) override;
		virtual ~sqlite();
		[[nodiscard]] tagd::code lookup_rank(tagd::rank&, tagd::id_view) override;
		[[nodiscard]] tagd::code inherited_sources(std::set<tagd::id_string>&,
			tagd::id_view, std::span<const tagd::id_view>) override;
		[[nodiscard]] tagd::code order_predicates(tagd::predicate_set&) override;

		// init db file
		[[nodiscard]] tagd::code init(const std::string&, bool create_missing = true);

		// idempotent, will only open if not already opened
		[[nodiscard]] tagd::code open();

		// wont fail if already closed
		[[nodiscard]] tagd::code open_named(const std::string&, const std::string&,
			open_mode) override;
		void close() override;
		[[nodiscard]] tagd::code open(const storage_location&) override;
		[[nodiscard]] tagd::code resolve_referents(tagd::abstract_tag&,
			const tagd::abstract_tag&, session*) override;

		// get into tag given id
		[[nodiscard]] tagd::code get(tagd::abstract_tag&, tagd::id_view, session*, flags_t = 0) override;
		[[nodiscard]] tagd::code get(tagd::url&, tagd::id_view, session*, flags_t = 0) override;

		bool supports_atomic_put() const override { return true; }
		bool checking_put() const { return _checking_put; }
		[[nodiscard]] tagd::code check_put(const tagd::abstract_tag&, flags_t) override;
		bool commit_unknown() const override { return _write_poisoned; }
		[[nodiscard]] tagd::code prepare_delete() override;
		[[nodiscard]] tagd::code del_batch(std::span<const tagd::id_string>, session*) override;
		[[nodiscard]] tagd::code prepare_store() override;
		[[nodiscard]] tagd::code put_batch(std::span<const tagd::abstract_tag* const>,
			session*, flags_t = 0) override;

		// put tag, will overrite existing (move + update)
		[[nodiscard]] tagd::code put(const tagd::abstract_tag&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code put(const tagd::url&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code put(const tagd::referent&, session *, flags_t = 0) override;

		// delete tag and/or relations
		[[nodiscard]] tagd::code del(const tagd::abstract_tag&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code del(const tagd::url&, session *, flags_t = 0) override;
		[[nodiscard]] tagd::code del(const tagd::referent&, session *, flags_t = 0) override;

// Backend helpers alongside the remaining storage-interface overrides.

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
