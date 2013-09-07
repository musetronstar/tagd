#include <iostream>
#include <sstream>
#include <iomanip> // setw
#include <cstring> // strcmp
#include <assert.h>

#include "tagspace/bootstrap.h"
#include "tagspace/sqlite.h"

void trace_callback( void* udp, const char* sql ) {
	if (udp == NULL) {
		std::cerr << "SQL trace: " << sql << std::endl;
	} else {
		std::cerr << "SQL trace(udp " << udp << "): " << sql << std::endl;
	}
}

// TODO
// profile_callback
// void *sqlite3_profile(sqlite3*, void(*xProfile)(void*,const char*,sqlite3_uint64), void*);

typedef enum {
    // specify types as sqlite only returns generic SQLITE_CONSTRAINT
    TS_SQLITE_UNIQUE,
    TS_SQLITE_FK,

    TS_SQLITE_UNK
} ts_sqlite_code;

ts_sqlite_code sqlite_constraint_type(const char *err) {
	if (strcmp(err, "columns subject, object are not unique") == 0)
		return TS_SQLITE_UNIQUE;

	if (strcmp(err, "foreign key constraint failed") == 0)
		return TS_SQLITE_FK;

	return TS_SQLITE_UNK;
}

void finalize_stmt(sqlite3_stmt **stmt) {
    sqlite3_finalize(*stmt);
    *stmt = NULL;
}


#define OK_OR_RET_ERR() if(_code != tagd::TAGD_OK) return _code;

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

tagd::code sqlite::init(const std::string& fname) {
	_doing_init = true;
	this->_init(fname);
	_doing_init = false;
	if (_code == tagd::TS_INIT)
		_code = tagd::TAGD_OK;
	return _code;
}

tagd::code sqlite::_init(const std::string& fname) {
    if (fname.empty())
		return this->error(tagd::TS_ERR, "init: fname empty");

    _db_fname = fname;

    this->close();	// won't fail if not open
	this->open();	// open will create db if not exists
	OK_OR_RET_ERR();

    this->exec("BEGIN");

    if (_code == tagd::TAGD_OK)
		this->create_tags_table();

    if (_code == tagd::TAGD_OK)
        this->create_relations_table();

    if (_code == tagd::TAGD_OK) {
		// check if hard_tags already initialized
		tagd::tag t;
		this->get(t, "_super");
		if (_code == tagd::TS_NOT_FOUND)
			bootstrap::init_hard_tags(*this); // on success _code will be TAGD_OK
	}

    if (_code != tagd::TAGD_OK) {
		// return error code before exec overwrites it
		tagd::code ts_rc = _code;
        this->exec("ROLLBACK");
		return this->code(ts_rc);
    }

    this->exec("COMMIT");

    return _code;
}

tagd::code sqlite::create_tags_table() {
    // check db
    sqlite3_stmt *stmt = NULL; 
    this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE tags%'",
        "tags table exists"
    );
	OK_OR_RET_ERR();

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_INTERNAL_ERR, "check tags table error");
    }
    
    // table exists
    if (s_rc == SQLITE_ROW) {
        tagd::tag t;
        this->get(t, "_entity");
		OK_OR_RET_ERR();

        if (t.id() == "_entity" && t.super() == "_entity")
            return tagd::TAGD_OK; // db already initialized
    
        return this->error(tagd::TS_INTERNAL_ERR, "tag table exists but has no _entity row; database corrupt");
    }

    //create db

    this->exec(
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
        // and the only tag allowed to have rank IS NULL (indicating the root)
        "CHECK ( "
            "(tag  = '_entity' AND super = '_entity' AND rank IS NULL) OR "
            "(tag <> '_entity' AND pos <> 0 AND rank IS NOT NULL) "  // 0 == POS_UNKNOWN
        ")"  // TODO  check rank <= super.rank
    ")" 
    );
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_pos ON tags(pos)");
	OK_OR_RET_ERR();

	this->exec(
		"INSERT INTO tags (tag, super, rank, pos) "
		"VALUES ('_entity', '_entity', NULL, 1)"
	); // IMPORTANT 1 == POS_TAG

    return _code;
}

tagd::code sqlite::create_relations_table() {
    // check db
    sqlite3_stmt *stmt = NULL; 
    this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE relations%'",
        "relations table exists"
    );
	OK_OR_RET_ERR();

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "check relations table error");
    
    // table exists
    if (s_rc == SQLITE_ROW)
        return tagd::TAGD_OK;

    //create db
    this->exec(
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
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_subject ON relations(subject)");
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_object ON relations(object)");

    return _code;
}


tagd::code sqlite::open() {
    if (_db != NULL)
		return this->code(tagd::TAGD_OK);

    int rc = sqlite3_open(_db_fname.c_str(), &_db);
    if( rc != SQLITE_OK ){
        this->close();
        return this->error(tagd::TS_INTERNAL_ERR, "open database failed: %s", _db_fname.c_str());
    }

	tagd::code ts_rc = this->exec("PRAGMA foreign_keys = ON");
    if (ts_rc != tagd::TAGD_OK)
        return this->error(ts_rc, "setting foreign_keys on failed: %s", _db_fname.c_str());

    return this->code(tagd::TAGD_OK);
}

void sqlite::close() {
    if (_db == NULL)
        return;

    this->finalize();
    sqlite3_close(_db);
    _db = NULL;
}

tagd::code sqlite::get(tagd::abstract_tag& t, const tagd::id_type& id) {
    this->prepare(&_get_stmt,
        "SELECT tag, super, pos, rank "
        "FROM tags WHERE tag = ?",
        "select tags"
    );
	OK_OR_RET_ERR(); 

    this->bind_text(&_get_stmt, 1, id.c_str(), "get statement id");
	OK_OR_RET_ERR(); 

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

        this->get_relations(t.relations, id);
        if (!(_code == tagd::TAGD_OK || _code == tagd::TS_NOT_FOUND)) return _code;

        return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_ERR, "get failed: %s", id.c_str());
    } else {  // TODO check other error codes
        return this->code(tagd::TS_NOT_FOUND);
    }
}

tagd::code sqlite::get(tagd::url& u, const tagd::id_type& id) {
	// id should be a canonical url
	u.init(id);
	if (!u.ok())
		return this->error(u.code(), "url init failed: %s", id.c_str());

	// we use hdurl to identify urls internally
	tagd::id_type hdurl = u.hdurl();
	this->get((tagd::abstract_tag&)u, hdurl);
	OK_OR_RET_ERR();

	u.init_hdurl(hdurl);  // id() should now be canonical url
	if (!u.ok())
		return this->error(u.code(), "url init_hdurl failed: %s", hdurl.c_str());

	return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::get_relations(tagd::predicate_set& P, const tagd::id_type& id) {

    this->prepare(&_get_relations_stmt,
        "SELECT relator, object, modifier "
        "FROM relations WHERE subject = ?",
        "select relations"
    );
	OK_OR_RET_ERR();

	this->bind_text(&_get_relations_stmt, 1, id.c_str(), "get subject");
	OK_OR_RET_ERR();

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

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_ERR, "get relations failed: %s", id.c_str());

    if (P.empty())
		return this->code(tagd::TS_NOT_FOUND);

    return this->code(tagd::TAGD_OK);
}

tagd::part_of_speech sqlite::pos(const tagd::id_type& id) {
	assert(id.length() <= tagd::MAX_TAG_LEN);

    tagd::code ts_rc = this->prepare(&_pos_stmt,
        "SELECT pos FROM tags WHERE tag = ?",
        "tag pos"
    );
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    ts_rc = this->bind_text(&_pos_stmt, 1, id.c_str(), "pos id");
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    const int F_POS = 0;

    int s_rc = sqlite3_step(_pos_stmt);
    if (s_rc == SQLITE_ROW) {
        return (tagd::part_of_speech) sqlite3_column_int(_pos_stmt, F_POS);
    } else if (s_rc == SQLITE_ERROR) {
        this->error(tagd::TS_INTERNAL_ERR, "pos failed: %s", id.c_str());
        return tagd::POS_UNKNOWN;
    } else {
        return tagd::POS_UNKNOWN;
    }
}

tagd::code sqlite::exists(const tagd::id_type& id) {
    this->prepare(&_exists_stmt,
        "SELECT 1 FROM tags WHERE tag = ?",
        "tag exists"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_exists_stmt, 1, id.c_str(), "exists statement id");
    OK_OR_RET_ERR();

    int s_rc = sqlite3_step(_exists_stmt);
    if (s_rc == SQLITE_ROW) {
        return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_ERR, "exists failed: %", id.c_str());
    } else {  // TODO check other error codes
        return this->code(tagd::TS_NOT_FOUND);
    }
}

// tagd::TS_NOT_FOUND returned if destination undefined
tagd::code sqlite::put(const tagd::abstract_tag& t) {
    if (t.id().length() > tagd::MAX_TAG_LEN)
        return this->error(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (t.id().empty())
		return this->error(tagd::TS_ERR, "inserting empty tag not allowed");

	if (t.id()[0] == '_' && !_doing_init)
		return this->error(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", t.id().c_str()); 

    tagd::abstract_tag existing;
    tagd::code existing_rc = this->get(existing, t.id());

    if (existing_rc != tagd::TAGD_OK && existing_rc != tagd::TS_NOT_FOUND)
        return existing_rc; // err set by get

    if (t.super().empty()) {
        if (existing_rc == tagd::TS_NOT_FOUND) {
            // can't do anything without knowing what a tag is
            return this->code(tagd::TS_SUPER_UNK);
        } else {
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return this->code(tagd::TS_DUPLICATE);
            else // insert relations
                return this->insert_relations(t);
        }
    }

    tagd::abstract_tag destination;
    tagd::code dest_rc = this->get(destination, t.super());

    if (dest_rc != tagd::TAGD_OK) {
        if (dest_rc == tagd::TS_NOT_FOUND)
            return this->error(tagd::TS_SUPER_UNK, "unknown super: %s", t.super().c_str());
        else
            return dest_rc; // err set by get
    }

    // handle duplicate tags up-front
    tagd::code ins_upd_rc;
    if (existing_rc == tagd::TAGD_OK) {  // existing tag
        if (t.super() == existing.super()) {  // same location
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return this->code(tagd::TS_DUPLICATE);
            else // insert relations
                return this->insert_relations(t);
        }
        // move existing to new location
        ins_upd_rc = this->update(existing, destination);
		//t = existing;
    } else if (existing_rc == tagd::TS_NOT_FOUND) {
        // new tag
        ins_upd_rc = this->insert(t, destination);
    } else {
        assert(false); // should never get here
        return this->error(tagd::TS_INTERNAL_ERR, "put failed: %s", t.id().c_str());
    }

    if (ins_upd_rc == tagd::TAGD_OK && !t.relations.empty())
        return this->insert_relations(t);

    // res from insert/update
    return this->code(ins_upd_rc);
}

tagd::code sqlite::put(const tagd::url& u) {
	if (!u.ok())
		return this->error(u.code(), "put url not ok(%s): %s",  tagd_code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hdurl
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hdurl());
	tagd::url::insert_url_part_relations(t.relations, u);
	return this->put(t);
}

tagd::code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& super) {
    assert( !super.rank().empty() || (super.rank().empty() && super.id() == "_entity") );

    tagd::rank_set R;
    this->child_ranks(R, super.id());
    OK_OR_RET_ERR(); 

    if (R.empty()) { // create first child element
        next = super.rank();
        next.push(1);
        return this->code(tagd::TAGD_OK);
    }

    tagd::rank_code r_rc = tagd::rank::next(next, R);
    if (r_rc != tagd::RANK_OK)
        return this->error(tagd::TS_ERR, "next_rank error: %s", tagd::rank_code_str(r_rc));

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::insert(const tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
    assert( t.super() == destination.id() );
    assert( !t.id().empty() );
    assert( !t.super().empty() );

    tagd::rank rank;
    next_rank(rank, destination);
    OK_OR_RET_ERR(); 

    // dout << "insert next rank: (" << rank.dotted_str() << ")" << std::endl;

    this->prepare(&_insert_stmt,
        "INSERT INTO tags (tag, super, rank, pos) VALUES (?, ?, ?, ?)",
        "insert tag"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_insert_stmt, 1, t.id().c_str(), "insert id");
    OK_OR_RET_ERR(); 
 
	this->bind_text(&_insert_stmt, 2, t.super().c_str(), "insert super");
    OK_OR_RET_ERR(); 

	this->bind_text(&_insert_stmt, 3, rank.c_str(), "insert rank");
    OK_OR_RET_ERR(); 

	tagd::part_of_speech pos;
	if (t.pos() == tagd::POS_UNKNOWN) {
		// use pos of super
		pos = this->pos(t.super());
		assert(pos != tagd::POS_UNKNOWN);
	} else {
		pos = t.pos();
	}
	this->bind_int(&_insert_stmt, 4, pos, "insert pos");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_insert_stmt);
    if (s_rc != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "insert tag failed: %s", t.id().c_str());

    return this->code(tagd::TAGD_OK);
}

// update existing with new tag
tagd::code sqlite::update(const tagd::abstract_tag& t,
                                      const tagd::abstract_tag& destination) {
    assert( !t.id().empty() );
    assert( !t.super().empty() );
    assert( !destination.super().empty() );

    tagd::rank rank;
    next_rank(rank, destination);
    OK_OR_RET_ERR(); 

	// dout << "update next rank: (" << rank.dotted_str() << ")" << std::endl;

    // TODO look into the impact of using a TRANSACTION

    // update the ranks
    this->prepare(&_update_ranks_stmt, 
        "UPDATE tags "
        "SET rank = (? || substr(rank, ?)) "
        "WHERE rank GLOB (? || '*')",
        "update ranks"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_ranks_stmt, 1, rank.c_str(), "new rank");
    OK_OR_RET_ERR(); 

    this->bind_int(&_update_ranks_stmt, 2, (t.rank().size()+1), "append rank");
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_ranks_stmt, 3, t.rank().c_str(), "sub rank");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_update_ranks_stmt);
    if (s_rc != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "update rank failed: %s", t.id().c_str());

    //update tag
    this->prepare(&_update_tag_stmt, 
            "UPDATE tags SET super = ? WHERE tag = ?",
            "update tag"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_tag_stmt, 1, destination.id().c_str(), "update super");
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_tag_stmt, 2, t.id().c_str(), "update tag");
    OK_OR_RET_ERR(); 

    s_rc = sqlite3_step(_update_tag_stmt);
    if (s_rc != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "update tag failed: %s", t.id().c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::insert_relations(const tagd::abstract_tag& t) {
    assert( !t.id().empty() );
    assert( !t.relations.empty() );

    this->prepare(&_insert_relations_stmt,
        "INSERT INTO relations (subject, relator, object, modifier) "
        "VALUES (?, ?, ?, ?)",
        "insert relations"
    );
    OK_OR_RET_ERR(); 

    tagd::predicate_set::iterator it = t.relations.begin();
    if (it == t.relations.end())
        return this->error(tagd::TS_INTERNAL_ERR, "insert empty relations");

    // if all inserts were unique constraint violations, tagd::TS_DUPLICATE will be returned
    size_t num_inserted = 0;

    while (true) {
        this->bind_text(&_insert_relations_stmt, 1, t.id().c_str(), "insert subject");
		OK_OR_RET_ERR(); 

		this->bind_text(&_insert_relations_stmt, 2, it->relator.c_str(), "insert relator");
		OK_OR_RET_ERR(); 

		this->bind_text(&_insert_relations_stmt, 3, it->object.c_str(), "insert object");
		OK_OR_RET_ERR(); 

		if (it->modifier.empty())
			this->bind_null(&_insert_relations_stmt, 4, "insert NULL modifier");
		else
			this->bind_text(&_insert_relations_stmt, 4, it->modifier.c_str(), "insert modifier");
		OK_OR_RET_ERR(); 


        int s_rc = sqlite3_step(_insert_relations_stmt);

        if (s_rc == SQLITE_DONE) {
            num_inserted++;
        } else if (s_rc == SQLITE_CONSTRAINT) {
            // sqlite does't tell us whether the object or relator caused the violation
			const char* errmsg = sqlite3_errmsg(_db);
            ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
            if (ts_sql_rc == TS_SQLITE_UNIQUE) { // object UNIQUE violation
                this->exists(it->relator);
                switch ( _code ) {
					case tagd::TS_NOT_FOUND:  return this->error(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
                    case tagd::TAGD_OK:         break; // TODO update the relation
                    default:            return _code; // error
                }
            } else if (ts_sql_rc == TS_SQLITE_FK) {
                // relations.object FK tags.tag violated
                this->exists(it->object);
                switch ( _code ) {
					case tagd::TS_NOT_FOUND:  return this->error(tagd::TS_OBJECT_UNK, "unknown object: %s", it->object.c_str());
					case tagd::TAGD_OK:  return this->error(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
                    default:            return _code;
                }
            } else {  // TS_SQLITE_UNK
                return this->error(tagd::TS_INTERNAL_ERR, "insert relations error: %s", errmsg);
            } 
        } else {
            return this->error(tagd::TS_ERR, "insert relations failed: %s, %s, %s",
									t.id().c_str(), it->relator.c_str(), it->object.c_str());
        }

        if (++it != t.relations.end()) {
            sqlite3_reset(_insert_relations_stmt);
            sqlite3_clear_bindings(_insert_relations_stmt);
        } else {
            break;
        }
    }  // while

	return this->code(num_inserted == 0 ? tagd::TS_DUPLICATE : tagd::TAGD_OK);
}

tagd::code sqlite::related(tagd::tag_set& R, const tagd::predicate& p, const tagd::id_type& super) {

    sqlite3_stmt *stmt;

    // _entity rank is NULL, so we have to treat it differently
    // subjects will not be filtered according to rank (as _entity is all enpcompassing)
    if (super == "_entity" || super.empty()) {
		if (p.modifier.empty()) {
			this->prepare(&_related_null_super_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = ? "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				 "SELECT tag FROM tags WHERE rank GLOB ( "
				  "SELECT rank FROM tags WHERE tag = ? "
				 ") || '*'" // all objects <= p.object
				") "
				"ORDER BY rank",
				"select related"
			);
			OK_OR_RET_ERR();

			if (p.relator.empty()) {
				this->bind_null(&_related_null_super_stmt, 1, "null relator");
				OK_OR_RET_ERR();

				this->bind_null(&_related_null_super_stmt, 2, "relator");
				OK_OR_RET_ERR();
			} else {
				this->bind_text(&_related_null_super_stmt, 1, p.relator.c_str(), "null relator");
				OK_OR_RET_ERR();

				this->bind_text(&_related_null_super_stmt, 2, p.relator.c_str(), "relator");
				OK_OR_RET_ERR();
			}

			this->bind_text(&_related_null_super_stmt, 3, p.object.c_str(), "object");
			OK_OR_RET_ERR();

			stmt = _related_null_super_stmt;
		} else {
			this->prepare(&_related_null_super_modifier_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = ? "
				  ") || '*'" // all relators <= p.relator
				 ") "
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
			OK_OR_RET_ERR();
	
			if (p.relator.empty()) {
				this->bind_null(&_related_null_super_modifier_stmt, 1, "null relator");
				OK_OR_RET_ERR();

				this->bind_null(&_related_null_super_modifier_stmt, 2, "relator");
				OK_OR_RET_ERR();
			} else {
				this->bind_text(&_related_null_super_modifier_stmt, 1, p.relator.c_str(), "null relator");
				OK_OR_RET_ERR();

				this->bind_text(&_related_null_super_modifier_stmt, 2, p.relator.c_str(), "relator");
				OK_OR_RET_ERR();
			}

			this->bind_text(&_related_null_super_modifier_stmt, 3, p.object.c_str(), "object");
			OK_OR_RET_ERR();

			this->bind_text(&_related_null_super_modifier_stmt, 4, p.modifier.c_str(), "modifier");
			OK_OR_RET_ERR();

			stmt = _related_null_super_modifier_stmt;
		}
    } else {
		if (p.modifier.empty()) {
			this->prepare(&_related_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all subjects <= super
				") "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = ? "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all objects <= p.object
				") "
				"ORDER BY rank",
				"select related"
			);
			OK_OR_RET_ERR();
	
			this->bind_text(&_related_stmt, 1, super.c_str(), "super");
			OK_OR_RET_ERR();

			if (p.relator.empty()) {
				this->bind_null(&_related_stmt, 2, "null test relator");
				OK_OR_RET_ERR();

				this->bind_null(&_related_stmt, 3, "null relator");
				OK_OR_RET_ERR();
			} else {
				this->bind_text(&_related_stmt, 2, p.relator.c_str(), "test relator");
				OK_OR_RET_ERR();

				this->bind_text(&_related_stmt, 3, p.relator.c_str(), "relator");
				OK_OR_RET_ERR();
			}

			this->bind_text(&_related_stmt, 4, p.object.c_str(), "object");
			OK_OR_RET_ERR();

			stmt = _related_stmt;
		} else {
			this->prepare(&_related_modifier_stmt,
				"SELECT subject, super, pos, rank, relator, object, modifier "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = ? "
				") || '*'" // all subjects <= super
				") "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = ? "
				  ") || '*'" // all relators <= p.relator
				 ") "
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
			OK_OR_RET_ERR();
	
			this->bind_text(&_related_modifier_stmt, 1, super.c_str(), "super");
			OK_OR_RET_ERR();

			if (p.relator.empty()) {
				this->bind_null(&_related_modifier_stmt, 2, "null relator");
				OK_OR_RET_ERR();

				this->bind_null(&_related_modifier_stmt, 3, "relator");
				OK_OR_RET_ERR();
			} else {
				this->bind_text(&_related_modifier_stmt, 2, p.relator.c_str(), "null relator");
				OK_OR_RET_ERR();

				this->bind_text(&_related_modifier_stmt, 3, p.relator.c_str(), "relator");
				OK_OR_RET_ERR();
			}

			this->bind_text(&_related_modifier_stmt, 4, p.object.c_str(), "object");
			OK_OR_RET_ERR();

			this->bind_text(&_related_modifier_stmt, 5, p.modifier.c_str(), "modifier");
			OK_OR_RET_ERR();

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
		return this->error(tagd::TS_ERR, "related failed(super, relator, object, modifier): %s, %s, %s, %s",
							super.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str());
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query(tagd::tag_set& R, const tagd::interrogator& intr) {
    //TODO use the id (who, what, when, where, why, how_many...)
    // to distinguish types of queries

    if (intr.relations.empty()) {
        if (intr.super().empty())
            return tagd::TS_NOT_FOUND;  // empty interrogator, nothing to do
        else {
            tagd::abstract_tag t;
			// TODO get children of super
            this->get(t, intr.super());
			OK_OR_RET_ERR();
            R.insert(t);
			return _code;
        }
    }

	// std::cerr << "interrogator: " << std::endl << intr << std::endl;

	size_t n = 0;
    tagd::tag_set S;  // related per predicate
    for (tagd::predicate_set::const_iterator it = intr.relations.begin();
                it != intr.relations.end(); ++it) {

        S.clear();

        this->related(S, *it, intr.super());
		// tagd::print_tag_ids(S);
		OK_OR_RET_ERR();

		n += merge_containing_tags(R, S);
    }

    if (n == 0)
        return this->code(tagd::TS_NOT_FOUND);

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::dump_grid(std::ostream& os) {
    sqlite3_stmt *stmt = NULL;
    tagd::code ts_rc = this->prepare(&stmt,
        "SELECT tag, super, rank, pos FROM tags ORDER BY rank",
        "dump grid"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER = 1;
    const int F_RANK = 2;
    const int F_POS = 3;

    int s_rc;
    const int colw = 20;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_ID) 
           << std::setw(colw) << std::left
           << tagd::super_relator(pos)
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUPER)
           << std::setw(colw) << std::left << pos_str(pos)
           << std::setw(colw) << std::left
           << tagd::rank::dotted_str(sqlite3_column_text(stmt, F_RANK))
           << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
        this->error(tagd::TS_INTERNAL_ERR, "dump grid failed");
    }

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::dump(std::ostream& os) {
	// dump tag identities before relations, so that they are
	// all known by the time relations are added
    sqlite3_stmt *stmt = NULL;
    tagd::code ts_rc = this->prepare(&stmt,
        "SELECT tag, super, pos "
        "FROM tags "
        "ORDER BY rank",
        "dump tags"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER = 1;
	const int F_POS = 2;

    int s_rc;
	tagd::abstract_tag *t = NULL;
	tagd::id_type id;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		id = (const char*) sqlite3_column_text(stmt, F_ID);

		// ignore hard tags for now
		if (id[0] == '_') continue;

		if (t == NULL || t->id() != id) {
			if (t != NULL) {
				os << *t << std::endl << std::endl; 
				delete t;
			}

			t = new tagd::abstract_tag(
					id,
					(const char*) sqlite3_column_text(stmt, F_SUPER),
					(tagd::part_of_speech) sqlite3_column_int(_get_stmt, F_POS) );
		}
    }

	if (t != NULL) {
		os << *t << std::endl << std::endl;
		delete t;
	}

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        this->error(tagd::TS_INTERNAL_ERR, "dump tags failed");

	// dump relations
    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT subject, relator, object, modifier "
        "FROM relations, tags "
        "WHERE subject = tag "
        "ORDER BY rank",
        "dump relations"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    // F_ID = 0
    const int F_RELATOR = 1;
    const int F_OBJECT = 2;
    const int F_MODIFIER = 3;

	t = NULL;
	id.clear();
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		id = (const char*) sqlite3_column_text(stmt, F_ID);

		// ignore hard tags for now
		if (id[0] == '_') continue;

		if (t == NULL || t->id() != id) {
			if (t != NULL) {
				os << *t << std::endl << std::endl; 
				delete t;
			}

			t = new tagd::abstract_tag(id);
		}

        if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				(const char*) sqlite3_column_text(stmt, F_OBJECT),
				(const char*) sqlite3_column_text(stmt, F_MODIFIER) );
        } else {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				(const char*) sqlite3_column_text(stmt, F_OBJECT) );
		}
    }

	if (t != NULL) {
		os << *t << std::endl;
		delete t;
	}

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        this->error(tagd::TS_INTERNAL_ERR, "dump relations failed");

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::child_ranks(tagd::rank_set& R, const tagd::id_type& super_id) {
    this->open();
	OK_OR_RET_ERR();
    

    int s_rc = this->prepare(&_child_ranks_stmt ,
        "SELECT rank FROM tags WHERE super = ?",
        "child ranks"
    );

    this->bind_text(&_child_ranks_stmt, 1, super_id.c_str(), "rank super_id");
	OK_OR_RET_ERR();

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
                    return this->error(tagd::TS_INTERNAL_ERR, "integrity error: NULL rank");
                }
                break;
            default:
                return this->error(tagd::TS_ERR, "get_childs rank.init() error: %s", tagd::rank_code_str(rc));
        }

        R.insert(rank);
    }

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::exec(const char *sql, const char *label) {
    this->open();
    OK_OR_RET_ERR();

    char *msg = NULL;

    int s_rc = sqlite3_exec(_db, sql, NULL, NULL, &msg);
	std::stringstream ss;
    if (s_rc != SQLITE_OK) {
        if (label != NULL)
            ss << "'" << label << "` failed: ";
        else
            ss << "exec failed: " << msg;
		//ss << std::endl << "sql: " << std::endl << sql << std::endl;
        sqlite3_free(msg);
		return this->error(tagd::TS_ERR, ss.str().c_str());
    }

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::prepare(sqlite3_stmt **stmt, const char *sql, const char *label) {
    this->open();
    OK_OR_RET_ERR();

    if (*stmt == NULL) {
        int s_rc = sqlite3_prepare_v2(
            _db,
            sql,
            -1,         // -1 copies \0 term string, otherwise use nbytes
            stmt,      // OUT: Statement handle
            NULL        // OUT: Pointer to unused portion of zSql
        );
        if (s_rc != SQLITE_OK) {
            *stmt = NULL;
            if (label != NULL)
				return this->error(tagd::TS_INTERNAL_ERR, "'%s` prepare statement failed: %s", label, sqlite3_errmsg(_db));
			else
				return this->error(tagd::TS_INTERNAL_ERR, "prepare statement failed: %s", sqlite3_errmsg(_db));
        }
    } else {
        sqlite3_reset(*stmt);
        sqlite3_clear_bindings(*stmt);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_text(sqlite3_stmt**stmt, int i, const char *text, const char*label) {
    if (sqlite3_bind_text(*stmt, i, text, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->error(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_int(sqlite3_stmt**stmt, int i, int val, const char*label) {
    if (sqlite3_bind_int(*stmt, i, val) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->error(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_rowid(sqlite3_stmt**stmt, int i, rowid_t id, const char*label) {
    if (id <= 0)  // interpret as NULL
        return this->bind_null(stmt, i, label);
 
    if (sqlite3_bind_int64(*stmt, i, id) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->error(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_null(sqlite3_stmt**stmt, int i, const char*label) {
    if (sqlite3_bind_null(*stmt, i) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->error(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}


void sqlite::finalize() {
    sqlite3_finalize(_get_stmt);
    sqlite3_finalize(_exists_stmt);
    sqlite3_finalize(_pos_stmt);
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
    _pos_stmt = NULL;
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

} // namespace tagspace
