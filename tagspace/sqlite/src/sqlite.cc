#include <iostream>
#include <sstream>
#include <iomanip> // setw
#include <cstring> // strcmp
#include <assert.h>

#include "tagspace/bootstrap.h"
#include "tagspace/sqlite.h"

void trace_callback( void* udp, const char* sql ) {
	if (udp == NULL) {
		dout << "SQL trace: " << sql << std::endl;
	} else {
		dout << "SQL trace(udp " << udp << "): " << sql << std::endl;
	}
}

// TODO
// profile_callback
// void *sqlite3_profile(sqlite3*, void(*xProfile)(void*,const char*,sqlite3_uint64), void*);

namespace tagspace {

sqlite::~sqlite() {
    this->close();
}

void sqlite::trace_on() {
	sqlite3_trace(_db, trace_callback, NULL);
    dout << "### sqlite trace on ###" << std::endl;
}

void sqlite::trace_off() {
	sqlite3_trace(_db, NULL, NULL);
    dout << "### sqlite trace off ###" << std::endl;
}

ts_res_code sqlite::init(const std::string& fname) {
    dout << "sqlite::init => " << fname << std::endl;

    if (fname.empty()) {
        std::cerr << "init: fname empty";
        return TS_ERR;
    }

    _db_fname = fname;

    this->close();  // won't fail if not open
    ts_res_code ts_rc = this->open(); // open will create db if not exists
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->exec("BEGIN");
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->create_tags_table();

    if (ts_rc == TS_OK)
        ts_rc = this->create_relations_table();

    if (ts_rc == TS_OK)
        ts_rc = bootstrap::init_hard_tags(*this);

    if (ts_rc != TS_OK) {
        this->exec("ROLLBACK");
        return TS_INTERNAL_ERR;
    }

    ts_rc = this->exec("COMMIT");

    return ts_rc;
}

ts_res_code sqlite::create_tags_table() {
    // check db
    sqlite3_stmt *stmt = NULL; 
    ts_res_code ts_rc = this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE tags%'",
        "tags table exists"
    );
    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR) {
        this->print_err("check tags table error");
        return TS_INTERNAL_ERR;
    }
    
    // table exists
    if (s_rc == SQLITE_ROW) {
        tagd::tag t;
        ts_rc = this->get(t, "_entity");
        if (ts_rc == TS_INTERNAL_ERR) return ts_rc;

        if (t.id() == "_entity" && t.super() == "_entity")
            return TS_OK; // db already initialized
    
        std::cerr << "tag table exists but has no _entity row; database corrupt" << std::endl;
        return TS_INTERNAL_ERR;
    }

    //create db

    ts_rc = this->exec(
    "CREATE TABLE tags ( "
        "tag     TEXT PRIMARY KEY NOT NULL, "
        "super   TEXT NOT NULL, "
        // binary collation defeats LIKE wildcard partial indexes,
        // but enables indexing for GLOB style patterns
        "rank    TEXT UNIQUE COLLATE BINARY, "
        // TODO pos should probably be removed here and instead be determined
        // for a tag according to the pos its superordinate hard tag (which could be hard coded)
        "pos     INTEGER NOT NULL, "
        "FOREIGN KEY(super) REFERENCES tags(tag), "
        // _entity is the only self referential tag (_entity = _entity)
        // and the only tag allowed to have rank IS NULL (indicated the root)
        "CHECK ( "
            "(tag  = '_entity' AND super = '_entity' AND rank IS NULL AND pos = 0) OR "
            "(tag <> '_entity' AND rank IS NOT NULL) "
        ")"  // TODO  check rank <= super.rank
    ")" 
    );

    if (ts_rc == TS_OK)
        ts_rc = this->exec("CREATE INDEX idx_pos ON tags(pos)");

    if (ts_rc == TS_OK) {
        ts_rc = this->exec(
            "INSERT INTO tags (tag, super, rank, pos) "
            "VALUES ('_entity', '_entity', NULL, 0)"
        );
    }

    return ts_rc;
}

ts_res_code sqlite::create_relations_table() {
    // check db
    sqlite3_stmt *stmt = NULL; 
    ts_res_code ts_rc = this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE relations%'",
        "relations table exists"
    );
    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR) {
        this->print_err("check relations table error");
        return TS_INTERNAL_ERR;
    }
    
    // table exists
    if (s_rc == SQLITE_ROW)
        return TS_OK;

    //create db
    ts_rc = this->exec(
    "CREATE TABLE relations ("
        "subject   TEXT NOT NULL, "
        "relator   TEXT NOT NULL, "
        "object    TEXT NOT NULL, "
        "modifier  TEXT, "
        // whatever statement throws a unique constraint will fail
        // but the trasaction overall can still succeed
        "PRIMARY KEY (subject, object) ON CONFLICT FAIL, " // makes unique
        "FOREIGN KEY (subject) REFERENCES tags(tag), "
        "FOREIGN KEY (relator) REFERENCES tags(tag), "
        "FOREIGN KEY (object) REFERENCES tags(tag)"
    ")" 
    );

    if (ts_rc == TS_OK)
        ts_rc = this->exec("CREATE INDEX idx_subject ON relations(subject)");

    if (ts_rc == TS_OK)
        ts_rc = this->exec("CREATE INDEX idx_object ON relations(object)");

    return ts_rc;
}


ts_res_code sqlite::open() {
    if (_db != NULL)
        return TS_OK;

    int rc = sqlite3_open(_db_fname.c_str(), &_db);
    if( rc != SQLITE_OK ){
        this->print_err("open database error");
        this->close();
        return TS_INTERNAL_ERR;
    }

    ts_res_code ts_rc = this->exec("PRAGMA foreign_keys = ON");
    if (ts_rc != TS_OK) return ts_rc;

    return TS_OK;
}

void sqlite::close() {
    if (_db == NULL)
        return;

    this->finalize();
    sqlite3_close(_db);
    _db = NULL;
}

ts_res_code sqlite::get(tagd::abstract_tag& t, const tagd::id_type& id) {
    if (id.length() > tagd::MAX_TAG_LEN) {
        dout << "id exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    ts_res_code ts_rc = this->prepare(&_get_stmt,
        "SELECT tag, super, pos, rank "
        "FROM tags WHERE tag = ?",
        "select tags"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_get_stmt, 1, id.c_str(), "get statement id");
    if (ts_rc != TS_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER = 1;
    const int F_POS = 2;
    const int F_RANK = 3;

    int s_rc = sqlite3_step(_get_stmt);
    if (s_rc == SQLITE_ROW) {
        t.id( (const char*) sqlite3_column_text(_get_stmt, F_ID) );
        t.super( (const char*) sqlite3_column_text(_get_stmt, F_SUPER) );
        t.pos( (tagd::part_of_speech) sqlite3_column_int(_get_stmt, F_POS) );
        t.rank( sqlite3_column_text(_get_stmt, F_RANK) );

        ts_rc = this->get_relations(t.relations, id);
        if (!(ts_rc == TS_OK || ts_rc == TS_NOT_FOUND)) return ts_rc;

        return TS_OK;
    } else if (s_rc == SQLITE_ERROR) {
        this->print_err("get failed");
        return TS_ERR;
    } else {  // TODO check other error codes
        return TS_NOT_FOUND;
    }
}

ts_res_code sqlite::get(tagd::url& u, const tagd::id_type& id) {
	ts_res_code ts_rc = this->get((tagd::abstract_tag&)u, id);
	if (ts_rc != TS_OK) return ts_rc;

	tagd::url_code u_rc = u.init_hduri(id);
	if (u_rc != tagd::URL_OK) return TS_ERR; // TODO implement errcode(), errstr()

	return TS_OK;
}

ts_res_code sqlite::get_relations(tagd::predicate_set& P, const tagd::id_type& id) {

    ts_res_code ts_rc = this->prepare(&_get_relations_stmt,
        "SELECT relator, object, modifier "
        "FROM relations WHERE subject = ?",
        "select relations"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_get_relations_stmt, 1, id.c_str(), "get subject");
    if (ts_rc != TS_OK) return ts_rc;

    const int F_RELATOR = 0;
    const int F_OBJECT = 1;
    const int F_MODIFIER = 2;

    int s_rc;
    while ((s_rc = sqlite3_step(_get_relations_stmt)) == SQLITE_ROW) {
        if (sqlite3_column_type(_get_relations_stmt, F_MODIFIER) != SQLITE_NULL) {
            P.insert(tagd::make_predicate(
                (const char*) sqlite3_column_text(_get_relations_stmt, F_RELATOR),
                (const char*) sqlite3_column_text(_get_relations_stmt, F_OBJECT),
                (const char*) sqlite3_column_text(_get_relations_stmt, F_MODIFIER)
            ));
        } else {
            P.insert(tagd::make_predicate(
                (const char*) sqlite3_column_text(_get_relations_stmt, F_RELATOR),
                (const char*) sqlite3_column_text(_get_relations_stmt, F_OBJECT)
            ));
        }
    } 

    if (s_rc == SQLITE_ERROR) {
        this->print_err("get relations failed");
        return TS_ERR;
    } 


    if (P.empty()) return TS_NOT_FOUND;

    return TS_OK;
}

ts_res_code sqlite::exists(const tagd::id_type& id) {
    if (id.length() > tagd::MAX_TAG_LEN) {
        dout << "id exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    ts_res_code ts_rc = this->prepare(&_exists_stmt,
        "SELECT 1 FROM tags WHERE tag = ?",
        "tag exists"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_exists_stmt, 1, id.c_str(), "exists statement id");
    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = sqlite3_step(_exists_stmt);
    if (s_rc == SQLITE_ROW) {
        return TS_OK;
    } else if (s_rc == SQLITE_ERROR) {
        this->print_err("exists failed");
        return TS_ERR;
    } else {  // TODO check other error codes
        return TS_NOT_FOUND;
    }
}

// TS_NOT_FOUND returned if destination undefined
ts_res_code sqlite::put(tagd::abstract_tag& t) {
    if (t.id().length() > tagd::MAX_TAG_LEN) {
        dout << "tag exceeds MAX_TAG_LEN of " << tagd::MAX_TAG_LEN << std::endl;
        return TS_ERR_MAX_TAG_LEN;;
    }

    tagd::abstract_tag existing;
    ts_res_code existing_rc = this->get(existing, t.id());

    if (existing_rc != TS_OK && existing_rc != TS_NOT_FOUND)
        return existing_rc; // err

    if (t.super().empty()) {
        if (existing_rc == TS_NOT_FOUND) {
            // can't do anything without knowing what a tag is
            return TS_SUPER_UNK;
        } else {
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return TS_DUPLICATE;
            else // insert relations
                return this->insert_relations(t);
        }
    }

    tagd::abstract_tag destination;
    ts_res_code dest_rc = this->get(destination, t.super());

    if (dest_rc != TS_OK) {
        if (dest_rc == TS_NOT_FOUND)
            return TS_SUPER_UNK;
        else
            return dest_rc; // err
    }

    // handle duplicate tags up-front
    ts_res_code ins_upd_rc;
    if (existing_rc == TS_OK) {  // existing tag
        if (t.super() == existing.super()) {  // same location
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return TS_DUPLICATE;
            else // insert relations
                return this->insert_relations(t);
        }
        // move existing to new location
        ins_upd_rc = this->update(t, destination);
    } else if (existing_rc == TS_NOT_FOUND) {
        // new tag
        ins_upd_rc = this->insert(t, destination);
    } else {
        assert(false); // should never get here
        return TS_INTERNAL_ERR;
    }

    if (ins_upd_rc == TS_OK && !t.relations.empty())
        return this->insert_relations(t);

    // res from insert/update
    return ins_upd_rc;
}

ts_res_code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& super) {
    assert( !super.rank().empty() || (super.rank().empty() && super.id() == "_entity") );

    tagd::rank_set R;
    ts_res_code ts_rc = child_ranks(R, super.id());
    if (ts_rc != TS_OK)
        return ts_rc;

    if (R.empty()) { // create first child element
        next = super.rank();
        next.push(1);
        return TS_OK;
    }

    tagd::rank_code r_rc = tagd::rank::next(next, R);
    if (r_rc != tagd::RANK_OK) {
        std::cerr << "next_rank error: "
                  << tagd::rank_code_str(r_rc) << std::endl;
        return TS_ERR;
    }

    return TS_OK;
}

ts_res_code sqlite::insert(tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
    assert( t.super() == destination.id() );
    assert( !t.id().empty() );
    assert( !t.super().empty() );

    tagd::rank rank;
    ts_res_code ts_rc = next_rank(rank, destination);
    if (ts_rc != TS_OK)
        return ts_rc;

    dout << "insert next rank: (" << rank.dotted_str() << ")" << std::endl;

    ts_rc = this->prepare(&_insert_stmt,
        "INSERT INTO tags (tag, super, rank, pos) VALUES (?, ?, ?, ?)",
        "insert tag"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_insert_stmt, 1, t.id().c_str(), "insert id");
    if (ts_rc == TS_OK)
        ts_rc = this->bind_text(&_insert_stmt, 2, t.super().c_str(), "insert super");

    if (ts_rc == TS_OK)
        ts_rc = this->bind_text(&_insert_stmt, 3, rank.c_str(), "insert rank");

    if (ts_rc == TS_OK)
        ts_rc = this->bind_int(&_insert_stmt, 4, t.pos(), "insert pos");

    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = sqlite3_step(_insert_stmt);
    if (s_rc != SQLITE_DONE) { 
        this->print_err("insert tags failed");
        return TS_ERR;
    }

	// tag mutated (why it can't be const)
    t.rank(rank);

    return TS_OK;
}

// update existing with new tag
ts_res_code sqlite::update(tagd::abstract_tag& t,
                                      const tagd::abstract_tag& destination) {
    assert( !t.id().empty() );
    assert( !t.super().empty() );
    assert( !destination.super().empty() );

    tagd::rank rank;
    ts_res_code ts_rc = next_rank(rank, destination);
    if (ts_rc != TS_OK)
        return ts_rc;

    dout << "update next rank: (" << rank.dotted_str() << ")" << std::endl;

    // TODO look into the impact of using a TRANSACTION

    // update the ranks
    ts_rc = this->prepare(&_update_ranks_stmt, 
        "UPDATE tags "
        "SET rank = (? || substr(rank, ?)) "
        "WHERE rank GLOB (? || '*')",
        "update ranks"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_update_ranks_stmt, 1, rank.c_str(), "new rank");
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_int(&_update_ranks_stmt, 2, (t.rank().size()+1), "append rank");
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_update_ranks_stmt, 3, t.rank().c_str(), "sub rank");
    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = sqlite3_step(_update_ranks_stmt);
    if (s_rc != SQLITE_DONE) { 
        this->print_err("update ranks failed");
        return TS_ERR;
    }

    //update tag
    ts_rc = this->prepare(&_update_tag_stmt, 
            "UPDATE tags SET super = ? WHERE tag = ?",
            "update tag"
    );
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_update_tag_stmt, 1, destination.id().c_str(), "update super");
    if (ts_rc != TS_OK) return ts_rc;

    ts_rc = this->bind_text(&_update_tag_stmt, 2, t.id().c_str(), "update tag");
    if (ts_rc != TS_OK) return ts_rc;

    s_rc = sqlite3_step(_update_tag_stmt);
    if (s_rc != SQLITE_DONE) { 
        this->print_err("update ranks failed");
        return TS_ERR;
    }

    t.rank(rank);

    return TS_OK;
}

ts_res_code sqlite::insert_relations(const tagd::abstract_tag& t) {
    assert( !t.id().empty() );
    assert( !t.relations.empty() );

    if (t.relations.empty()) return TS_MISUSE;

    ts_res_code ts_rc = this->prepare(&_insert_relations_stmt,
        "INSERT INTO relations (subject, relator, object, modifier) "
        "VALUES (?, ?, ?, ?)",
        "insert relations"
    );
    if (ts_rc != TS_OK) return ts_rc;

    tagd::predicate_set::iterator it = t.relations.begin();
    if (it == t.relations.end()) {  // should never happen
        std::cerr << "insert empty relations" << std::endl;
        return TS_ERR;
    }

    // if all inserts were unique constraint violations, TS_DUPLICATE will be returned
    size_t num_inserted = 0;

    while (true) {
        ts_rc = this->bind_text(&_insert_relations_stmt, 1, t.id().c_str(), "insert subject");

        if (ts_rc == TS_OK)
            ts_rc = this->bind_text(&_insert_relations_stmt, 2, it->relator.c_str(), "insert relator");

        if (ts_rc == TS_OK)
            ts_rc = this->bind_text(&_insert_relations_stmt, 3, it->object.c_str(), "insert object");

        if (ts_rc == TS_OK) {
            if (it->modifier.empty())
                ts_rc = this->bind_null(&_insert_relations_stmt, 4, "insert NULL modifier");
            else
                ts_rc = this->bind_text(&_insert_relations_stmt, 4,
                    it->modifier.c_str(), "insert modifier");
        }

        if (ts_rc != TS_OK) return ts_rc;

        int s_rc = sqlite3_step(_insert_relations_stmt);

        if (s_rc == SQLITE_DONE) {
            num_inserted++;
        } else if (s_rc == SQLITE_CONSTRAINT) {
            // sqlite does't tell us whether the object or relator caused the violation
            ts_sqlite_code ts_sql_rc = sqlite_constraint_type(sqlite3_errmsg(_db));
            if (ts_sql_rc == TS_SQLITE_UNIQUE) { // object UNIQUE violation
                ts_rc = this->exists(it->relator);
                switch ( ts_rc ) {
                    case TS_NOT_FOUND:  return TS_RELATOR_UNK;
                    case TS_OK:         break; // TODO update the relation
                    default:            return ts_rc;
                }
            } else if (ts_sql_rc == TS_SQLITE_FK) {
                // relations.object FK tags.tag violated
                ts_rc = this->exists(it->object);
                switch ( ts_rc ) {
                    case TS_NOT_FOUND:  return TS_OBJECT_UNK;
                    case TS_OK:         return TS_RELATOR_UNK;
                    default:            return ts_rc;
                }
            } else {  // TS_SQLITE_UNK
                return TS_INTERNAL_ERR;
            } 
        } else {
            std::stringstream ss;
            ss << "insert relations failed ("
                << t.id() << "," << it->relator << "," << it->object << ")";
            this->print_err(ss.str().c_str());
            return TS_ERR;
        }

        if (++it != t.relations.end()) {
            sqlite3_reset(_insert_relations_stmt);
            sqlite3_clear_bindings(_insert_relations_stmt);
        } else {
            break;
        }
    }

    if (num_inserted == 0)
        return TS_DUPLICATE;
    else
        return TS_OK;
}

/*

ts_res_code update(const tagd::abstract_tag& t, const tagd::abstract_tag& e) {  // new tag, existing
    if (t.id().length() > tagd::MAX_TAG_LEN) {
        dout << "tag exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;;
    }

    if (t.is_a().length() > tagd::MAX_TAG_LEN) {
        dout << "is_a exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;;
    }

    if (t.is_a() == e.is_a()) {
        dout << "already defined" << std::endl; 
        return TS_UNCHANGED;
    }

    tagdb_sqlite_query rank_query(
    "SELECT CONCAT( "
      "rank, ':', "
      "( "
        "SELECT CONVERT( "
          "SUBSTRING_INDEX( "
            "IFNULL( "
              "( "
                "SELECT rank "
                "FROM tags AS child "
                "WHERE child.is_a = ? "
                "ORDER BY CHAR_LENGTH(child.rank) DESC, child.rank DESC LIMIT 1 "
              "), "
              "CONCAT( "
                "( "
                  "SELECT rank "
                  "FROM tags AS single_child "
                  "WHERE single_child.tag = ? "
                "), ':0' "
              ") "
            "), ':', -1 "
          "), UNSIGNED "
        ") "
      ") + 1 "
    ") AS next_rank "
    "FROM tags AS parent "
    "WHERE parent.tag = ?"
    );
    rank_query.p(t.is_a());
    rank_query.p(t.is_a());
    rank_query.p(t.is_a());

    ts_res_code r = _db.query(rank_query);

    if (r != TS_OK) {
        std::cerr << "next_rank failed:" << std::endl << rank_query.str() << std::endl
                  << "error code:" << rank_query.error_code() << std::endl
                  << "error:" << rank_query.error_msg() << std::endl;
        return r;
    }

    char **row = rank_query.row_next();
    if (row == NULL) {
        std::cerr << "next_rank not found:" << std::endl << rank_query.str() << std::endl
                  << "error code:" << rank_query.error_code() << std::endl
                  << "error:" << rank_query.error_msg() << std::endl;
        return TS_NOT_FOUND;
    }

    tagd::rank_type next_rank = row[0];
    int root_end = e.rank().length() + 1;

    tagdb_sqlite_query update_rank_query(
    "UPDATE tags "
    "SET rank = CONCAT(?, SUBSTRING(rank, ?)) "
    "WHERE rank LIKE CONCAT(?, '%')"
    );
    update_rank_query.p(next_rank);
    update_rank_query.p(root_end);
    update_rank_query.p(e.rank());

    r = _db.query(update_rank_query);

    if (r != TS_OK) {
        std::cerr << "update rank query failed:" << std::endl << rank_query.str() << std::endl
                  << "error code:" << rank_query.error_code() << std::endl
                  << "error:" << rank_query.error_msg() << std::endl;
        return r;
    }

    tagdb_sqlite_query update_is_a_query(
    "UPDATE tags "
    "SET is_a = ? "
    "WHERE tag = ?"
    );
    update_is_a_query.p(t.is_a());
    update_is_a_query.p(t.id());

    r = _db.query(update_is_a_query);

    if (r != TS_OK) {
        dout << "update is_a query failed" << std::endl;
        return r;
    }

    return r;
}

ts_res_code statement(const tagd::statement& s) {
    if (s.subject.length() > tagd::MAX_TAG_LEN) {
        dout << "subject exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    if (s.relation.length() > tagd::MAX_TAG_LEN) {
        dout << "relation exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    if (s.object.length() > tagd::MAX_TAG_LEN) {
        dout << "object exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    if (s.modifier.length() > tagd::MAX_TAG_LEN) {
        dout << "modifier exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    tagdb_sqlite_query q("INSERT INTO relations VALUES(?,?,?,?)");
    q.p(s.subject).p(s.relation).p(s.object).p(s.modifier);

    ts_res_code r = _db.query(q);
    if (r != TS_OK) {
        return r;
    }

    // error
    switch(q.error_code()) {
        case 0: // ok
            break;
        // case 1062:  // Duplicate entry '<tag>' for key 'PRIMARY'
        //     r = TS_DUPLICATE;
        //     break;
        // case 1048:  // Column 'rank' cannot be null
        //     r = TS_SUPER_UNK;
        //     break;
        default:
            r = TS_INTERNAL_ERR;
            std::cerr << "unhandled error(" << q.error_code() << "): "
                      << q.error_msg() << std::endl;
    }

    return r;
}
*/

ts_res_code sqlite::related(tagd::tag_set& R, const tagd::predicate& p, const tagd::id_type& super) {

    sqlite3_stmt *stmt;

    // _entity rank is NULL, so we have to treat it differently
    // subjects will not be filtered according to rank (as _entity is all enpcompassing)
    if (super == "_entity" || super.empty()) {
		if (p.modifier.empty()) {
			ts_res_code ts_rc = this->prepare(&_related_null_super_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND relator IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all relators <= p.relator
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all objects <= p.object
				") "
				"ORDER BY rank",
				"select related"
			);
			if (ts_rc != TS_OK) return ts_rc;
	
			ts_rc = this->bind_text(&_related_null_super_stmt, 1, p.relator.c_str(), "relator");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_null_super_stmt, 2, p.object.c_str(), "object");
			if (ts_rc != TS_OK) return ts_rc;

			stmt = _related_null_super_stmt;
		} else {
			ts_res_code ts_rc = this->prepare(&_related_null_super_modifier_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND relator IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all relators <= p.relator
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all objects <= p.object
				") "
				"AND modifier = ? "
				"ORDER BY rank",
				"select related"
			);
			if (ts_rc != TS_OK) return ts_rc;
	
			ts_rc = this->bind_text(&_related_null_super_modifier_stmt, 1, p.relator.c_str(), "relator");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_null_super_modifier_stmt, 2, p.object.c_str(), "object");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_null_super_modifier_stmt, 3, p.modifier.c_str(), "modifier");
			if (ts_rc != TS_OK) return ts_rc;

			stmt = _related_null_super_modifier_stmt;
		}
    } else {
		if (p.modifier.empty()) {
			ts_res_code ts_rc = this->prepare(&_related_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all subjects <= super
				") "
				"AND relator IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all relators <= p.relator
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all objects <= p.object
				") "
				"ORDER BY rank",
				"select related"
			);
		if (ts_rc != TS_OK) return ts_rc;
	
			ts_rc = this->bind_text(&_related_stmt, 1, super.c_str(), "super");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_stmt, 2, p.relator.c_str(), "relator");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_stmt, 3, p.object.c_str(), "object");
			if (ts_rc != TS_OK) return ts_rc;

			stmt = _related_stmt;
		} else {
			ts_res_code ts_rc = this->prepare(&_related_modifier_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all subjects <= super
				") "
				"AND relator IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all relators <= p.relator
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all objects <= p.object
				") "
				"AND modifier = ? "
				"ORDER BY rank",
				"select related"
			);
		if (ts_rc != TS_OK) return ts_rc;
	
			ts_rc = this->bind_text(&_related_modifier_stmt, 1, super.c_str(), "super");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_modifier_stmt, 2, p.relator.c_str(), "relator");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_modifier_stmt, 3, p.object.c_str(), "object");
			if (ts_rc != TS_OK) return ts_rc;

			ts_rc = this->bind_text(&_related_modifier_stmt, 4, p.modifier.c_str(), "modifier");
			if (ts_rc != TS_OK) return ts_rc;

			stmt = _related_modifier_stmt;
		}
    }

    const int F_SUBJECT = 0;
    const int F_SUPER = 1;
    const int F_POS = 2;
    const int F_RANK = 3;
    const int F_RELATOR = 4;
    const int F_OBJECT = 5;
    const int F_MODIFIER = 6;

    R.clear();
    tagd::tag_set::iterator it = R.begin();
    tagd::rank rank;
    int s_rc;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        rank.init(sqlite3_column_text(stmt, F_RANK));
        tagd::abstract_tag t(
            (const char*) sqlite3_column_text(stmt, F_SUBJECT),
            (const char*) sqlite3_column_text(stmt, F_SUPER),
            (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS),
            rank
        );

        tagd::id_type relator((const char*) sqlite3_column_text(stmt, F_RELATOR));
        tagd::id_type object((const char*) sqlite3_column_text(stmt, F_OBJECT));

		if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			tagd::id_type modifier((const char*) sqlite3_column_text(stmt, F_MODIFIER));
			t.relation(relator, object, modifier);
		} else {
			t.relation(relator, object);
		}

        it = R.insert(it, t);
    }

    if (s_rc == SQLITE_ERROR) {
        this->print_err("related failed");
        return TS_ERR;
    }

    if (R.size() == 0)
        return TS_NOT_FOUND;

    return TS_OK;
}

ts_res_code sqlite::query(tagd::tag_set& R, const tagd::interrogator& intr) {
    //TODO use the id (who, what, when, where, why, how_many...)
    // to distinguish types of queries

    ts_res_code ts_rc;
    if (intr.relations.empty()) {
        if (intr.super().empty())
            return TS_NOT_FOUND;  // empty interrogator, nothing to do
        else {
            tagd::abstract_tag t;
            ts_rc = this->get(t, intr.super());
            if (ts_rc == TS_OK)
                R.insert(t);
            return ts_rc;
        }
    }

	// dout << "interrogator: " << intr << std::endl;

    tagd::tag_set T;  // temp results
    tagd::tag_set S;  // related per predicate
    for (tagd::predicate_set::const_iterator it = intr.relations.begin();
                it != intr.relations.end(); ++it) {

        S.clear();

        ts_rc = this->related(S, *it, intr.super());
        if (ts_rc != TS_OK)
            return ts_rc; // not found or err

        size_t n = merge_tags_erase_diffs(T, S);
        assert( n > 0 );
    }

    if (T.size() == 0)
        return TS_NOT_FOUND;

    // merge temp results into final results
    merge_tags(R, T);

    return TS_OK;
}

ts_res_code sqlite::dump(std::ostream& os) {
    sqlite3_stmt *stmt = NULL;
    ts_res_code ts_rc = this->prepare(&stmt,
        "SELECT tag, super, rank, pos FROM tags ORDER BY rank",
        "dump tags"
    );
    if (ts_rc != TS_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER = 1;
    const int F_RANK = 2;
    const int F_POS = 3;

    int s_rc;
    const int colw = 20;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_ID) 
           << std::setw(colw) << std::left
            << tagd::super_relator((tagd::part_of_speech) sqlite3_column_int(stmt, F_POS))
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUPER)
           << std::setw(colw) << std::left
           << tagd::rank::dotted_str(sqlite3_column_text(stmt, F_RANK))
           << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
        this->print_err("dump failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}

ts_res_code sqlite::dump_relations(std::ostream& os) {
    sqlite3_stmt *stmt = NULL;
    ts_res_code ts_rc = this->prepare(&stmt,
        "SELECT subject, relator, object, modifier "
        "FROM relations, tags "
        "WHERE subject = tag "
        "ORDER BY rank",
        "dump relations"
    );
    if (ts_rc != TS_OK) return ts_rc;

    const int F_SUBJECT = 0;
    const int F_RELATOR = 1;
    const int F_OBJECT = 2;
    const int F_MODIFIER = 3;

    int s_rc;
    const int colw = 20;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUBJECT) 
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_RELATOR)
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_OBJECT);

        if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
            os << std::setw(colw) << std::left
               << sqlite3_column_text(stmt, F_MODIFIER);
        }

        os << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
        this->print_err("dump relations failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}

ts_res_code sqlite::child_ranks(tagd::rank_set& R, const tagd::id_type& super_id) {
    if (super_id.length() > tagd::MAX_TAG_LEN) {
        dout << "super_id id exceeds MAX_TAG_LEN" << std::endl;
        return TS_ERR_MAX_TAG_LEN;
    }

    ts_res_code ts_rc = this->open();
    if (ts_rc != TS_OK) return ts_rc;

    int s_rc = this->prepare(&_child_ranks_stmt ,
        "SELECT rank FROM tags WHERE super = ?",
        "child ranks"
    );

    ts_rc = this->bind_text(&_child_ranks_stmt, 1, super_id.c_str(), "rank super_id");
    if (ts_rc != TS_OK) return ts_rc;

    const int F_RANK = 0;
    tagd::rank rank;
    tagd::rank_code rc;

    while ((s_rc = sqlite3_step(_child_ranks_stmt)) == SQLITE_ROW) {
        rc = rank.init(sqlite3_column_text(_child_ranks_stmt, F_RANK));
        switch (rc) {
            case tagd::RANK_OK:
                break;
            case tagd::RANK_EMPTY:
                if (super_id == "_entity") {
                    // _entity is the only tag having NULL rank
                    // don't insert _entity in the rank_set,
                    // only its children
                    continue;
                } else {
                    std::cerr << "integrity error: NULL rank" << std::endl;
                    return TS_INTERNAL_ERR;
                }
                break;
            default:
                std::cerr << "get_childs rank.init() error: "
                          << tagd::rank_code_str(rc) << std::endl;
                return TS_ERR;
        }

        R.insert(rank);
    }

    return TS_OK;
}

ts_res_code sqlite::exec(const char *sql, const char *label) {
    ts_res_code ts_rc = this->open();
    if (ts_rc != TS_OK) return ts_rc;

    char *err = NULL;

    int s_rc = sqlite3_exec(_db, sql, NULL, NULL, &err);
    if (s_rc != SQLITE_OK) {
        if (label != NULL)
            std::cerr << "'" << label << "` failed: ";
        else
            std::cerr << "exec failed: ";
        std::cerr << err << std::endl;
        sqlite3_free(err);
        return TS_ERR;
    }

    return TS_OK;
}

ts_res_code sqlite::prepare(sqlite3_stmt **stmt, const char *sql, const char *label) {
    ts_res_code ts_rc = this->open();
    if (ts_rc != TS_OK) return ts_rc;

    if (*stmt == NULL) {
        int s_rc = sqlite3_prepare_v2(
            _db,
            // update the rank prefix from old to new
            sql,
            -1,         // -1 copies \0 term string, otherwise use nbytes
            stmt,      // OUT: Statement handle
            NULL        // OUT: Pointer to unused portion of zSql
        );
        if (s_rc != SQLITE_OK) {
            std::stringstream ss;
            if (label != NULL)
                ss << "'" << label << "` ";
            ss << "prepare statement failed";
            this->print_err(ss.str().c_str());
            *stmt = NULL;
            return TS_INTERNAL_ERR;
        }
    } else {
        sqlite3_reset(*stmt);
        sqlite3_clear_bindings(*stmt);
    }

    return TS_OK;
}

inline ts_res_code sqlite::bind_text(sqlite3_stmt**stmt, int i, const char *text, const char*label) {
    if (sqlite3_bind_text(*stmt, i, text, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        this->finalize_print_err(stmt, label, "bind failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}

inline ts_res_code sqlite::bind_int(sqlite3_stmt**stmt, int i, int val, const char*label) {
    if (sqlite3_bind_int(*stmt, i, val) != SQLITE_OK) {
        this->finalize_print_err(stmt, label, "bind failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}

inline ts_res_code sqlite::bind_rowid(sqlite3_stmt**stmt, int i, rowid_t id, const char*label) {
    if (id <= 0)  // interpret as NULL
        return this->bind_null(stmt, i, label);
 
    if (sqlite3_bind_int64(*stmt, i, id) != SQLITE_OK) {
        this->finalize_print_err(stmt, label, "bind failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}

inline ts_res_code sqlite::bind_null(sqlite3_stmt**stmt, int i, const char*label) {
    if (sqlite3_bind_null(*stmt, i) != SQLITE_OK) {
        this->finalize_print_err(stmt, label, "bind failed");
        return TS_INTERNAL_ERR;
    }

    return TS_OK;
}


void sqlite::finalize() {
    sqlite3_finalize(_get_stmt);
    sqlite3_finalize(_exists_stmt);
    sqlite3_finalize(_insert_stmt);
    sqlite3_finalize(_update_tag_stmt);
    sqlite3_finalize(_update_ranks_stmt);
    sqlite3_finalize(_child_ranks_stmt);
    sqlite3_finalize(_insert_relations_stmt);
    sqlite3_finalize(_get_relations_stmt);
    sqlite3_finalize(_related_stmt);
    sqlite3_finalize(_related_null_super_stmt);
    /*** uridb ***/
    sqlite3_finalize(_get_uri_stmt);
    sqlite3_finalize(_get_uri_specs_stmt);
    sqlite3_finalize(_get_host_stmt);
    sqlite3_finalize(_get_authority_stmt);
    sqlite3_finalize(_get_uri_relations_stmt);
    sqlite3_finalize(_insert_uri_stmt);
    sqlite3_finalize(_insert_uri_specs_stmt);
    sqlite3_finalize(_insert_host_stmt);
    sqlite3_finalize(_insert_authority_stmt);
    sqlite3_finalize(_insert_uri_relations_stmt);

    _get_stmt = NULL;
    _exists_stmt = NULL;
    _insert_stmt = NULL;
    _update_tag_stmt = NULL;
    _update_ranks_stmt = NULL;
    _child_ranks_stmt = NULL;
    _insert_relations_stmt = NULL;
    _get_relations_stmt = NULL;
    _related_stmt = NULL;
    _related_null_super_stmt = NULL;
    /*** uridb ***/
    _get_uri_stmt = NULL;
    _get_uri_specs_stmt = NULL;
    _get_host_stmt = NULL;
    _get_authority_stmt = NULL;
    _get_uri_relations_stmt = NULL;
    _insert_uri_specs_stmt = NULL;
    _insert_uri_stmt = NULL;
    _insert_host_stmt = NULL;
    _insert_authority_stmt = NULL;
    _insert_uri_relations_stmt = NULL;
}

void sqlite::finalize_print_err(sqlite3_stmt **stmt, const char *msg, const char *append) {
    this->print_err(msg, append);
    sqlite3_finalize(*stmt);
    *stmt = NULL;
}

// prepends msg to last sqlite err msg
void sqlite::print_err(const char *msg, const char *append) {
    if (msg != NULL) {
        std::cerr << msg;
        if (append != NULL)
            std::cerr << " " << append;
        std::cerr << ": ";
    } else if (append != NULL) {
            std::cerr << append << ": ";
    } 
    std::cerr << "(" << sqlite_err_code_str(sqlite3_errcode(_db)) << ") "
        << sqlite3_errmsg(_db) << std::endl;
}

} // namespace tagspace
