#pragma once

#include "tagd.h"
#include "tagspace.h"
#include "sqlite3.h"

namespace tagspace {

typedef sqlite3_int64 rowid_t;

class sqlite: public tagspace {
    protected:
        sqlite3 *_db;   // sqlite connection
        std::string _db_fname;

    private:
        // prepared statement handles, must be sqlite3_finalized in the destructor
        sqlite3_stmt *_get_stmt;
        sqlite3_stmt *_exists_stmt;
        sqlite3_stmt *_pos_stmt;
        sqlite3_stmt *_insert_stmt;
        sqlite3_stmt *_update_tag_stmt;
        sqlite3_stmt *_update_ranks_stmt;
        sqlite3_stmt *_child_ranks_stmt;
        sqlite3_stmt *_insert_relations_stmt;
        sqlite3_stmt *_get_relations_stmt;
        sqlite3_stmt *_related_stmt;
        sqlite3_stmt *_related_modifier_stmt;
        sqlite3_stmt *_related_null_super_stmt;
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

		// wrapped by init(), sets _doing_init
        tagd_code _init(const std::string&);

    public:
        sqlite() :
			tagspace(),
            _db(NULL),
            _db_fname(),
            _get_stmt(NULL),
            _exists_stmt(NULL),
            _pos_stmt(NULL),
            _insert_stmt(NULL),
            _update_tag_stmt(NULL),
            _update_ranks_stmt(NULL),
            _child_ranks_stmt(NULL),
            _insert_relations_stmt(NULL),
            _get_relations_stmt(NULL),
            _related_stmt(NULL),
            _related_modifier_stmt(NULL),
            _related_null_super_stmt(NULL),
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
			_doing_init(false)
        {}

        virtual ~sqlite();

        // init db file
        tagd_code init(const std::string&);

        // idempotent, will only open if not already opened
        tagd_code open();

        // wont fail if already closed
        void close();

        // get into tag given id
        tagd_code get(tagd::abstract_tag&, const tagd::id_type&);
        tagd_code get(tagd::url&, const tagd::id_type&);

        tagd_code exists(const tagd::id_type& id); 

        // put tag, will overrite existing (move + update)
        tagd_code put(const tagd::abstract_tag&);
        tagd_code put(const tagd::url&);

		tagd::part_of_speech pos(const tagd::id_type&);

        tagd_code related(tagd::tag_set&, const tagd::predicate&, const tagd::id_type& = "_entity");
        tagd_code query(tagd::tag_set&, const tagd::interrogator&);

        tagd_code dump(std::ostream& = std::cout);
        tagd_code dump_grid(std::ostream& = std::cout);

        tagd_code dump_uridb(std::ostream& = std::cout);
        tagd_code dump_uridb_relations(std::ostream& = std::cout);

		void trace_on();
		void trace_off();

    protected:
        // insert - new, destination (super of new tag)
        tagd_code insert(const tagd::abstract_tag&, const tagd::abstract_tag&);
        // update - updated, new destination
        tagd_code update(const tagd::abstract_tag&, const tagd::abstract_tag&);

        tagd_code insert_relations(const tagd::abstract_tag&);
        tagd_code get_relations(tagd::predicate_set&, const tagd::id_type&);

        tagd_code next_rank(tagd::rank&, const tagd::abstract_tag&);
        tagd_code child_ranks(tagd::rank_set&, const tagd::id_type&);

        // sqlite3 helper funcs
        tagd_code exec(const char*, const char*label=NULL);
        tagd_code prepare(sqlite3_stmt**, const char*, const char*label=NULL);
        tagd_code bind_text(sqlite3_stmt**, int, const char*, const char*label=NULL);
        tagd_code bind_int(sqlite3_stmt**, int, int, const char*label=NULL);
        tagd_code bind_rowid(sqlite3_stmt**, int, rowid_t, const char*label=NULL);
        tagd_code bind_null(sqlite3_stmt**, int, const char*label=NULL);
        virtual void finalize();

        // init db funcs
        tagd_code create_tags_table();
        tagd_code create_relations_table();

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
                default: return "UNKNOWN";
            }
        }
};

} // tagspace
