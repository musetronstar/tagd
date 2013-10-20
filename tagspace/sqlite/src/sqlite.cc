#include <iostream>
#include <sstream>
#include <iomanip> // setw
#include <cstring> // strcmp
#include <assert.h>
#include <vector>

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
	if (strcmp(err, "columns subject, relator, object are not unique") == 0)
		return TS_SQLITE_UNIQUE;

	if (strcmp(err, "columns refers, refers_to are not unique") == 0)
		return TS_SQLITE_UNIQUE;

	if (strcmp(err, "columns refers, context are not unique") == 0)
		return TS_SQLITE_UNIQUE;

	if (strcmp(err, "columns refers, context = NULL are not unique") == 0)
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

    if (_code == tagd::TAGD_OK)
        this->create_referents_table();

    if (_code == tagd::TAGD_OK)
        this->create_context_stack_table();

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

tagd::code sqlite::create_referents_table() {
    // check db
    sqlite3_stmt *stmt = NULL; 
    this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE referents%'",
        "referents table exists"
    );
	OK_OR_RET_ERR();

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "check referents table error");
    
    // table exists
    if (s_rc == SQLITE_ROW)
        return tagd::TAGD_OK;

    //create db
    this->exec(
    "CREATE TABLE referents ("
        "refers     TEXT NOT NULL, "
        "refers_to  TEXT NOT NULL, "
        "context    TEXT, "
        // whatever statement throws a unique constraint will fail
        // but the trasaction overall can still succeed
        "PRIMARY KEY (refers, refers_to) ON CONFLICT FAIL, " 
		"UNIQUE(refers, context), "
        "FOREIGN KEY (refers_to) REFERENCES tags(tag), "
        "FOREIGN KEY (context) REFERENCES tags(tag)"
    ")" 
    );
	OK_OR_RET_ERR();

	// UNIQUE index on (refers, context) only enforces non-NULL
	// values, so we have to create our own check
	// Only one distinct refers value can refer_to a NULL context
	this->exec(
		"CREATE TRIGGER trg_refers "
		"BEFORE INSERT ON referents "
		"WHEN NEW.context IS NULL "
		"BEGIN "
		"SELECT RAISE(FAIL, 'columns refers, context = NULL are not unique') "
		"WHERE EXISTS("
		  "SELECT 1 FROM referents "
		  "WHERE refers = NEW.refers "
		  "AND context IS NULL"
		 "); "
		"END "
	);
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_refers ON referents(refers)");
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_refers_to ON referents(refers_to)");

    return _code;
}

tagd::code sqlite::create_context_stack_table() {
    //create db
    this->exec(
    "CREATE TEMP TABLE context_stack ( "
        "ctx_elem     TEXT PRIMARY KEY NOT NULL "
		// enabling the foreign key will produce an error
		// on INSERTs when using a ':memory:' database  : no such table: temp.tags
		// TODO submit a bug report
        //"FOREIGN KEY(ctx_elem) REFERENCES tags(tag)"
    ")" 
    );
	OK_OR_RET_ERR();

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
        "PRIMARY KEY (subject, relator, object) ON CONFLICT FAIL, " // makes unique
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
        return this->error(ts_rc, "PRAGMA foreign_keys failed: %s", _db_fname.c_str());

	ts_rc = this->exec("PRAGMA temp_store = MEMORY");
    if (ts_rc != tagd::TAGD_OK)
        return this->error(ts_rc, "PRAGMA temp_store failed: %s", _db_fname.c_str());

    return this->code(tagd::TAGD_OK);
}

void sqlite::close() {
    if (_db == NULL)
        return;

    this->finalize();
    sqlite3_close(_db);
    _db = NULL;
}

tagd::code sqlite::get(tagd::abstract_tag& t, const tagd::id_type& id, const flags_t& flags) {

	const int F_REFERS_TO = 0;
	const int F_CONTEXT = 1;

	// 1. if context is is set, do context lookup
	//    use refers_to, set _referent to id
	tagd::id_type refers_to;
	if (!(flags & NO_REFERENTS) && !_context.empty()) {
		// get the top most element on the context stack
		// that matches the referent 
		this->prepare(&_get_referent_context_stmt,
			"SELECT refers_to, context, context_id "
			"FROM ("
				"SELECT refers_to, context, context_stack.ROWID AS context_id "
				"FROM referents, tags, context_stack "
				"WHERE refers = ? "
				"AND context = tag "
				"AND rank GLOB ("
				 "SELECT rank FROM tags WHERE tag = ctx_elem"
				") || '*' " // all context <= {_context}
				"UNION "
				// in cases where a refers and a tag would match, we have to select
				// the tag to see if it is the closer match
				"SELECT tag AS refers_to, NULL AS context, context_stack.ROWID AS context_id "
				"FROM tags, context_stack "
				"WHERE tag = ? "
				"AND rank GLOB ("
				 "SELECT rank FROM tags WHERE tag = ctx_elem"
				") || '*' " // all context <= {_context}
			") "
			"ORDER BY context_id DESC "
			"LIMIT 1",
			"select referents context"
		);
		OK_OR_RET_ERR();

		this->bind_text(&_get_referent_context_stmt, 1, id.c_str(), "bind referent refers");
		OK_OR_RET_ERR();

		this->bind_text(&_get_referent_context_stmt, 2, id.c_str(), "bind referent tag");
		OK_OR_RET_ERR();

		if (sqlite3_step(_get_referent_context_stmt) == SQLITE_ROW) {
			// NULL context means a tag is a closer match that any
			// referents of the same value, so ignore the referent
			// and do a regular lookup (i.e. empty refers_to)
			if (sqlite3_column_type(_get_referent_context_stmt, F_CONTEXT) != SQLITE_NULL) {
				t.relation(
					HARD_TAG_REFERENT,
					id,
					(const char*) sqlite3_column_text(_get_referent_context_stmt, F_CONTEXT)
				);
				refers_to = (const char*) sqlite3_column_text(_get_referent_context_stmt, F_REFERS_TO);
			}
		}
	}
	
	// 2. regular lookup
    this->prepare(&_get_stmt,
        "SELECT tag, super, pos, rank "
        "FROM tags WHERE tag = ?",
        "select tags"
    );
	OK_OR_RET_ERR(); 

	if (!refers_to.empty())
		this->bind_text(&_get_stmt, 1, refers_to.c_str(), "get tag refers_to");
	else
		this->bind_text(&_get_stmt, 1, id.c_str(), "get tag id");
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
        t.rank( (const char*) sqlite3_column_text(_get_stmt, F_RANK) );

        this->get_relations(t.relations, id);
        if (!(_code == tagd::TAGD_OK || _code == tagd::TS_NOT_FOUND)) return _code;

        return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_ERR, "get failed: %s", id.c_str());
    }

	if (flags & NO_REFERENTS)
		return this->code(tagd::TS_NOT_FOUND);

	// 3. NOT FOUND, and context not set
	//    Return referent with NULL context if it exists
	//    Otherwise, return ambiguous referents as an error

    this->prepare(&_get_referents_stmt,
        "SELECT refers_to, context "
        "FROM referents "
		"LEFT JOIN tags ON context = tag "
		"WHERE refers = ? "
		"ORDER BY rank",
        "select referents"
    );
	OK_OR_RET_ERR();

	this->bind_text(&_get_referents_stmt, 1, id.c_str(), "get referent");
	OK_OR_RET_ERR();

	typedef std::vector<tagd::referent> refs_t;
	refs_t refs;

    while ((s_rc = sqlite3_step(_get_referents_stmt)) == SQLITE_ROW) {
		// this will be the first row if it exists
        if (sqlite3_column_type(_get_referents_stmt, F_CONTEXT) == SQLITE_NULL) {
			t.relation(HARD_TAG_REFERENT, id);
			return this->get(
					t,
					(const char*) sqlite3_column_text(_get_referents_stmt, F_REFERS_TO),
					(flags|NO_REFERENTS)
				);
        } else {
			refs.push_back(
				tagd::referent(
					id,
					(const char*) sqlite3_column_text(_get_referents_stmt, F_REFERS_TO),
					(const char*) sqlite3_column_text(_get_referents_stmt, F_CONTEXT)
				)
			);
        }
    }

	if (refs.size() == 0) {
		return this->code(tagd::TS_NOT_FOUND);
	} else if (refs.size() == 1) {
		// In most cases, it would be more convenient to return
		// this tag, but it would be inconsistent handling contexts this way,
		// and probably introduce subtle logic errors for users.
		// For example, say there is  one referent (perro, dog, spanish) and 
		// a user looks up perro (no context), and we return dog
		// and the user builds an application depending on it returning that tag. 
		// Then another user adds a referent (perro, lazy, spanish_slang).
		// Now the lookup for perro (no context) returns TS_AMBIGUOUS, which could
		// break the first users application.
		this->error(tagd::TS_AMBIGUOUS, "%s refers to a tag with no matching context", id.c_str(), refs.size());
		this->last_error().relation(HARD_TAG_REFERS_TO, refs[0].refers_to(), refs[0].context());
		return _code;
	} else {
		this->error(tagd::TS_AMBIGUOUS, "%s refers to %d contexts", id.c_str(), refs.size());
		for (refs_t::iterator it = refs.begin(); it != refs.end(); ++it)
			this->last_error().relation(HARD_TAG_REFERS_TO, it->refers_to(), it->context());
		return _code;
	}
}

tagd::code sqlite::get(tagd::url& u, const tagd::id_type& id, const flags_t& flags) {
	// id should be a canonical url
	u.init(id);
	if (!u.ok())
		return this->error(u.code(), "url init failed: %s", id.c_str());

	// we use hduri to identify urls internally
	tagd::id_type hduri = u.hduri();
	this->get((tagd::abstract_tag&)u, hduri, flags);
	OK_OR_RET_ERR();

	u.init_hduri(hduri);  // id() should now be canonical url
	if (!u.ok())
		return this->error(u.code(), "url init_hduri failed: %s", hduri.c_str());

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
tagd::code sqlite::put(const tagd::abstract_tag& t, const flags_t& flags) {
    if (t.id().length() > tagd::MAX_TAG_LEN)
        return this->error(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (t.id().empty())
		return this->error(tagd::TS_ERR, "inserting empty tag not allowed");

	if (t.id()[0] == '_' && !_doing_init)
		return this->error(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", t.id().c_str()); 

	if (!(flags & NO_POS_CAST)) {
		switch (t.pos()) {
			case tagd::POS_URL:
				return this->put((const tagd::url&)t, flags);
			case tagd::POS_REFERENT:
				return this->put((const tagd::referent&)t, flags);
			default:
				; // NOOP
		}
	}

    tagd::abstract_tag existing;
    tagd::code existing_rc = this->get(existing, t.id(), flags);

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
    tagd::code dest_rc = this->get(destination, t.super(), flags);

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

tagd::code sqlite::put(const tagd::url& u, const flags_t& flags) {
	if (!u.ok())
		return this->error(u.code(), "put url not ok(%s): %s",  tagd_code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hduri
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hduri());
	tagd::url::insert_url_part_relations(t.relations, u);
	return this->put(t, (flags|NO_POS_CAST));
}

tagd::code sqlite::put(const tagd::referent& r, const flags_t& flags) {
	return this->insert_referent(r, flags);
}

tagd::code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& super) {
    assert( !super.rank().empty() || (super.rank().empty() && super.id() == "_entity") );

    tagd::rank_set R;
    this->child_ranks(R, super.id());
    OK_OR_RET_ERR(); 

    if (R.empty()) { // create first child element
        next = super.rank();
        next.push_back(1);
        return this->code(tagd::TAGD_OK);
    }

    tagd::code r_rc = tagd::rank::next(next, R);
    if (r_rc != tagd::TAGD_OK)
        return this->error(tagd::TS_ERR, "next_rank error: %s", tagd_code_str(r_rc));

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

tagd::code sqlite::insert_referent(const tagd::referent& t, const flags_t& flags) {
    assert( !t.refers().empty() );
    assert( !t.refers_to().empty() );
	assert( flags < 32 );  // use flags here to suppress unused param warning

    this->prepare(&_insert_referents_stmt,
        "INSERT INTO referents (refers, refers_to, context) "
        "VALUES (?, ?, ?)",
        "insert referents"
    );
    OK_OR_RET_ERR(); 

	this->bind_text(&_insert_referents_stmt, 1, t.refers().c_str(), "insert refers");
	OK_OR_RET_ERR(); 

	this->bind_text(&_insert_referents_stmt, 2, t.refers_to().c_str(), "insert refers_to");
	OK_OR_RET_ERR(); 

	if (t.context().empty())
		this->bind_null(&_insert_referents_stmt, 3, "insert NULL context");
	else
		this->bind_text(&_insert_referents_stmt, 3, t.context().c_str(), "insert context");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_insert_referents_stmt);

	if (s_rc == SQLITE_DONE) 
		return this->code(tagd::TAGD_OK);

	if (s_rc != SQLITE_CONSTRAINT)
		return this->error(tagd::TS_ERR, "insert referents failed: %s, %s, %s",
								t.refers().c_str(), t.refers_to().c_str(), t.context().c_str());

	assert(s_rc == SQLITE_CONSTRAINT);	

	// sqlite does't tell us whether the context or refers_to caused the violation
	const char* errmsg = sqlite3_errmsg(_db);
	ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
	if (ts_sql_rc == TS_SQLITE_UNIQUE) // context UNIQUE violation
		return this->code(tagd::TS_DUPLICATE);

	if (ts_sql_rc == TS_SQLITE_FK) {
		this->exists(t.refers_to());
		switch ( _code ) {
			case tagd::TS_NOT_FOUND:
				return this->error(tagd::TS_REFERS_TO_UNK, "unknown refers_to: %s", t.refers_to().c_str());
			case tagd::TAGD_OK:
				break;
			default:
				return _code; // error
		}

		if (!t.context().empty()) {
			// referents.context FK tags.tag violated
			this->exists(t.context());
			switch ( _code ) {
				case tagd::TS_NOT_FOUND:
					return this->error(tagd::TS_CONTEXT_UNK, "unknown context: %s", t.context().c_str());
				case tagd::TAGD_OK:
					break;
				default:
					return _code;
			}
		}

		return this->error(tagd::TS_INTERNAL_ERR, "foreign key constraint: %s", errmsg);
	}

	// TS_SQLITE_UNK
	return this->error(tagd::TS_INTERNAL_ERR, "insert referents error: %s", errmsg);
}

tagd_code sqlite::push_context(const tagd::id_type& id) {
	this->insert_context(id);
	if (_code == tagd::TAGD_OK)
		return tagspace::push_context(id);

	return this->error(tagd::TS_INTERNAL_ERR, "push_context failed: %s", id.c_str());
}

tagd_code sqlite::pop_context() {
	if (_context.empty()) return tagd::TAGD_OK;

	if (this->delete_context(_context[_context.size()-1]) == tagd::TAGD_OK)
		return tagspace::pop_context();

	return this->error(tagd::TS_INTERNAL_ERR, "pop_context failed: %s", _context[_context.size()-1].c_str());
}

tagd_code sqlite::clear_context() {
	if (_context.empty()) return tagd::TAGD_OK;

    this->prepare(&_truncate_context_stmt,
        "DELETE FROM context_stack; ",
		"DELETE FROM sqlite_sequence WHERE name = 'context_stack'"  // otherwise ROWID won't reset and could exceed it max
        "truncate context"
    );
	OK_OR_RET_ERR();

    if (sqlite3_step(_truncate_context_stmt) != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "clear context failed");

	return tagspace::clear_context();	
}

tagd::code sqlite::insert_context(const tagd::id_type& id) {
    assert( !id.empty() );

    this->prepare(&_insert_context_stmt,
        "INSERT INTO context_stack (ctx_elem) VALUES (?)",
        "insert context"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_insert_context_stmt, 1, id.c_str(), "insert context");
    OK_OR_RET_ERR(); 
 
    if (sqlite3_step(_insert_context_stmt) != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "insert context failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_context(const tagd::id_type& id) {
    assert( !id.empty() );

    this->prepare(&_delete_context_stmt,
        "DELETE FROM context_stack WHERE ctx_elem = ?",
        "delete context"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_context_stmt, 1, id.c_str(), "delete context");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_context_stmt);
    if (s_rc != SQLITE_DONE)
        return this->error(tagd::TS_ERR, "delete context failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
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
        rank.init( (const char*) sqlite3_column_text(stmt, F_RANK));
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);

        tagd::abstract_tag *t;
		if (pos != tagd::POS_URL) {
			t = new tagd::abstract_tag( (const char*) sqlite3_column_text(stmt, F_SUBJECT) );
		} else {
			t = new tagd::url();
			((tagd::url*)t)->init_hduri( (const char*) sqlite3_column_text(stmt, F_SUBJECT) );
			if (t->code() != tagd::TAGD_OK) {
				this->error( t->code(), "failed to init related url: %s",
						(const char*) sqlite3_column_text(stmt, F_SUBJECT) );
				delete t;
				return this->code();
			}
		}

		t->super( (const char*) sqlite3_column_text(stmt, F_SUPER) );
		t->pos(pos);
		t->rank(rank);

		if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				(const char*) sqlite3_column_text(stmt, F_OBJECT),
				(const char*) sqlite3_column_text(stmt, F_MODIFIER)
			);
		} else {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				(const char*) sqlite3_column_text(stmt, F_OBJECT)
			);
		}

        it = R.insert(it, *t);
		delete t;
    }

    if (s_rc == SQLITE_ERROR) {
		return this->error(tagd::TS_ERR, "related failed(super, relator, object, modifier): %s, %s, %s, %s",
							super.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str());
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::get_children(tagd::tag_set& R, const tagd::id_type& super) {
	this->prepare(&_get_children_stmt,
        "SELECT tag, super, pos, rank "
        "FROM tags WHERE super = ? "
		"ORDER BY rank",
        "select children"
    );
	OK_OR_RET_ERR(); 

	this->bind_text(&_get_children_stmt, 1, super.c_str(), "get children super");
	OK_OR_RET_ERR(); 

    const int F_ID = 0;
    const int F_SUPER = 1;
    const int F_POS = 2;
    const int F_RANK = 3;

	R.clear();
    tagd::tag_set::iterator it = R.begin();
    int s_rc;

    while ((s_rc = sqlite3_step(_get_children_stmt)) == SQLITE_ROW) {
		tagd::abstract_tag t(
			(const char*) sqlite3_column_text(_get_children_stmt, F_ID),
        	(const char*) sqlite3_column_text(_get_children_stmt, F_SUPER),
			(tagd::part_of_speech) sqlite3_column_int(_get_children_stmt, F_POS)
		);
        t.rank( (const char*) sqlite3_column_text(_get_children_stmt, F_RANK) );

        it = R.insert(it, t);
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query_referents(tagd::tag_set& R, const tagd::interrogator& intr) {
    sqlite3_stmt *stmt = NULL;
	tagd::id_type refers, refers_to, context;

    for (tagd::predicate_set::const_iterator it = intr.relations.begin(); it != intr.relations.end(); ++it) {
        if (it->relator == HARD_TAG_REFERS)
            refers = it->object;
		else if (it->relator == HARD_TAG_REFERS_TO)
            refers_to = it->object;
		else if (it->relator == HARD_TAG_CONTEXT)
            context = it->object;
    }

	if (refers.empty()) {
		if (refers_to.empty()) {
			if (context.empty()) {
				// all referents
				this->prepare(&stmt,
					"SELECT refers, refers_to, context FROM referents",
					"query referent context"
				);
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = ?"
					 ") || '*' " // all context <= {context}
					")",

					"query referent context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, context.c_str(), "context");
				OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context.empty()) {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers_to = ?",
					"query referent refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers_to = ? "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = ?"
					 ") || '*' " // all context <= {context}
					")",
					"query referent refers_to, context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, context.c_str(), "context");
				OK_OR_RET_ERR();
			}
		}
	} else {  // !refers.empty()
		if (refers_to.empty()) {
			if (context.empty()) {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers = ?",
					"query referent refers"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers = ? "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = ?"
					 ") || '*' " // all context <= {context}
					")",
					"query referent refers context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, context.c_str(), "context");
				OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context.empty()) {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers = ? "
					"AND refers_to = ?",
					"query referent refers, refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT refers, refers_to, context "
					"FROM referents "
					"WHERE refers = ? "
					"AND refers_to = ? "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = ?"
					 ") || '*' " // all context <= {context}
					")",
					"query referent refers, refers_to, context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 3, context.c_str(), "context");
				OK_OR_RET_ERR();
			}
		}
	}

    const int F_REFERS = 0;
    const int F_REFERS_TO = 1;
    const int F_CONTEXT = 2;

    R.clear();
    int s_rc;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        tagd::referent r(
            (const char*) sqlite3_column_text(stmt, F_REFERS),
            (const char*) sqlite3_column_text(stmt, F_REFERS_TO)
        );

		if (sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL)
			r.context( (const char*) sqlite3_column_text(stmt, F_CONTEXT) );

		// std::cerr << "query_referents: " << r << std::endl;
        R.insert(r);
    }

	if (stmt != NULL)
		sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
		return this->error(tagd::TS_ERR, "query referents failed(refers, refers_to, context): %s, %s, %s",
							refers.c_str(), refers_to.c_str(), context.c_str());
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query(tagd::tag_set& R, const tagd::interrogator& intr) {
    //TODO use the id (who, what, when, where, why, how_many...)
    // to distinguish types of queries

	if (intr.super() == HARD_TAG_REFERENT)
		return this->query_referents(R, intr);

    if (intr.relations.empty()) {
        if (intr.super().empty())
            return tagd::TS_NOT_FOUND;  // empty interrogator, nothing to do
        else
            return this->get_children(R, intr.super());
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
	os << std::setw(colw) << std::left << "-- id"
	   << std::setw(colw) << std::left << "relator"
	   << std::setw(colw) << std::left << "super"
	   << std::setw(colw) << std::left << "pos"
	   << std::setw(colw) << std::left << "rank"
	   << std::endl;
	os << std::setw(colw) << std::left << "-----"
	   << std::setw(colw) << std::left << "-------"
	   << std::setw(colw) << std::left << "-----"
	   << std::setw(colw) << std::left << "---"
	   << std::setw(colw) << std::left << "----"
	   << std::endl;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_ID) 
           << std::setw(colw) << std::left
           << tagd::super_relator(pos)
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUPER)
           << std::setw(colw) << std::left << pos_str(pos)
           << std::setw(colw) << std::left
           << tagd::rank::dotted_str( (const char*) sqlite3_column_text(stmt, F_RANK))
           << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_INTERNAL_ERR, "dump grid failed");
    }

    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT refers, refers_to, context FROM referents ORDER BY refers",
        "dump referent grid"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_REFERS = 0;
    const int F_REFERS_TO = 1;
    const int F_CONTEXT = 2;

	os << std::endl;
	os << std::setw(colw) << std::left << "-- refers"
	   << std::setw(colw) << std::left << "relator"
	   << std::setw(colw) << std::left << "refers_to"
	   << std::setw(colw) << std::left << "context"
	   << std::endl;
	os << std::setw(colw) << std::left << "---------"
	   << std::setw(colw) << std::left << "-------"
	   << std::setw(colw) << std::left << "---------"
	   << std::setw(colw) << std::left << "-------"
	   << std::endl;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_REFERS) 
           << std::setw(colw) << std::left
           << tagd::super_relator(tagd::POS_REFERENT)
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_REFERS_TO);

		if (sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL)
			os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_CONTEXT); 

        os << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump referent grid failed");

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
				os << "PUT " << *t << std::endl << std::endl; 
				delete t;
			}

			t = new tagd::abstract_tag(
					id,
					(const char*) sqlite3_column_text(stmt, F_SUPER),
					(tagd::part_of_speech) sqlite3_column_int(_get_stmt, F_POS) );
		}
    }

	if (t != NULL) {
		os << "PUT " << *t << std::endl << std::endl;
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
				os << "PUT " << *t << std::endl << std::endl; 
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
		os << "PUT " << *t << std::endl;
		delete t;
	}

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump relations failed");

    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT refers, refers_to, context "
        "FROM referents "
        "ORDER BY refers",
        "dump referents"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_REFERS = 0;
    const int F_REFERS_TO = 1;
	const int F_CONTEXT = 2;

	tagd::referent *r = NULL;
	tagd::id_type refers;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		refers = (const char*) sqlite3_column_text(stmt, F_REFERS);

		if (r == NULL || r->id() != refers) {
			if (r != NULL) {
				os << std::endl << "PUT " << *r << std::endl; 
				delete r;
			}

			if (sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL) {
				r = new tagd::referent(
					refers,
					(const char*) sqlite3_column_text(stmt, F_REFERS_TO),
					(const char*) sqlite3_column_text(stmt, F_CONTEXT) );
			} else {
				r = new tagd::referent(
					refers,
					(const char*) sqlite3_column_text(stmt, F_REFERS_TO) );
			}
		}
    }

	if (r != NULL) {
		os << std::endl << "PUT " << *r << std::endl;
		delete r;
	}

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump referents failed");

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
    tagd::code rc;

    while ((s_rc = sqlite3_step(_child_ranks_stmt)) == SQLITE_ROW) {
        rc = rank.init( (const char*) sqlite3_column_text(_child_ranks_stmt, F_RANK));
        switch (rc) {
            case tagd::TAGD_OK:
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
                return this->error(tagd::TS_ERR, "get_childs rank.init() error: %s", tagd_code_str(rc));
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
    sqlite3_finalize(_insert_referents_stmt);
    sqlite3_finalize(_insert_context_stmt);
    sqlite3_finalize(_delete_context_stmt);
    sqlite3_finalize(_truncate_context_stmt);
    sqlite3_finalize(_get_relations_stmt);
    sqlite3_finalize(_get_referents_stmt);
    sqlite3_finalize(_get_referent_context_stmt);
    sqlite3_finalize(_related_stmt);
    sqlite3_finalize(_related_null_super_stmt);
    sqlite3_finalize(_get_children_stmt);

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
    _insert_referents_stmt = NULL;
    _insert_context_stmt = NULL;
    _delete_context_stmt = NULL;
    _truncate_context_stmt = NULL;
    _get_relations_stmt = NULL;
    _get_referents_stmt = NULL;
    _get_referent_context_stmt = NULL;
    _related_stmt = NULL;
    _related_null_super_stmt = NULL;
	_get_children_stmt = NULL;
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
