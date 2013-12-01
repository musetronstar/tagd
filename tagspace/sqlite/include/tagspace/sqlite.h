#pragma once

#include "tagd.h"
#include "tagspace.h"
#include "sqlite3.h"
#include <functional>

namespace tagspace {

typedef sqlite3_int64 rowid_t;

typedef std::function<tagd::id_type(const tagd::id_type&)> id_transform_func_t;

class sqlite: public tagspace {
    protected:
        sqlite3 *_db;   // sqlite connection
        std::string _db_fname;

    private:
        // prepared statement handles, must be sqlite3_finalized in the destructor
        sqlite3_stmt *_get_stmt;
        sqlite3_stmt *_exists_stmt;
        sqlite3_stmt *_term_pos_stmt;
        sqlite3_stmt *_term_id_pos_stmt;
        sqlite3_stmt *_pos_stmt;
        sqlite3_stmt *_refers_to_stmt;
        sqlite3_stmt *_refers_stmt;
        sqlite3_stmt *_insert_term_stmt;
        sqlite3_stmt *_update_term_stmt;
        sqlite3_stmt *_insert_stmt;
        sqlite3_stmt *_update_tag_stmt;
        sqlite3_stmt *_update_ranks_stmt;
        sqlite3_stmt *_child_ranks_stmt;
        sqlite3_stmt *_insert_relations_stmt;
        sqlite3_stmt *_insert_referents_stmt;
        sqlite3_stmt *_insert_context_stmt;
        sqlite3_stmt *_delete_context_stmt;
        sqlite3_stmt *_truncate_context_stmt;
        sqlite3_stmt *_get_relations_stmt;
        sqlite3_stmt *_related_stmt;
        sqlite3_stmt *_related_modifier_stmt;
        sqlite3_stmt *_related_null_super_stmt;
		sqlite3_stmt *_get_children_stmt;
        sqlite3_stmt *_related_null_super_modifier_stmt;
        /*** uridb ***/
        sqlite3_stmt *_get_uri_stmt;
        sqlite3_stmt *_get_uri_specs_stmt;
        sqlite3_stmt *_get_host_stmt;
        sqlite3_stmt *_get_authority_stmt;
        sqlite3_stmt *_get_uri_relations_stmt;
        sqlite3_stmt *_insert_uri_stmt;
        sqlite3_stmt *_insert_uri_specs_stmt;
        sqlite3_stmt *_insert_host_stmt;
        sqlite3_stmt *_insert_authority_stmt;
        sqlite3_stmt *_insert_uri_relations_stmt;

		// performing init() operations
		bool _doing_init;
		bool _trace_on;

		// wrapped by init(), sets _doing_init
        tagd_code _init(const std::string&);

		id_transform_func_t  _f_encode_referent;

    public:
        sqlite() :
			tagspace(),
            _db(NULL),
            _db_fname(),
            _get_stmt(NULL),
            _exists_stmt(NULL),
            _term_pos_stmt(NULL),
            _term_id_pos_stmt(NULL),
            _pos_stmt(NULL),
            _refers_to_stmt(NULL),
            _refers_stmt(NULL),
            _insert_term_stmt(NULL),
            _update_term_stmt(NULL),
            _insert_stmt(NULL),
            _update_tag_stmt(NULL),
            _update_ranks_stmt(NULL),
            _child_ranks_stmt(NULL),
            _insert_relations_stmt(NULL),
			_insert_referents_stmt(NULL),
			_insert_context_stmt(NULL),
			_delete_context_stmt(NULL),
			_truncate_context_stmt(NULL),
            _get_relations_stmt(NULL),
            _related_stmt(NULL),
            _related_modifier_stmt(NULL),
            _related_null_super_stmt(NULL),
			_get_children_stmt(NULL),
            _related_null_super_modifier_stmt(NULL),
            /*** uridb ***/
            _get_uri_stmt(NULL),
            _get_uri_specs_stmt(NULL),
            _get_host_stmt(NULL),
            _get_authority_stmt(NULL),
            _get_uri_relations_stmt(NULL),
            _insert_uri_stmt(NULL),
            _insert_uri_specs_stmt(NULL),
            _insert_host_stmt(NULL),
            _insert_authority_stmt(NULL),
            _insert_uri_relations_stmt(NULL),
			_doing_init(false),
			_trace_on(false)
        {
			_f_encode_referent = [&](const tagd::id_type &from) -> tagd::id_type {
				tagd::id_type to;

				if (!from.empty())
					this->refers(to, from); // to set on success

				// refers will not populate 'to' unless there is a referent
				return (to.empty() ? from : to);
			};
		}

        virtual ~sqlite();

        // init db file
        tagd_code init(const std::string&);

        // idempotent, will only open if not already opened
        tagd_code open();

        // wont fail if already closed
        void close();

		tagd_code push_context(const tagd::id_type& id);
		tagd_code pop_context();
		tagd_code clear_context();

        // get into tag given id
        tagd_code get(tagd::abstract_tag&, const tagd::id_type&, flags_t = flags_t());
        tagd_code get(tagd::url&, const tagd::id_type&, flags_t = flags_t());

        tagd_code exists(const tagd::id_type& id); 

        // put tag, will overrite existing (move + update)
        tagd_code put(const tagd::abstract_tag&, flags_t = flags_t());
        tagd_code put(const tagd::url&, flags_t = flags_t());
        tagd_code put(const tagd::referent&, flags_t = flags_t());

		tagd::part_of_speech term_pos(const tagd::id_type& t) {
			return this->term_pos(t, NULL);
		}
		tagd::part_of_speech pos(const tagd::id_type&, flags_t = flags_t());
		tagd_code refers_to(tagd::id_type&, const tagd::id_type&);
		tagd_code refers(tagd::id_type&, const tagd::id_type&);

        tagd_code related(tagd::tag_set&, const tagd::predicate&, const tagd::id_type&, flags_t = flags_t());
        tagd_code related(tagd::tag_set &T, const tagd::predicate &p, flags_t f = flags_t()) {
			return this->related(T, p, tagd::id_type(), f);
		}
        tagd_code query(tagd::tag_set&, const tagd::interrogator&, flags_t = flags_t());
        tagd_code get_children(tagd::tag_set&, const tagd::id_type&, flags_t = flags_t());
        tagd_code query_referents(tagd::tag_set&, const tagd::interrogator&);

        tagd_code dump(std::ostream& = std::cout);
        tagd_code dump_grid(std::ostream& = std::cout);
        tagd_code dump_terms(std::ostream& = std::cout);

        tagd_code dump_uridb(std::ostream& = std::cout);
        tagd_code dump_uridb_relations(std::ostream& = std::cout);

		void trace_on();
		void trace_off();

    protected:
		// returns the part of speech that was inserted or updated, or a duplicate, POS_UNKNOWN on error
		tagd::part_of_speech put_term(const tagd::id_type&, const tagd::part_of_speech);
		tagd::part_of_speech term_pos(const tagd::id_type&, rowid_t*);
		tagd::part_of_speech term_id_pos(rowid_t, tagd::id_type* = NULL);

        tagd_code insert_term(const tagd::id_type&, const tagd::part_of_speech);
        tagd_code update_term(const tagd::id_type&, const tagd::part_of_speech);

        // insert - new, destination (super of new tag)
        tagd_code insert(const tagd::abstract_tag&, const tagd::abstract_tag&);
        // update - updated, new destination
        tagd_code update(const tagd::abstract_tag&, const tagd::abstract_tag&);

        tagd_code insert_relations(const tagd::abstract_tag&);
		tagd_code insert_referent(const tagd::referent&, const flags_t& = flags_t());

		void encode_referent(tagd::id_type&, const tagd::id_type&);
		void encode_referents(tagd::predicate_set&, const tagd::predicate_set&);
		void encode_referents(tagd::abstract_tag&, const tagd::abstract_tag&);

		void decode_referent(tagd::id_type&, const tagd::id_type&);
		void decode_referents(tagd::predicate_set&, const tagd::predicate_set&);
		void decode_referents(tagd::abstract_tag&, const tagd::abstract_tag&);

		tagd_code insert_context(const tagd::id_type&);
		tagd_code delete_context(const tagd::id_type&);
        tagd_code get_relations(tagd::predicate_set&, const tagd::id_type&, flags_t = flags_t());

        tagd_code next_rank(tagd::rank&, const tagd::abstract_tag&);
        tagd_code child_ranks(tagd::rank_set&, const tagd::id_type&);

        // sqlite3 helper funcs
        tagd_code exec(const char*, const char*label=NULL);
		tagd_code exec_mprintf(const char *, ...);
        tagd_code prepare(sqlite3_stmt**, const char*, const char*label=NULL);
        tagd_code bind_text(sqlite3_stmt**, int, const char*, const char*label=NULL);
        tagd_code bind_int(sqlite3_stmt**, int, int, const char*label=NULL);
        tagd_code bind_rowid(sqlite3_stmt**, int, rowid_t, const char*label=NULL);
        tagd_code bind_null(sqlite3_stmt**, int, const char*label=NULL);
        virtual void finalize();

		// overloaded errorable::ferror
		tagd_code ferror(tagd::code, const char *, ...);

        // init db funcs
		tagd_code create_terms_table();
        tagd_code create_tags_table();
        tagd_code create_relations_table();
        tagd_code create_referents_table();
		tagd_code create_context_stack_table();

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

} // tagspace
