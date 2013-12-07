#include <iostream>
#include <sstream>
#include <iomanip> // setw
#include <cstring> // strcmp
#include <assert.h>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdarg>

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

auto f_passthrough = [](const tagd::id_type& id) -> tagd::id_type { return id; };

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

#define OK_OR_ROLLBACK_RET_ERR() if(_code != tagd::TAGD_OK) { \
		this->finalize(); \
		tagd::code c = _code; \
		this->exec("ROLLBACK"); \
		return c; \
	}

#define OK_OR_RET_POS_UNKNOWN() if(_code != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

namespace tagspace {

sqlite::~sqlite() {
    this->close();
}

void sqlite::trace_on() {
	if (_db != nullptr)
		sqlite3_trace(_db, trace_callback, NULL);
    dout << "### sqlite trace on ###" << std::endl;
	_trace_on = true;
}

void sqlite::trace_off() {
	sqlite3_trace(_db, NULL, NULL);
    dout << "### sqlite trace off ###" << std::endl;
	_trace_on = false;
}

tagd::code sqlite::init(const std::string& fname) {
	_doing_init = true;
	this->_init(fname);
	_doing_init = false;

	//TODO this line of code is not needed
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

	if (_trace_on) // run again now that _db set
		this->trace_on();	

    this->exec("BEGIN");

    if (_code == tagd::TAGD_OK)
		this->create_terms_table();

    if (_code == tagd::TAGD_OK)
		this->create_tags_table();

    if (_code == tagd::TAGD_OK)
        this->create_relations_table();

    if (_code == tagd::TAGD_OK)
        this->create_referents_table();

    if (_code == tagd::TAGD_OK)
        this->create_context_stack_table();

	// We have to insert _entity and _super manually because of the FK on _super_relator
	// The rest of the hard tags will be inserted by bootstrap
	bool init_hard_tags = false;
    if (_code == tagd::TAGD_OK) {
		if (this->exists(HARD_TAG_ENTITY) == tagd::TS_NOT_FOUND) {
			this->exec_mprintf(
				"INSERT INTO terms (ROWID, term, term_pos) "
				"VALUES (1, '%s', %d)",
				HARD_TAG_ENTITY, tagd::POS_TAG
			);
			init_hard_tags = true;
		}

		if ( _code == tagd::TAGD_OK &&
			this->exists(HARD_TAG_SUPER) == tagd::TS_NOT_FOUND )
		{
			this->exec_mprintf(
				"INSERT INTO terms (ROWID, term, term_pos) "
				"VALUES (2, '%s', %d)",
				HARD_TAG_SUPER, tagd::POS_SUPER_RELATOR
			);
			init_hard_tags = true;
		}

		if ( _code == tagd::TAGD_OK && init_hard_tags) {
			this->exec_mprintf(
				"INSERT INTO tags (tag, super_relator, super_object, rank, pos) "
				"VALUES (tid('%s'), tid('%s'), tid('%s'), NULL, %d)",
				HARD_TAG_ENTITY, HARD_TAG_SUPER, HARD_TAG_ENTITY, tagd::POS_TAG
			);
		}
	}

    if (_code == tagd::TAGD_OK && init_hard_tags)
		bootstrap::init_hard_tags(*this);

    if (_code != tagd::TAGD_OK) {
		tagd::code ts_rc = _code;
        this->exec("ROLLBACK");
		return this->code(ts_rc);
    }

    this->exec("COMMIT");

	// enforce foreign keys after inserting  _entity _super _entity
    if (_code == tagd::TAGD_OK) {
		// must be executed outside of transaction
		this->exec("PRAGMA foreign_keys = ON");
		if (_code != tagd::TAGD_OK)
			return this->ferror(tagd::TS_INTERNAL_ERR, "PRAGMA foreign_keys failed: %s", _db_fname.c_str());
	}

    return _code;
}



tagd::code sqlite::create_terms_table() {

    auto f_tid = [](
				sqlite3_context *context,
				int argc,
				sqlite3_value **argv
			) { 
		assert( argc==1 );
		if( sqlite3_value_type(argv[0])==SQLITE_NULL ) {
			sqlite3_result_null(context);
			return;
		}

		tagd::id_type term(
			(const char*)sqlite3_value_text(argv[0]),
			sqlite3_value_bytes(argv[0])
		);

		// sqlite3 *db = sqlite3_context_db_handle(context);

		sqlite *ts = (sqlite*)sqlite3_user_data(context);
		rowid_t term_id;
		tagd::part_of_speech pos = ts->term_pos(term, &term_id);
		if (pos == tagd::POS_UNKNOWN) {
			sqlite3_result_null(context);
		} else {
			sqlite3_result_int64(context, term_id);
		}
	};

	auto f_idt = [](
					sqlite3_context *context,
					int argc,
					sqlite3_value **argv
				) {
		assert( argc==1 );
		if( sqlite3_value_type(argv[0])==SQLITE_NULL ) {
			sqlite3_result_null(context);
			return;
		}

		sqlite3_int64 term_id = sqlite3_value_int64(argv[0]);
		sqlite *ts = (sqlite*)sqlite3_user_data(context);

		std::string term;
		tagd::part_of_speech pos = ts->term_id_pos(term_id, &term);
		if (pos == tagd::POS_UNKNOWN) {
			sqlite3_result_null(context);
			return;
		}

		// can't use SQLITE_STATIC because term data destroyed before results accessed
		sqlite3_result_text(context, term.data(), term.size(), SQLITE_TRANSIENT);
	};

	int rc;
	rc = sqlite3_create_function(_db, "tid", 1, SQLITE_UTF8, this, f_tid, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function tid failed");

	rc = sqlite3_create_function(_db, "idt", 1, SQLITE_UTF8, this, f_idt, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function idt failed");

    // check db
    sqlite3_stmt *stmt = NULL; 
    this->prepare(&stmt,
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' "
        "AND sql LIKE 'CREATE TABLE terms%'",
        "terms table exists"
    );
	OK_OR_RET_ERR();

    int s_rc = sqlite3_step(stmt);
    if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_INTERNAL_ERR, "check terms table error");
    }
    
    // table exists
    if (s_rc == SQLITE_ROW) {
        if (this->term_pos("_entity") != tagd::POS_UNKNOWN)
            return tagd::TAGD_OK; // db already initialized
    }

    this->exec(
    "CREATE TABLE terms ( "
        "term TEXT PRIMARY KEY NOT NULL, "
        "term_pos INTEGER NOT NULL"
    ")" 
    );
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_term ON terms(term)");
	OK_OR_RET_ERR();

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
        this->get(t, "_entity", NO_TRANSFORM_REFERENTS);
		OK_OR_RET_ERR();

        if (t.id() == "_entity" && t.super_object() == "_entity")
            return tagd::TAGD_OK; // db already initialized
    
        return this->error(tagd::TS_INTERNAL_ERR, "tag table exists but has no _entity row; database corrupt");
    }

    //create db

    this->exec(
    "CREATE TABLE tags ( "
        "tag     INTEGER PRIMARY KEY NOT NULL, "
        "super_relator   INTEGER NOT NULL, "
        "super_object   INTEGER NOT NULL, "
        // binary collation defeats LIKE wildcard partial indexes,
        // but enables indexing for GLOB style patterns
        "rank    TEXT UNIQUE COLLATE BINARY, "
        "pos     INTEGER NOT NULL, "
        "FOREIGN KEY(super_relator) REFERENCES tags(tag), "
        "FOREIGN KEY(super_object) REFERENCES tags(tag), "
        // _entity is the only self referential tag (_entity = _entity)
        // and the only tag allowed to have rank IS NULL (indicating the root)
        "CHECK ( "
            "(tag = 1 AND super_object = 1 AND rank IS NULL) OR "
            "(tag <> 1 AND pos <> 0 AND rank IS NOT NULL) "  // _entity == 1,  0 == POS_UNKNOWN
        ")"
    ")" 
    );
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_super_object ON tags(super_object)");
	OK_OR_RET_ERR();

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
        "refers     INTEGER NOT NULL, "
        "refers_to  INTEGER NOT NULL, "
        "context    INTEGER, "
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
        "ctx_elem INTEGER PRIMARY KEY NOT NULL, "
        "stack_level INTEGER NOT NULL "
		// enabling the foreign key will produce an error
		// on INSERTs when using a ':memory:' database  : no such table: temp.tags
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
        "subject   INTEGER NOT NULL, "
        "relator   INTEGER NOT NULL, "
        "object    INTEGER NOT NULL, "
        "modifier  INTEGER, "
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

	// relator index added because of use in _term_pos_occurence_stmt
	// TODO look into optimizing
	this->exec("CREATE INDEX idx_relator ON relations(relator)");
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_object ON relations(object)");
	OK_OR_RET_ERR();

	// relator index added because of use in _term_pos_occurence_stmt
	// TODO look into optimizing
	this->exec("CREATE INDEX idx_modifier ON relations(modifier)");

    return _code;
}

tagd::code sqlite::open() {
    if (_db != NULL)
		return this->code(tagd::TAGD_OK);

    int rc = sqlite3_open(_db_fname.c_str(), &_db);
    if( rc != SQLITE_OK ){
        this->close();
        return this->ferror(tagd::TS_INTERNAL_ERR, "open database failed: %s", _db_fname.c_str());
    }

	this->exec("PRAGMA temp_store = MEMORY");
    if (_code != tagd::TAGD_OK)
        return this->ferror(tagd::TS_INTERNAL_ERR, "PRAGMA temp_store failed: %s", _db_fname.c_str());

    return this->code(tagd::TAGD_OK);
}

void sqlite::close() {
    if (_db == NULL)
        return;

    this->finalize();
    sqlite3_close(_db);
    _db = NULL;
}

tagd::code sqlite::get(tagd::abstract_tag& t, const tagd::id_type& term, flags_t flags) {

	tagd::part_of_speech term_pos = this->term_pos(term);
	// std::cerr << "term_pos(" << term << "): " << pos_list_str(term_pos) << std::endl;

	if (term_pos == tagd::POS_UNKNOWN) {
		if (flags & NO_NOT_FOUND_ERROR) {
			return this->code(tagd::TS_NOT_FOUND);
		} else {
			return this->error(tagd::TS_NOT_FOUND,
				tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term) );
		}
	}

	tagd::id_type id;
	if (flags & NO_TRANSFORM_REFERENTS) {
		id = term;
	} else if (term_pos & tagd::POS_REFERS) {
		this->decode_referent(id, term);
	} else {
		id = term;
	}

    this->prepare(&_get_stmt,
        "SELECT idt(tag), idt(super_relator), idt(super_object), pos, rank "
        "FROM tags WHERE tag = tid(?)",
        "select tags"
    );
	OK_OR_RET_ERR(); 

	this->bind_text(&_get_stmt, 1, id.c_str(), "get tag id");
	OK_OR_RET_ERR(); 

    const int F_ID = 0;
    const int F_SUPER_REL = 1;
    const int F_SUPER_OBJ = 2;
    const int F_POS = 3;
    const int F_RANK = 4;

	id_transform_func_t f_transform =
		((flags & NO_TRANSFORM_REFERENTS) ?  f_passthrough : _f_encode_referent);

    int s_rc = sqlite3_step(_get_stmt);
    if (s_rc == SQLITE_ROW) {
        t.id( f_transform((const char*) sqlite3_column_text(_get_stmt, F_ID)) );
        t.super_relator( f_transform((const char*) sqlite3_column_text(_get_stmt, F_SUPER_REL)) );
        t.super_object( f_transform((const char*) sqlite3_column_text(_get_stmt, F_SUPER_OBJ)) );
        t.pos( (tagd::part_of_speech) sqlite3_column_int(_get_stmt, F_POS) );
        t.rank( (const char*) sqlite3_column_text(_get_stmt, F_RANK) );

        this->get_relations(t.relations, id, flags);
        if (!(_code == tagd::TAGD_OK || _code == tagd::TS_NOT_FOUND)) return _code;

		// if id was transformed via referent, add a _refers_to the orignal id
		// don't transform _refers_to because it we want it to be accessible
		// universally and programmatically (the id will be easily recognizable
		// as a referent due to the presence of a _refers_to predicate)
		if (!(flags & NO_TRANSFORM_REFERENTS)) {
			if (id != t.id())
				t.relation(HARD_TAG_REFERS_TO, id);
		} 

    } else if (s_rc == SQLITE_ERROR) {
        return this->ferror(tagd::TS_ERR, "get failed: %s", id.c_str());
    } else {
		if (term_pos & tagd::POS_REFERS)
			return this->ferror(tagd::TS_AMBIGUOUS,
				"%s refers to a tag with no matching context", term.c_str());
		else {
			if (flags & NO_NOT_FOUND_ERROR) {
				return this->code(tagd::TS_NOT_FOUND);
			} else {
				return this->error(tagd::TS_NOT_FOUND,
					tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term) );
			}
		}
	}

	return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::get(tagd::url& u, const tagd::id_type& id, flags_t flags) {
	// id should be a canonical url
	u.init(id);
	if (!u.ok())
		return this->ferror(u.code(), "url init failed: %s", id.c_str());

	// we use hduri to identify urls internally
	tagd::id_type hduri = u.hduri();
	this->get((tagd::abstract_tag&)u, hduri, flags);
	OK_OR_RET_ERR();

	u.init_hduri(hduri);  // id() should now be canonical url
	if (!u.ok())
		return this->ferror(u.code(), "url init_hduri failed: %s", hduri.c_str());

	return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::get_relations(tagd::predicate_set& P, const tagd::id_type& id, flags_t flags) {

    this->prepare(&_get_relations_stmt,
        "SELECT idt(relator), idt(object), idt(modifier) "
        "FROM relations WHERE subject = tid(?)",
        "select relations"
    );
	OK_OR_RET_ERR();

	this->bind_text(&_get_relations_stmt, 1, id.c_str(), "get subject");
	OK_OR_RET_ERR();

    const int F_RELATOR = 0;
    const int F_OBJECT = 1;
    const int F_MODIFIER = 2;

	id_transform_func_t f_transform =
		((flags & NO_TRANSFORM_REFERENTS) ?  f_passthrough : _f_encode_referent);

    int s_rc;
    while ((s_rc = sqlite3_step(_get_relations_stmt)) == SQLITE_ROW) {
		auto p = tagd::make_predicate(
			f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_RELATOR) ),
			f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_OBJECT) )
		);
        if (sqlite3_column_type(_get_relations_stmt, F_MODIFIER) != SQLITE_NULL) {
			p.modifier = f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_MODIFIER) );
        }
		P.insert(p);
    } 

    if (s_rc == SQLITE_ERROR)
        return this->ferror(tagd::TS_ERR, "get relations failed: %s", id.c_str());

    if (P.empty())
		return this->code(tagd::TS_NOT_FOUND);

    return this->code(tagd::TAGD_OK);
}

tagd::part_of_speech sqlite::term_pos(const tagd::id_type& id, rowid_t *term_id) {
	assert(id.length() <= tagd::MAX_TAG_LEN);

    tagd::code ts_rc = this->prepare(&_term_pos_stmt,
        "SELECT ROWID, term_pos FROM terms WHERE term = ?",
        "term term_pos"
    );
	if (ts_rc != tagd::TAGD_OK)
		this->print_errors();
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    ts_rc = this->bind_text(&_term_pos_stmt, 1, id.c_str(), "term");
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    const int F_TERM_ID = 0;
    const int F_TERM_POS = 1;

    int s_rc = sqlite3_step(_term_pos_stmt);
    if (s_rc == SQLITE_ROW) {
		if (term_id != NULL)
			*term_id = sqlite3_column_int64(_term_pos_stmt, F_TERM_ID);
		return (tagd::part_of_speech) sqlite3_column_int(_term_pos_stmt, F_TERM_POS);
    } else if (s_rc == SQLITE_ERROR) {
        this->ferror(tagd::TS_INTERNAL_ERR, "term_pos failed: %s", id.c_str());
        return tagd::POS_UNKNOWN;
    } else {
        return tagd::POS_UNKNOWN;
    }
}

tagd::part_of_speech sqlite::term_id_pos(rowid_t term_id, std::string *term) {

    tagd::code ts_rc = this->prepare(&_term_id_pos_stmt,
        "SELECT term, term_pos FROM terms WHERE ROWID = ?",
        "term term_pos"
    );
	if (ts_rc != tagd::TAGD_OK)
		this->print_errors();
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    ts_rc = this->bind_rowid(&_term_id_pos_stmt, 1, term_id, "term_id");
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    const int F_TERM = 0;
    const int F_TERM_POS = 1;

    int s_rc = sqlite3_step(_term_id_pos_stmt);
    if (s_rc == SQLITE_ROW) {
		if (term != NULL)
			*term = (const char*)sqlite3_column_text(_term_id_pos_stmt, F_TERM);
		return (tagd::part_of_speech) sqlite3_column_int(_term_id_pos_stmt, F_TERM_POS);
    } else if (s_rc == SQLITE_ERROR) {
        this->ferror(tagd::TS_INTERNAL_ERR, "term_id_pos failed: %ld", term_id);
        return tagd::POS_UNKNOWN;
    } else {
        return tagd::POS_UNKNOWN;
    }
}

tagd::part_of_speech sqlite::pos(const tagd::id_type& id, flags_t flags) {
	assert(id.length() <= tagd::MAX_TAG_LEN);


	tagd::id_type refers_to;
	if (flags & NO_TRANSFORM_REFERENTS)
		refers_to = id;
	else
		this->refers_to(refers_to, id);  // refers_to set if id refers to it (in context)

    tagd::code ts_rc = this->prepare(&_pos_stmt,
        "SELECT pos FROM tags WHERE tag = tid(?)",
        "tag pos"
    );
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    ts_rc = this->bind_text(&_pos_stmt, 1, (refers_to.empty() ? id.c_str() : refers_to.c_str()), "pos id");
    assert(ts_rc == tagd::TAGD_OK);
    if (ts_rc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

    const int F_POS = 0;

    int s_rc = sqlite3_step(_pos_stmt);
    if (s_rc == SQLITE_ROW) {
		return (tagd::part_of_speech) sqlite3_column_int(_pos_stmt, F_POS);
    } else if (s_rc == SQLITE_ERROR) {
        this->ferror(tagd::TS_INTERNAL_ERR, "pos failed: %s", (refers_to.empty() ? id.c_str() : refers_to.c_str()));
        return tagd::POS_UNKNOWN;
    } else {
        return tagd::POS_UNKNOWN;
    }
}

tagd_code sqlite::refers(tagd::id_type &refers, const tagd::id_type& refers_to) {
	assert(!refers_to.empty());
	assert(refers_to.length() <= tagd::MAX_TAG_LEN);

    this->prepare(&_refers_stmt,
		"SELECT idt(refers) FROM ("
		 "SELECT refers, stack_level "
		 "FROM referents, tags, context_stack "
		 "WHERE refers_to = tid(?) "
		 "AND context = tag "
		 "AND rank GLOB ("
		  "SELECT rank FROM tags WHERE tag = ctx_elem"
		 ") || '*' " // all context <= {_context}
		 "UNION "
		 "SELECT refers, 0 as stack_level "
		 "FROM referents "
		 "WHERE refers_to = tid(?) "
		 "AND context IS NULL"
		") "
		"ORDER BY stack_level DESC "
		"LIMIT 1",
        "refers"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_refers_stmt, 1, refers_to.c_str(), "refers_to context");
    OK_OR_RET_ERR();

    this->bind_text(&_refers_stmt, 2, refers_to.c_str(), "refers_to universal context");
    OK_OR_RET_ERR();

    const int F_REFERS = 0;

    int s_rc = sqlite3_step(_refers_stmt);
    if (s_rc == SQLITE_ROW) {
		refers = (const char *) sqlite3_column_text(_refers_stmt, F_REFERS);
		return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->ferror(tagd::TS_INTERNAL_ERR, "refers failed: %s", refers_to.c_str());
    } else {
        return this->code(tagd::TS_NOT_FOUND);
    }
}

tagd_code sqlite::refers_to(tagd::id_type &refers_to, const tagd::id_type& refers) {
	assert(!refers.empty());
	assert(refers.length() <= tagd::MAX_TAG_LEN);

    this->prepare(&_refers_to_stmt,
		"SELECT idt(refers_to) FROM ("
		 "SELECT refers_to, 0 AS tag_context, context_stack.stack_level "
		 "FROM referents, tags, context_stack "
		 "WHERE refers = tid(?) "
		 "AND context = tag "
		 "AND rank GLOB ("
		  "SELECT rank FROM tags WHERE tag = ctx_elem"
		 ") || '*' " // all context <= {_context}
		 "UNION "
		 "SELECT refers_to, 0 AS tag_context, 0 AS stack_level "
		 "FROM referents "
		 "WHERE refers = tid(?) "
		 "AND context IS NULL "
		 "UNION "
		 "SELECT tag AS refers_to, 1 AS tag_context, 0 AS stack_level "
		 "FROM tags, context_stack "
		 "WHERE tag = tid(?) "
		 "AND rank GLOB ("
		  "SELECT rank FROM tags WHERE tag = ctx_elem"
		 ") || '*' " // all context <= {_context}
		") "
		"ORDER BY tag_context DESC, stack_level DESC ",
		//"LIMIT 1",
        "refers_to"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_refers_to_stmt, 1, refers.c_str(), "refers context");
    OK_OR_RET_ERR();

    this->bind_text(&_refers_to_stmt, 2, refers.c_str(), "refers universal context");
    OK_OR_RET_ERR();

    this->bind_text(&_refers_to_stmt, 3, refers.c_str(), "tag in context");
    OK_OR_RET_ERR();

    const int F_REFERS_TO = 0;

    int s_rc = sqlite3_step(_refers_to_stmt);
    if (s_rc == SQLITE_ROW) {
		refers_to = (const char *) sqlite3_column_text(_refers_to_stmt, F_REFERS_TO);
		return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->ferror(tagd::TS_INTERNAL_ERR, "refers_to failed: %s", refers.c_str());
    } else {
        return this->code(tagd::TS_NOT_FOUND);
    }
}

tagd::code sqlite::exists(const tagd::id_type& id) {
    this->prepare(&_exists_stmt,
        "SELECT 1 FROM tags WHERE tag = tid(?)",
        "tag exists"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_exists_stmt, 1, id.c_str(), "exists statement id");
    OK_OR_RET_ERR();

    int s_rc = sqlite3_step(_exists_stmt);
    if (s_rc == SQLITE_ROW) {
        return this->code(tagd::TAGD_OK);
    } else if (s_rc == SQLITE_ERROR) {
        return this->ferror(tagd::TS_ERR, "exists failed: %", id.c_str());
    } else {
        return this->code(tagd::TS_NOT_FOUND);
    }
}

// tagd::TS_NOT_FOUND returned if destination undefined
tagd::code sqlite::put(const tagd::abstract_tag& put_tag, flags_t flags) {
    if (put_tag.id().length() > tagd::MAX_TAG_LEN)
        return this->ferror(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (put_tag.id().empty())
		return this->error(tagd::TS_ERR, "inserting empty tag not allowed");

	if (put_tag.id()[0] == '_' && !_doing_init)
		return this->ferror(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", put_tag.id().c_str());

	if (!(flags & NO_POS_CAST)) {
		switch (put_tag.pos()) {
			case tagd::POS_URL:
				return this->put((const tagd::url&)put_tag, flags);
			case tagd::POS_REFERENT:
				return this->put((const tagd::referent&)put_tag, flags);
			default:
				; // NOOP
		}
	}

	tagd::abstract_tag t;
	if (flags & NO_TRANSFORM_REFERENTS) {
		t = put_tag;
	} else {
		this->decode_referents(t, put_tag);
	}
/*
	if ( t != put_tag ) {
		std::cerr << "put transform before: " << put_tag << std::endl;
		std::cerr << " put transform after: " << t << std::endl;
	}
*/

	if (t.id() == t.super_object() && t.id() != "_entity")
		return this->ferror(tagd::TS_MISUSE, "_id == _super_object not allowed: %s", t.id().c_str()); 

    tagd::abstract_tag existing;
    tagd::code existing_rc = this->get(existing, t.id(), (flags|NO_TRANSFORM_REFERENTS|NO_NOT_FOUND_ERROR));

    if (existing_rc != tagd::TAGD_OK && existing_rc != tagd::TS_NOT_FOUND)
        return existing_rc; // err set by get

    if (t.super_object().empty()) {
        if (existing_rc == tagd::TS_NOT_FOUND) {
            // can't do anything without knowing what a tag is
			return this->error(tagd::TS_SUPER_UNK,
				tagd::make_predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, t.id()) );
        } else {
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return this->ferror(tagd::TS_MISUSE, "cannot put a tag without relations: %s", t.id().c_str());
            else // insert relations
                return this->insert_relations(t);
        }
    }

    tagd::abstract_tag destination;
    tagd::code dest_rc = this->get(destination, t.super_object(), (flags|NO_TRANSFORM_REFERENTS));

    if (dest_rc != tagd::TAGD_OK) {
        if (dest_rc == tagd::TS_NOT_FOUND)
            return this->ferror(tagd::TS_SUPER_UNK, "unknown super_object: %s", t.super_object().c_str());
        else
            return dest_rc; // err set by get
    }

    // handle duplicate tags up-front
    tagd::code ins_upd_rc;
    if (existing_rc == tagd::TAGD_OK) {  // existing tag
        if ( t.super_relator() == existing.super_relator() &&
		     t.super_object() == existing.super_object() )
		{  // same location
            if (t.relations.empty())  // duplicate tag and no relations to insert 
                return this->ferror(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
            else // insert relations
                return this->insert_relations(t);
        }

		// use existing because it has the rank
		if (existing.super_relator() != t.super_relator())
			existing.super_relator(t.super_relator());
        // move existing to new location or relator
        ins_upd_rc = this->update(existing, destination);
		//t = existing;
    } else if (existing_rc == tagd::TS_NOT_FOUND) {
        // new tag
        ins_upd_rc = this->insert(t, destination);
    } else {
        assert(false); // should never get here
        return this->ferror(tagd::TS_INTERNAL_ERR, "put failed: %s", t.id().c_str());
    }

    if (ins_upd_rc == tagd::TAGD_OK && !t.relations.empty())
        return this->insert_relations(t);

    // res from insert/update, (errors will have been set)
    return this->code(ins_upd_rc);
}

tagd::code sqlite::put(const tagd::url& u, flags_t flags) {
	if (!u.ok())
		return this->ferror(u.code(), "put url not ok(%s): %s",  tagd_code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hduri
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hduri());
	tagd::url::insert_url_part_relations(t.relations, u);
	return this->put(t, (flags|NO_POS_CAST));
}

tagd::code sqlite::put(const tagd::referent& r, flags_t flags) {
	return this->insert_referent(r, flags);
}

tagd::code sqlite::del(const tagd::abstract_tag& t, flags_t flags) {
    if (t.id().length() > tagd::MAX_TAG_LEN)
        return this->ferror(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (t.id().empty())
		return this->error(tagd::TS_ERR, "deleting empty tag not allowed");

	if (t.id()[0] == '_' && !_doing_init)
		return this->ferror(tagd::TS_MISUSE, "deleting hard tags not allowed: %s", t.id().c_str());

	if (!(flags & NO_POS_CAST)) {
		switch (t.pos()) {
			case tagd::POS_URL:
				return this->del((const tagd::url&)t, flags);
			case tagd::POS_REFERENT:
				return this->del((const tagd::referent&)t, flags);
			default:
				; // NOOP
		}
	}

	if (!t.super_object().empty() /*TODO && !(flags && IGNORE_SUPER) */) {
		return this->ferror(tagd::TS_MISUSE,
			"super must not be specified when deleting tag: %s", t.id().c_str());
	}

	tagd::abstract_tag del_tag;
	if (flags & NO_TRANSFORM_REFERENTS) {
		del_tag = t;
	} else {
		this->decode_referents(del_tag, t);
	}

    tagd::abstract_tag existing;
    tagd::code existing_rc = this->get(existing, del_tag.id(), (flags|NO_TRANSFORM_REFERENTS));

    if (existing_rc == tagd::TS_NOT_FOUND) {
        return existing_rc; // err set by get
	}
	OK_OR_RET_ERR();

	// make a set off all terms affected, so we can update the term pos after deleting tag
	std::set<tagd::id_type> terms_affected;

	auto f_term_affected = [&terms_affected](const tagd::id_type& term) mutable {
		if (!term.empty())
			terms_affected.insert(term);
	};

	auto f_tag_affected = [&f_term_affected](const tagd::abstract_tag& t) {
		f_term_affected(t.id());
		f_term_affected(t.super_relator());
		f_term_affected(t.super_object());
		for ( auto p : t.relations ) {
			f_term_affected(p.relator);
			f_term_affected(p.object);
			f_term_affected(p.modifier);
		}
	};

    this->exec("BEGIN");

	if (del_tag.relations.empty()) {
		// delete referents, relations, and tag
		this->delete_refers_to(del_tag.id());
		OK_OR_ROLLBACK_RET_ERR();

		this->delete_relations(del_tag.id());  // all relations given subject
		OK_OR_ROLLBACK_RET_ERR();

		this->delete_tag(del_tag.id());
		OK_OR_ROLLBACK_RET_ERR();

		// referents affected
		tagd::tag_set R;
        tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
        q_refers_to.relation(HARD_TAG_REFERS_TO, del_tag.id());
        this->query(R, q_refers_to);
		if (_code != tagd::TS_NOT_FOUND) {
			OK_OR_ROLLBACK_RET_ERR();
			std::for_each(R.begin(), R.end(), f_tag_affected);
		}

		f_tag_affected(existing);
	} else {
		// delete only del_tag.relations
		for( auto p : del_tag.relations ) {
			if (!existing.related(p)) {
				if (p.modifier.empty()) {
					this->ferror(tagd::TS_NOT_FOUND,
						"cannot delete non-existent relation: %s %s %s",
							del_tag.id().c_str(), p.relator.c_str(), p.object.c_str()); 
				} else {
					this->ferror(tagd::TS_NOT_FOUND,
						"cannot delete non-existent relation: %s %s %s = %s",
							del_tag.id().c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str()); 
				}
			}
		}
		OK_OR_ROLLBACK_RET_ERR();

		this->delete_relations(del_tag.id(), del_tag.relations);
		OK_OR_ROLLBACK_RET_ERR();

		f_tag_affected(del_tag);
	}

	for ( auto id : terms_affected ) {
		this->update_pos_occurence(id);
		OK_OR_ROLLBACK_RET_ERR();
	}

    this->exec("COMMIT");

	return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::del(const tagd::url& u, flags_t flags) {
	if (!u.ok())
		return this->ferror(u.code(), "del url not ok(%s): %s",  tagd_code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hduri
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hduri());
	t.super_relator(tagd::id_type());
	t.super_object(tagd::id_type());
	// don't insert url part relations
	return this->del(t, (flags|NO_POS_CAST));
}

tagd::code sqlite::del(const tagd::referent& r, flags_t flags) {
	assert(!flags);  // suppress unused param warning for now

    sqlite3_stmt *stmt = NULL;

	if (r.refers().empty()) {
		if (r.refers_to().empty()) {
			if (r.context().empty()) {
				// don't allow deleting all referents
				return this->error(tagd::TS_MISUSE, "deleting all referents not allowed");
			} else {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE context = tid(?)",
					/* //TODO implement RECURSIVE flag to delete all contained contexts
					"WHERE context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					  "SELECT rank FROM tags WHERE tag = tid(?)"
					 ") || '*' " // all context <= {context}
					")",
					*/
					"delete referent context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.context().c_str(), "context");
				OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (r.context().empty()) {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers_to = tid(?)",
					"delete referent refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers_to().c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers_to = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ") || '*' " // all context <= {context}
					")",
					*/
					"delete referent refers_to, context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers_to().c_str(), "refers_to");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.context().c_str(), "context");
				OK_OR_RET_ERR();
			}
		}
	} else {  // !refers.empty()
		if (r.refers_to().empty()) {
			if (r.context().empty()) {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?)",
					"delete referent refers"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ") || '*' " // all context <= {context}
					")",
					*/
					"delete referent refers context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.context().c_str(), "context");
				OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context().empty()) {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?)",
					"delete referent refers, refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.refers_to().c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ") || '*' " // all context <= {context}
					")",
					*/
					"delete referent refers, refers_to, context"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.refers_to().c_str(), "refers_to");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 3, r.context().c_str(), "context");
				OK_OR_RET_ERR();
			}
		}
	}

    int s_rc = sqlite3_step(stmt);
 	if (stmt != NULL)
		sqlite3_finalize(stmt);
    
    if (s_rc != SQLITE_DONE)
		return this->ferror(tagd::TS_ERR, "delete referent failed: %s", r.str().c_str());

	if (sqlite3_changes(_db) == 0)
		return this->ferror(tagd::TS_NOT_FOUND, "delete referent not found: %s", r.str().c_str());

	return this->code(tagd::TAGD_OK);
}

tagd::part_of_speech sqlite::term_pos_occurence(const tagd::id_type& id, bool set_fk_err) {
	if (_term_pos_occurence_stmt == NULL) {
		// TODO there is probably a more optimal way of doing this
		// I know, WTF, but when in doubt, use brute force
		std::stringstream ss;
		ss << "SELECT pos FROM tags WHERE tag = tid(?) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_SUPER_RELATOR << " FROM tags WHERE super_relator = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_SUPER_OBJECT << " FROM tags WHERE super_object = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_SUBJECT << " FROM relations WHERE subject = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_RELATED << " FROM relations WHERE relator = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_OBJECT << " FROM relations WHERE object = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_MODIFIER << " FROM relations WHERE modifier = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_REFERS << " FROM referents WHERE refers = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_REFERS_TO << " FROM referents WHERE refers_to = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_CONTEXT << " FROM referents WHERE context = tid(?) LIMIT 1)";
		this->prepare(&_term_pos_occurence_stmt, ss.str().c_str(), "pos occurence statement");
	} else {
		this->prepare(&_term_pos_occurence_stmt, NULL, "pos occurence statement");
	}
	OK_OR_RET_POS_UNKNOWN();

	int i = 1;
	this->bind_text(&_term_pos_occurence_stmt, i, id.c_str(), "tag pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "super_relator pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "super_object pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "subject pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "relator pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "object pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "modifier pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "refers pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "refers_to pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "context pos occurence");
	OK_OR_RET_POS_UNKNOWN();

	const int F_POS = 0;

	int s_rc;
	tagd::part_of_speech occurence_pos = tagd::POS_UNKNOWN;
	while ((s_rc = sqlite3_step(this->_term_pos_occurence_stmt)) == SQLITE_ROW) {
		// occurence_pos |= pos is prettier, but give invalid conversion error
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(this->_term_pos_occurence_stmt, F_POS);
		//std::cerr << id << ": " << pos_list_str(occurence_pos) << " |= " << pos_str(pos) << std::endl;
		occurence_pos = ((tagd::part_of_speech)(occurence_pos | pos));

		// using this method to set error for the cause of FK constraint failures
		if (set_fk_err) {
			// TODO causes will need to be defined as hard tags
			// if we ever want to insert errors in a tagspace
			tagd::id_type cause;
			switch(pos) {
				case tagd::POS_SUPER_RELATOR:
					cause = "super_relator";
					break;
				case tagd::POS_SUPER_OBJECT:
					cause = "super_object";
					break;
				case tagd::POS_SUBJECT:
					cause = "subject";
					break;
				case tagd::POS_RELATED:
					cause = "related";
					break;
				case tagd::POS_OBJECT:
					cause = "object";
					break;
				case tagd::POS_MODIFIER:
					cause = "modifier";
					break;
				// no FK on refers
				// case tagd::POS_REFERS:
				// 	cause = "refers";
				// 	break;
				case tagd::POS_REFERS_TO:
					cause = "refers_to";
					break;
				case tagd::POS_CONTEXT:
					cause = "context";
					break;
				default:
					; // not a cause of FK constraint
			}

			if (!cause.empty()) {
				this->error(tagd::TS_FOREIGN_KEY,
					tagd::make_predicate(HARD_TAG_CAUSED_BY, cause, id) );
			}
		}
	}
	//std::cerr << "occurence_pos(" << id << "): " << pos_list_str(occurence_pos) << std::endl;

	if (s_rc == SQLITE_ERROR)
		this->ferror(tagd::TS_ERR, "term pos occurence failed: %s", id.c_str());

	return occurence_pos;
}

tagd_code sqlite::update_pos_occurence(const tagd::id_type& id) {
	tagd::part_of_speech occurence_pos = this->term_pos_occurence(id);
	OK_OR_RET_ERR();

	return ( occurence_pos == tagd::POS_UNKNOWN
		? this->delete_term(id) : this->update_term(id, occurence_pos) );
}

tagd::code sqlite::delete_refers_to(const tagd::id_type& id) {
    assert( !id.empty() );

    this->prepare(&_delete_refers_to_stmt,
        "DELETE FROM referents WHERE refers_to = tid(?)",
        "delete refers_to"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_refers_to_stmt, 1, id.c_str(), "delete refers_to");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_refers_to_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "delete refers_to failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_relations(const tagd::id_type& subject) {
    assert( !subject.empty() );

    this->prepare(&_delete_subject_relations_stmt,
        "DELETE FROM relations WHERE subject = tid(?)",
        "delete relations"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_subject_relations_stmt, 1, subject.c_str(), "delete subject relations");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_subject_relations_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "delete subject relations failed: %s", subject.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_relations(const tagd::id_type& subject, const tagd::predicate_set& P) {
    assert( !subject.empty() );
    assert( !P.empty() );

	for (auto p : P) {
		this->prepare(&_delete_relation_stmt,
			"DELETE FROM relations "
			"WHERE subject = tid(?) "
			"AND relator = tid(?) "
			"AND object = tid(?)",
			"delete relations"
		);
		OK_OR_RET_ERR();

		this->bind_text(&_delete_relation_stmt, 1, subject.c_str(), "delete relation subject");
		OK_OR_RET_ERR();

		this->bind_text(&_delete_relation_stmt, 2, p.relator.c_str(), "delete relation relator");
		OK_OR_RET_ERR();
		
		this->bind_text(&_delete_relation_stmt, 3, p.object.c_str(), "delete relation object");
		OK_OR_RET_ERR();

		if (sqlite3_step(_delete_relation_stmt) != SQLITE_DONE) {
			return this->ferror(tagd::TS_ERR,
				"delete relation failed: %s %s %s",
					subject.c_str(), p.relator.c_str(), p.object.c_str()); 
		}
	}

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_tag(const tagd::id_type& id) {
    assert( !id.empty() );

    this->prepare(&_delete_tag_stmt,
        "DELETE FROM tags WHERE tag = tid(?)",
        "delete tag"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_tag_stmt, 1, id.c_str(), "delete tag");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_tag_stmt);
	if (s_rc == SQLITE_DONE) {
		return this->code(tagd::TAGD_OK);
	} else if (s_rc == SQLITE_CONSTRAINT) {
		if (_trace_on) {
			const char* errmsg = sqlite3_errmsg(_db);
			std::cerr << "SQLITE_CONSTRAINT: " << errmsg << std::endl;
		}
		
		// set error for cause of FK constraint failure
		this->term_pos_occurence(id, true);

		assert(_code == tagd::TS_FOREIGN_KEY);
		if (!_code == tagd::TS_FOREIGN_KEY)  // unknown cause of FK constraint failure, should't happen
			return this->ferror(tagd::TS_ERR, "delete tag unknown constraint failure: %s", id.c_str());
		else  // error(s) set
			return _code;
	} else {
        return this->ferror(tagd::TS_ERR, "delete tag failed: %s", id.c_str());
	}
}

tagd::code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& super) {
    assert( !super.rank().empty() || (super.rank().empty() && super.id() == "_entity") );

    tagd::rank_set R;
	// TODO, this will be extremely wasteful for large set
	// instead:
	//   SELECT the max child rank
	//   if rank->back() is a single ut8 byte (0-127)
	//     do child_ranks like we have been
	//   else
	//     increment the max child rank (don't loop through child ranks)
    this->child_ranks(R, super.id());
    OK_OR_RET_ERR(); 

    if (R.empty()) { // create first child element
        next = super.rank();
        next.push_back(1);
        return this->code(tagd::TAGD_OK);
    }

	// This will only fill holes in the rank set if the last utf8 code point
	// of the first item in the rank set is a single utf8 byte (0-127).
	// Otherwise, its too large to loop through looking for holes
	// and the last rank in the set +1 will be used.
    tagd::code r_rc = tagd::rank::next(next, R);
    if (r_rc != tagd::TAGD_OK)
        return this->ferror(tagd::TS_ERR, "next_rank error: %s", tagd_code_str(r_rc));

    return this->code(tagd::TAGD_OK);
}

tagd::part_of_speech sqlite::put_term(const tagd::id_type& t, const tagd::part_of_speech pos) {
    assert( !t.empty() );
	rowid_t term_id;
	tagd::part_of_speech p = this->term_pos(t, &term_id);
	if (p == tagd::POS_UNKNOWN) {
		this->insert_term(t, pos);
		return (this->ok() ? pos : tagd::POS_UNKNOWN);
	} else if (p == pos) {
		this->code(tagd::TS_DUPLICATE);
		return pos;
	} else {
		this->update_term(t, (tagd::part_of_speech)(p | pos));
		return (this->ok() ? (tagd::part_of_speech)(p | pos): tagd::POS_UNKNOWN);
	}
}

tagd::code sqlite::insert_term(const tagd::id_type& t, const tagd::part_of_speech pos) {
    assert( !t.empty() );

    this->prepare(&_insert_term_stmt,
        "INSERT INTO terms (term, term_pos) VALUES (?, ?)",
        "insert term"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_insert_term_stmt, 1, t.c_str(), "insert term");
    OK_OR_RET_ERR(); 
 
	this->bind_int(&_insert_term_stmt, 2, pos, "insert term_pos");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_insert_term_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "insert term failed: %s", t.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::update_term(const tagd::id_type& t, const tagd::part_of_speech pos) {
    assert( !t.empty() );

    this->prepare(&_update_term_stmt,
        "UPDATE terms SET term_pos = ? WHERE term = ? AND term_pos <> ?",
        "update term"
    );
    OK_OR_RET_ERR(); 
 
	this->bind_int(&_update_term_stmt, 1, pos, "update term_pos");
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_term_stmt, 2, t.c_str(), "update term");
    OK_OR_RET_ERR(); 

	this->bind_int(&_update_term_stmt, 3, pos, "update <> term_pos");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_update_term_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "update term failed: %s", t.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_term(const tagd::id_type& id) {
    assert( !id.empty() ); this->prepare(&_delete_term_stmt,
        "DELETE FROM terms WHERE term = ?",
        "delete term"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_term_stmt, 1, id.c_str(), "delete term");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_term_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "delete term failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::insert(const tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
    assert( t.super_object() == destination.id() );
    assert( !t.id().empty() );
    assert( !t.super_relator().empty() );
    assert( !t.super_object().empty() );

    tagd::rank rank;
    next_rank(rank, destination);
    OK_OR_RET_ERR(); 

    // dout << "insert next rank: (" << rank.dotted_str() << ")" << std::endl;

    this->prepare(&_insert_stmt,
        "INSERT INTO tags (tag, super_relator, super_object, rank, pos) "
		"VALUES (tid(?), tid(?), tid(?), ?, ?)",
        "insert tag"
    );
    OK_OR_RET_ERR(); 

	tagd::part_of_speech pos;
	if (t.pos() == tagd::POS_UNKNOWN) {
		// use pos of super
		pos = this->pos(t.super_object());
		assert(pos != tagd::POS_UNKNOWN);
	} else {
		pos = t.pos();
	}

	int i = 0;
	this->put_term(t.id(), pos);
    this->bind_text(&_insert_stmt, ++i, t.id().c_str(), "insert id");
    OK_OR_RET_ERR(); 
 
	this->put_term(t.super_relator(), tagd::POS_SUPER_RELATOR);
	this->bind_text(&_insert_stmt, ++i, t.super_relator().c_str(), "insert super_relator");
    OK_OR_RET_ERR(); 

	this->put_term(t.super_object(), tagd::POS_SUPER_OBJECT);
	this->bind_text(&_insert_stmt, ++i, t.super_object().c_str(), "insert super_object");
    OK_OR_RET_ERR(); 

	this->bind_text(&_insert_stmt, ++i, rank.c_str(), "insert rank");
    OK_OR_RET_ERR(); 

	this->bind_int(&_insert_stmt, ++i, pos, "insert pos");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_insert_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "insert tag failed: %s", t.id().c_str());

    return this->code(tagd::TAGD_OK);
}

// update existing with new tag
tagd::code sqlite::update(const tagd::abstract_tag& t,
                                      const tagd::abstract_tag& destination) {
    assert( !t.id().empty() );
    assert( !t.super_relator().empty() );
    assert( !t.super_object().empty() );
    assert( !destination.super_object().empty() );

	if (t.rank() != destination.rank()) {
		tagd::rank rank;
		next_rank(rank, destination);
		OK_OR_RET_ERR(); 

		// dout << "update next rank: (" << rank.dotted_str() << ")" << std::endl;

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

		this->bind_int(&_update_ranks_stmt, 2, (t.rank().size()+1), "rank size");
		OK_OR_RET_ERR(); 

		this->bind_text(&_update_ranks_stmt, 3, t.rank().c_str(), "sub rank");
		OK_OR_RET_ERR(); 

		int s_rc = sqlite3_step(_update_ranks_stmt);
		if (s_rc != SQLITE_DONE)
			return this->ferror(tagd::TS_ERR, "update rank failed: %s", t.id().c_str());
	}

    //update tag
    this->prepare(&_update_tag_stmt, 
            "UPDATE tags SET super_relator = tid(?), super_object = tid(?) WHERE tag = tid(?)",
            "update tag"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_tag_stmt, 1, t.super_relator().c_str(), "update super_relator");
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_tag_stmt, 2, destination.id().c_str(), "update super_object");
    OK_OR_RET_ERR(); 

    this->bind_text(&_update_tag_stmt, 3, t.id().c_str(), "update tag id");
    OK_OR_RET_ERR(); 

    int s_rc = sqlite3_step(_update_tag_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "update tag failed: %s", t.id().c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::insert_relations(const tagd::abstract_tag& t) {
    assert( !t.id().empty() );
    assert( !t.relations.empty() );

    this->prepare(&_insert_relations_stmt,
        "INSERT INTO relations (subject, relator, object, modifier) "
        "VALUES (tid(?), tid(?), tid(?), tid(?))",
        "insert relations"
    );
    OK_OR_RET_ERR(); 

    tagd::predicate_set::iterator it = t.relations.begin();
    if (it == t.relations.end())
        return this->error(tagd::TS_INTERNAL_ERR, "insert empty relations");

    // if all inserts were unique constraint violations, tagd::TS_DUPLICATE will be returned
    size_t num_inserted = 0;

    while (true) {
		this->put_term(t.id(), tagd::POS_SUBJECT);
        this->bind_text(&_insert_relations_stmt, 1, t.id().c_str(), "insert subject");
		OK_OR_RET_ERR(); 

		this->put_term(it->relator, tagd::POS_RELATED);
		this->bind_text(&_insert_relations_stmt, 2, it->relator.c_str(), "insert relator");
		OK_OR_RET_ERR(); 

		this->put_term(it->object, tagd::POS_OBJECT);
		this->bind_text(&_insert_relations_stmt, 3, it->object.c_str(), "insert object");
		OK_OR_RET_ERR(); 

		if (it->modifier.empty())
			this->bind_null(&_insert_relations_stmt, 4, "insert NULL modifier");
		else {
			this->put_term(it->modifier, tagd::POS_MODIFIER);
			this->bind_text(&_insert_relations_stmt, 4, it->modifier.c_str(), "insert modifier");
		}
		OK_OR_RET_ERR(); 


        int s_rc = sqlite3_step(_insert_relations_stmt);

        if (s_rc == SQLITE_DONE) {
            num_inserted++;
        } else if (s_rc == SQLITE_CONSTRAINT) {
            // sqlite does't tell us whether the object or relator caused the violation
			const char* errmsg = sqlite3_errmsg(_db);

			if (_trace_on)
				std::cerr << "SQLITE_CONSTRAINT: " << errmsg << std::endl;

            ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
            if (ts_sql_rc == TS_SQLITE_UNIQUE) { // object UNIQUE violation
                this->exists(it->relator);
                switch ( _code ) {
					case tagd::TS_NOT_FOUND:  return this->ferror(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
                    case tagd::TAGD_OK:         break;
                    default:            return _code; // error
                }
            } else if (ts_sql_rc == TS_SQLITE_FK) {
                // relations.object FK tags.tag violated
                this->exists(it->object);
                switch ( _code ) {
					case tagd::TS_NOT_FOUND:  return this->ferror(tagd::TS_OBJECT_UNK, "unknown object: %s", it->object.c_str());
					case tagd::TAGD_OK:  return this->ferror(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
                    default:            return _code;
                }
            } else {  // TS_SQLITE_UNK
                return this->ferror(tagd::TS_INTERNAL_ERR, "insert relations error: %s", errmsg);
            } 
        } else {
            return this->ferror(tagd::TS_ERR, "insert relations failed: %s, %s, %s",
									t.id().c_str(), it->relator.c_str(), it->object.c_str());
        }

        if (++it != t.relations.end()) {
            sqlite3_reset(_insert_relations_stmt);
            sqlite3_clear_bindings(_insert_relations_stmt);
        } else {
            break;
        }
    }  // while

	if (num_inserted == 0)
		return this->ferror(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
	else
		return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::insert_referent(const tagd::referent& put_ref, const flags_t& flags) {
    assert( !put_ref.refers().empty() );
    assert( !put_ref.refers_to().empty() );
	assert( flags < 32 );  // use flags here to suppress unused param warning

	tagd::referent t;
	if (flags & NO_TRANSFORM_REFERENTS) {
		t = put_ref;
	} else {
		this->decode_referents(t, put_ref);
		if (t.id() != put_ref.id())
			t.id(put_ref.id());  // don't transform the refers
	}

	if (t.refers() == t.refers_to())
		return this->error(tagd::TS_MISUSE, "_refers == _refers_to not allowed!"); 

    this->prepare(&_insert_referents_stmt,
        "INSERT INTO referents (refers, refers_to, context) "
        "VALUES (tid(?), tid(?), tid(?))",
        "insert referents"
    );
    OK_OR_RET_ERR(); 

	this->put_term(t.refers(), tagd::POS_REFERS);
	this->bind_text(&_insert_referents_stmt, 1, t.refers().c_str(), "insert refers");
	OK_OR_RET_ERR(); 

	this->put_term(t.refers_to(), tagd::POS_REFERS_TO);
	this->bind_text(&_insert_referents_stmt, 2, t.refers_to().c_str(), "insert refers_to");
	OK_OR_RET_ERR(); 

	if (t.context().empty())
		this->bind_null(&_insert_referents_stmt, 3, "insert NULL context");
	else {
		this->put_term(t.context(), tagd::POS_CONTEXT);
		this->bind_text(&_insert_referents_stmt, 3, t.context().c_str(), "insert context");
	}
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_insert_referents_stmt);
	
	if (s_rc == SQLITE_DONE) 
		return this->code(tagd::TAGD_OK);

	if (s_rc != SQLITE_CONSTRAINT)
		return this->ferror(tagd::TS_ERR, "insert referents failed: %s, %s, %s",
								t.refers().c_str(), t.refers_to().c_str(), t.context().c_str());

	assert(s_rc == SQLITE_CONSTRAINT);	

	// sqlite does't tell us whether the context or refers_to caused the violation
	const char* errmsg = sqlite3_errmsg(_db);

	if (_trace_on)
		std::cerr << "SQLITE_CONSTRAINT: " << errmsg << std::endl;

	ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
	if (ts_sql_rc == TS_SQLITE_UNIQUE) // referents UNIQUE violation
		return this->ferror(tagd::TS_DUPLICATE, "duplicate referent: %s", t.refers().c_str());

	if (ts_sql_rc == TS_SQLITE_FK) {
		this->exists(t.refers_to());
		switch ( _code ) {
			case tagd::TS_NOT_FOUND:
				return this->ferror(tagd::TS_REFERS_TO_UNK, "unknown refers_to: %s", t.refers_to().c_str());
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
					return this->ferror(tagd::TS_CONTEXT_UNK, "unknown context: %s", t.context().c_str());
				case tagd::TAGD_OK:
					break;
				default:
					return _code;
			}
		}

		return this->ferror(tagd::TS_INTERNAL_ERR, "foreign key constraint: %s", errmsg);
	}

	// TS_SQLITE_UNK
	return this->ferror(tagd::TS_INTERNAL_ERR, "insert referents error: %s", errmsg);
}

void sqlite::encode_referent(tagd::id_type &to, const tagd::id_type &from) {
	if (from.empty()) return;

	if (this->refers(to, from) == tagd::TAGD_OK)  // to set
		return;

	to = from;
}

void sqlite::encode_referents(tagd::predicate_set&to, const tagd::predicate_set&from) {
	for (auto it = from.begin(); it != from.end(); ++it) {
		tagd::predicate p;
		this->encode_referent(p.relator, it->relator);
		this->encode_referent(p.object, it->object);
		if (!it->modifier.empty())
			this->encode_referent(p.modifier, it->modifier);
		to.insert(p);
	}
}

void sqlite::encode_referents(tagd::abstract_tag&to, const tagd::abstract_tag&from) {
		// transform referents in put_tag to their "refers_to" given context
		to.pos(from.pos());

		tagd::id_type rt;
		if (!from.id().empty()) {
			this->encode_referent(rt, from.id());
			to.id(rt);
		}

		if (!from.super_object().empty()) {
			this->encode_referent(rt, from.super_object());
			to.super_object(rt);
		}

		if (!from.rank().empty()) {
			to.rank(from.rank());
		}

		this->encode_referents(to.relations, from.relations);
}


void sqlite::decode_referent(tagd::id_type &to, const tagd::id_type &from) {
	if (from.empty()) return;

	if (from[0] == '_') { // don't lookup hard tags
		to = from;
		return;
	}

	if (this->refers_to(to, from) == tagd::TAGD_OK)  // to set
		return;

	to = from;
}

void sqlite::decode_referents(tagd::predicate_set&to, const tagd::predicate_set&from) {
	for (auto it = from.begin(); it != from.end(); ++it) {
		tagd::predicate p;
		this->decode_referent(p.relator, it->relator);
		this->decode_referent(p.object, it->object);
		if (!it->modifier.empty())
			this->decode_referent(p.modifier, it->modifier);
		to.insert(p);
	}
}

void sqlite::decode_referents(tagd::abstract_tag&to, const tagd::abstract_tag&from) {
		// transform referents in put_tag to their "refers_to" given context
		to.pos(from.pos());

		tagd::id_type rt;
		if (!from.id().empty()) {
			this->decode_referent(rt, from.id());
			to.id(rt);
		}

		if (!from.super_relator().empty()) {
			this->decode_referent(rt, from.super_relator());
			to.super_relator(rt);
		}

		if (!from.super_object().empty()) {
			this->decode_referent(rt, from.super_object());
			to.super_object(rt);
		}

		this->decode_referents(to.relations, from.relations);
}

tagd_code sqlite::push_context(const tagd::id_type& id) {
	this->insert_context(id);
	if (_code == tagd::TAGD_OK)
		return tagspace::push_context(id);

	return this->ferror(tagd::TS_INTERNAL_ERR, "push_context failed: %s", id.c_str());
}

tagd_code sqlite::pop_context() {
	if (_context.empty()) return tagd::TAGD_OK;

	if (this->delete_context(_context[_context.size()-1]) == tagd::TAGD_OK)
		return tagspace::pop_context();

	return this->ferror(tagd::TS_INTERNAL_ERR, "pop_context failed: %s", _context[_context.size()-1].c_str());
}

tagd_code sqlite::clear_context() {
	if (_context.empty()) return tagd::TAGD_OK;

    this->prepare(&_truncate_context_stmt,
        "DELETE FROM context_stack",
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
        "INSERT INTO context_stack (ctx_elem, stack_level) "
		"VALUES (tid(?), (SELECT ifnull(max(stack_level), 0)+1 FROM context_stack))",
        "insert context"
    );
    OK_OR_RET_ERR(); 

    this->bind_text(&_insert_context_stmt, 1, id.c_str(), "insert context");
    OK_OR_RET_ERR(); 
 
    if (sqlite3_step(_insert_context_stmt) != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "insert context failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::delete_context(const tagd::id_type& id) {
    assert( !id.empty() );

    this->prepare(&_delete_context_stmt,
        "DELETE FROM context_stack WHERE ctx_elem = tid(?)",
        "delete context"
    );
    OK_OR_RET_ERR();

    this->bind_text(&_delete_context_stmt, 1, id.c_str(), "delete context");
    OK_OR_RET_ERR(); 
 
    int s_rc = sqlite3_step(_delete_context_stmt);
    if (s_rc != SQLITE_DONE)
        return this->ferror(tagd::TS_ERR, "delete context failed: %s", id.c_str());

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::related(tagd::tag_set& R, const tagd::predicate& rel, const tagd::id_type& sup, flags_t flags) {
	tagd::predicate p;
	tagd::id_type super_object;
	if (flags & NO_TRANSFORM_REFERENTS) {
		p = rel;
		super_object = sup;
	} else {
		this->decode_referent(super_object, sup);
		this->decode_referent(p.relator, rel.relator);
		this->decode_referent(p.object, rel.object);
		this->decode_referent(p.modifier, rel.modifier);
	}

    sqlite3_stmt *stmt;

    // _entity rank is NULL, so we have to treat it differently
    // subjects will not be filtered according to rank (as _entity is all enpcompassing)
    if (super_object == "_entity" || super_object.empty()) {
		if (p.modifier.empty()) {
			this->prepare(&_related_null_super_stmt,
				"SELECT idt(subject), idt(super_object), pos, rank, "
				"idt(relator), idt(object), idt(modifier) "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = tid(?) "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				 "SELECT tag FROM tags WHERE rank GLOB ( "
				  "SELECT rank FROM tags WHERE tag = tid(?) "
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
				"SELECT idt(subject), idt(super_object), pos, rank, "
				"idt(relator), idt(object), idt(modifier) "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = tid(?) "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				 "SELECT tag FROM tags WHERE rank GLOB ( "
				  "SELECT rank FROM tags WHERE tag = tid(?) "
				 ") || '*'" // all objects <= p.object
				") "
				"AND modifier = tid(?) "
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
				"SELECT idt(subject), idt(super_object), pos, rank, "
				"idt(relator), idt(object), idt(modifier) "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = tid(?) "
				") || '*'" // all subjects <= super_object
				") "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = tid(?) "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = tid(?) "
				") || '*'" // all objects <= p.object
				") "
				"ORDER BY rank",
				"select related"
			);
			OK_OR_RET_ERR();
	
			this->bind_text(&_related_stmt, 1, super_object.c_str(), "super_object");
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
				"SELECT idt(subject), idt(super_object), pos, rank, "
				"idt(relator), idt(object), idt(modifier) "
				"FROM relations, tags "
				"WHERE tag = subject "
				"AND subject IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = tid(?) "
				") || '*'" // all subjects <= super_object
				") "
				"AND ("
				 "? IS NULL OR "
				 "relator IN ( "
				  "SELECT tag FROM tags WHERE rank GLOB ( "
				   "SELECT rank FROM tags WHERE tag = tid(?) "
				  ") || '*'" // all relators <= p.relator
				 ") "
				") "
				"AND object IN ( "
				"SELECT tag FROM tags WHERE rank GLOB ( "
				"SELECT rank FROM tags WHERE tag = tid(?) "
				") || '*'" // all objects <= p.object
				") "
				"AND modifier = tid(?) "
				"ORDER BY rank",
				"select related"
			);
			OK_OR_RET_ERR();
	
			this->bind_text(&_related_modifier_stmt, 1, super_object.c_str(), "super_object");
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

	id_transform_func_t f_transform =
		((flags & NO_TRANSFORM_REFERENTS) ?  f_passthrough : _f_encode_referent);

    R.clear();
    tagd::tag_set::iterator it = R.begin();
    tagd::rank rank;
    int s_rc;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        rank.init( (const char*) sqlite3_column_text(stmt, F_RANK));
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);

        tagd::abstract_tag *t;
		if (pos != tagd::POS_URL) {
			t = new tagd::abstract_tag( f_transform((const char*) sqlite3_column_text(stmt, F_SUBJECT)) );
		} else {
			t = new tagd::url();
			((tagd::url*)t)->init_hduri( (const char*) sqlite3_column_text(stmt, F_SUBJECT) );
			if (t->code() != tagd::TAGD_OK) {
				this->ferror( t->code(), "failed to init related url: %s",
						(const char*) sqlite3_column_text(stmt, F_SUBJECT) );
				delete t;
				return this->code();
			}
		}

		t->super_object( f_transform((const char*) sqlite3_column_text(stmt, F_SUPER)) );
		t->pos(pos);
		t->rank(rank);

		auto pred = tagd::make_predicate(
			f_transform( (const char*) sqlite3_column_text(stmt, F_RELATOR) ),
			f_transform( (const char*) sqlite3_column_text(stmt, F_OBJECT) )
		);
		if (sqlite3_column_type(_get_relations_stmt, F_MODIFIER) != SQLITE_NULL) {
			pred.modifier = f_transform( (const char*) sqlite3_column_text(stmt, F_MODIFIER) );
		}
		t->relation(pred);

        it = R.insert(it, *t);
		delete t;
    }

    if (s_rc == SQLITE_ERROR) {
		return this->ferror(tagd::TS_ERR, "related failed(super_object, relator, object, modifier): %s, %s, %s, %s",
							super_object.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str());
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::get_children(tagd::tag_set& R, const tagd::id_type& super_object, flags_t flags) {
	this->prepare(&_get_children_stmt,
        "SELECT idt(tag), idt(super_relator), idt(super_object), idt(pos), idt(rank) "
        "FROM tags WHERE super_object = tid(?) "
		"ORDER BY rank",
        "select children"
    );
	OK_OR_RET_ERR(); 

	this->bind_text(&_get_children_stmt, 1, super_object.c_str(), "get children super_object");
	OK_OR_RET_ERR(); 

    const int F_ID = 0;
    const int F_SUPER_REL = 1;
    const int F_SUPER_OBJ = 2;
    const int F_POS = 3;
    const int F_RANK = 4;

	id_transform_func_t f_transform =
		((flags & NO_TRANSFORM_REFERENTS) ?  f_passthrough : _f_encode_referent);

	R.clear();
    tagd::tag_set::iterator it = R.begin();
    int s_rc;

    while ((s_rc = sqlite3_step(_get_children_stmt)) == SQLITE_ROW) {
		tagd::abstract_tag t(
			f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_ID)),
        	f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_SUPER_REL)),
        	f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_SUPER_OBJ)),
			(tagd::part_of_speech) sqlite3_column_int(_get_children_stmt, F_POS)
		);
        t.rank( (const char*) sqlite3_column_text(_get_children_stmt, F_RANK) );

        it = R.insert(it, t);
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query_referents(tagd::tag_set& R, const tagd::interrogator& intr) {
	tagd::id_type refers, refers_to, context;
    for (auto it = intr.relations.begin(); it != intr.relations.end(); ++it) {
        if (it->relator == HARD_TAG_REFERS)
            refers = it->object;
		else if (it->relator == HARD_TAG_REFERS_TO)
            refers_to = it->object;
		else if (it->relator == HARD_TAG_CONTEXT)
            context = it->object;
    }

    sqlite3_stmt *stmt = NULL;

	if (refers.empty()) {
		if (refers_to.empty()) {
			if (context.empty()) {
				// all referents
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents",
					"query referent context"
				);
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
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
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers_to = tid(?)",
					"query referent refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers_to = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
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
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?)",
					"query referent refers"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
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
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?)",
					"query referent refers, refers_to"
				);
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, refers_to.c_str(), "refers_to");
				OK_OR_RET_ERR();
			} else {
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB ("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
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

        auto pr = R.insert(r);
		assert( pr.second );
    }

	if (stmt != NULL)
		sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
		return this->ferror(tagd::TS_ERR, "query referents failed(refers, refers_to, context): %s, %s, %s",
							refers.c_str(), refers_to.c_str(), context.c_str());
    }

    return this->code(R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query(tagd::tag_set& R, const tagd::interrogator& intr, flags_t flags) {
    //TODO use the id (who, what, when, where, why, how_many...)
    // to distinguish types of queries

	if (intr.super_object() == HARD_TAG_REFERENT)
		return this->query_referents(R, intr);

    if (intr.relations.empty()) {
        if (intr.super_object().empty())
            return tagd::TS_NOT_FOUND;  // empty interrogator, nothing to do
        else
            return this->get_children(R, intr.super_object(), flags);
    }


	size_t n = 0;
    tagd::tag_set S;  // related per predicate
    for (tagd::predicate_set::const_iterator it = intr.relations.begin();
                it != intr.relations.end(); ++it) {

        S.clear();

		// super_relator is not used for finding related tags (its not advantagious)
        this->related(S, *it, intr.super_object());
		if (_trace_on) {
			std::cerr << "related: ";
			tagd::print_tag_ids(S);
		}
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
        "SELECT idt(tag), idt(super_relator), idt(super_object), rank, pos FROM tags ORDER BY rank",
        "dump grid"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER_REL = 1;
    const int F_SUPER_OBJ = 2;
    const int F_RANK = 3;
    const int F_POS = 4;

    int s_rc;
    const int colw = 20;
	os << std::setw(colw) << std::left << "-- id"
	   << std::setw(colw) << std::left << "relator"
	   << std::setw(colw) << std::left << "super_object"
	   << std::setw(colw) << std::left << "pos"
	   << std::setw(colw) << std::left << "rank"
	   << std::endl;
	os << std::setw(colw) << std::left << "-----"
	   << std::setw(colw) << std::left << "-------"
	   << std::setw(colw) << std::left << "------------"
	   << std::setw(colw) << std::left << "---"
	   << std::setw(colw) << std::left << "----"
	   << std::endl;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_ID) 
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUPER_REL)
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUPER_OBJ)
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
        "SELECT idt(refers), idt(refers_to), idt(context) "
		"FROM referents "
		"ORDER BY context",
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

	int i = 0;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		i++;
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_REFERS) 
           << std::setw(colw) << std::left << HARD_TAG_REFERS_TO
           << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_REFERS_TO);

		if (sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL)
			os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_CONTEXT); 

        os << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump referent grid failed");

    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT idt(ctx_elem), stack_level "
		"FROM context_stack "
		"ORDER BY stack_level",
        "dump context grid"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_CTX_ELEM = 0;
    const int F_STACK_LEVEL = 1;

	os << std::endl;
	os << std::setw(colw) << std::left << "-- context"
	   << std::setw(colw) << std::left << "stack_level"
	   << std::endl;
	os << std::setw(colw) << std::left << "---------"
	   << std::setw(colw) << std::left << "-------"
	   << std::endl;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_CTX_ELEM) 
           << std::setw(colw) << std::left << sqlite3_column_int(stmt, F_STACK_LEVEL)
		   << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump context grid failed");

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::dump_terms(std::ostream& os) {
    sqlite3_stmt *stmt = NULL;
    tagd::code ts_rc = this->prepare(&stmt,
        "SELECT term, term_pos, ROWID FROM terms",
        "dump terms"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_TERM = 0;
    const int F_TERM_POS = 1;
    const int F_TERM_ID = 2;

    int s_rc;
    const int colw = 20;
	os << std::setw(colw) << std::left << "-- term"
	   << std::setw(colw*2) << std::left << "term_pos"
	   << std::setw(colw) << std::left << "term_id"
	   << std::endl;
	os << std::setw(colw) << std::left << "-----"
	   << std::setw(colw*2) << std::left << "---"
	   << std::setw(colw) << std::left << "----"
	   << std::endl;

    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_TERM) 
           << std::setw(colw*2) << std::left
		   << pos_list_str((tagd::part_of_speech) sqlite3_column_int(stmt, F_TERM_POS))
		   << ' '
           << std::setw(colw) << std::left << sqlite3_column_int64(stmt, F_TERM_ID)
           << std::endl; 
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR) {
        return this->error(tagd::TS_INTERNAL_ERR, "dump terms failed");
    }

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::dump(std::ostream& os) {
	// dump tag identities before relations, so that they are
	// all known by the time relations are added
    sqlite3_stmt *stmt = NULL;
    tagd::code ts_rc = this->prepare(&stmt,
        "SELECT idt(tag), idt(super_relator), idt(super_object), pos "
        "FROM tags "
        "ORDER BY rank",
        "dump tags"
    );
    if (ts_rc != tagd::TAGD_OK) return ts_rc;

    const int F_ID = 0;
    const int F_SUPER_REL = 1;
    const int F_SUPER_OBJ = 2;
	int F_POS = 3;

    int s_rc;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::id_type id{(const char*) sqlite3_column_text(stmt, F_ID)};
		tagd::part_of_speech pos{(tagd::part_of_speech) sqlite3_column_int(stmt, F_POS)};

		// ignore hard tags
		if (id[0] == '_') continue;

		// TAGL PUT url statements require a predicate (they will get ouput below with relations)
		if (pos == tagd::POS_URL) continue;
	
		tagd::abstract_tag t(
				id,
				(const char*) sqlite3_column_text(stmt, F_SUPER_REL),
				(const char*) sqlite3_column_text(stmt, F_SUPER_OBJ),
				pos );
		os << "PUT " << t << std::endl << std::endl;
    }

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        this->error(tagd::TS_INTERNAL_ERR, "dump tags failed");

	// dump relations
    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT idt(subject), idt(relator), idt(object), idt(modifier), pos "
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
	F_POS = 4;

	tagd::abstract_tag *t = nullptr;
    while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::id_type id{(const char*) sqlite3_column_text(stmt, F_ID)};
		tagd::part_of_speech pos{(tagd::part_of_speech) sqlite3_column_int(stmt, F_POS)};
		tagd::id_type object{(const char*) sqlite3_column_text(stmt, F_OBJECT)};

		// ignore hard tags
		if (id[0] == '_') continue;

		// ignore hard tag objects
		if (object[0] == '_') continue;

		if (t == nullptr || t->id() != id) {
			if (t != nullptr) {
				os << "PUT " << *t << std::endl << std::endl; 
				delete t;
			}

			if (pos == tagd::POS_URL) {
				t = new tagd::url;
				((tagd::url*)t)->init_hduri(id);
			} else {
				t = new tagd::abstract_tag(id);
			}
		}

        if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				object,
				(const char*) sqlite3_column_text(stmt, F_MODIFIER) );
        } else {
			t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				object );
		}
    }

	if (t != nullptr) {
		os << "PUT " << *t << std::endl;
		delete t;
	}

    sqlite3_finalize(stmt);

    if (s_rc == SQLITE_ERROR)
        return this->error(tagd::TS_INTERNAL_ERR, "dump relations failed");

    stmt = NULL;
    ts_rc = this->prepare(&stmt,
        "SELECT idt(refers), idt(refers_to), idt(context) "
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

tagd::code sqlite::child_ranks(tagd::rank_set& R, const tagd::id_type& super_object) {
    this->open();
	OK_OR_RET_ERR();
    

    int s_rc = this->prepare(&_child_ranks_stmt ,
        "SELECT rank FROM tags WHERE super_object = tid(?)",
        "child ranks"
    );

    this->bind_text(&_child_ranks_stmt, 1, super_object.c_str(), "rank super_object");
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
                if (super_object == "_entity") {
                    // _entity is the only tag having NULL rank
                    // don't insert _entity in the rank_set,
                    // only its children
                    continue;
                } else {
                    return this->error(tagd::TS_INTERNAL_ERR, "integrity error: NULL rank");
                }
                break;
            default:
                return this->ferror(tagd::TS_ERR, "get_childs rank.init() error: %s", tagd_code_str(rc));
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
		return this->error(tagd::TS_ERR, ss.str());
    }

    return this->code(tagd::TAGD_OK);
}

tagd::code sqlite::exec_mprintf(const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	va_end (args);

	char *sql = sqlite3_vmprintf(fmt, args);
	this->exec(sql);
	sqlite3_free(sql);

	return _code;
}

tagd::code sqlite::prepare(sqlite3_stmt **stmt, const char *sql, const char *label) {
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
				return this->ferror(tagd::TS_INTERNAL_ERR, "'%s` prepare statement failed: %s", label, sqlite3_errmsg(_db));
			else
				return this->ferror(tagd::TS_INTERNAL_ERR, "prepare statement failed: %s", sqlite3_errmsg(_db));
        }
    } else {
        sqlite3_reset(*stmt);
        sqlite3_clear_bindings(*stmt);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_text(sqlite3_stmt**stmt, int i, const char *text, const char*label) {
	int s_rc;
    if ((s_rc = sqlite3_bind_text(*stmt, i, text, -1, SQLITE_TRANSIENT)) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s: %s", sqlite_err_code_str(s_rc), label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_int(sqlite3_stmt**stmt, int i, int val, const char*label) {
    if (sqlite3_bind_int(*stmt, i, val) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_rowid(sqlite3_stmt**stmt, int i, rowid_t id, const char*label) {
    if (id <= 0)  // interpret as NULL
        return this->bind_null(stmt, i, label);
 
    if (sqlite3_bind_int64(*stmt, i, id) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

inline tagd::code sqlite::bind_null(sqlite3_stmt**stmt, int i, const char*label) {
    if (sqlite3_bind_null(*stmt, i) != SQLITE_OK) {
		finalize_stmt(stmt);
        return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
    }

    return this->code(tagd::TAGD_OK);
}

tagd_code sqlite::ferror(tagd::code c, const char *errfmt, ...) {
	va_list args;
	va_start (args, errfmt);
	va_end (args);

	if (_trace_on) {
		std::cerr << tagd_code_str(c) << ": ";
		vfprintf(stderr, errfmt, args);
		std::cerr << std::endl;

		const char* errmsg = sqlite3_errmsg(_db);
		if (strcmp(errmsg, "unknown error") != 0)
			std::cerr << "sqlite3_errmsg: " << errmsg << std::endl;
	}

	return this->verror(c, errfmt, args);
}

void sqlite::finalize() {
    sqlite3_finalize(_get_stmt);
    sqlite3_finalize(_exists_stmt);
    sqlite3_finalize(_term_pos_stmt);
    sqlite3_finalize(_term_id_pos_stmt);
    sqlite3_finalize(_pos_stmt);
    sqlite3_finalize(_refers_to_stmt);
    sqlite3_finalize(_refers_stmt);
    sqlite3_finalize(_insert_term_stmt);
    sqlite3_finalize(_update_term_stmt);
    sqlite3_finalize(_delete_term_stmt);
    sqlite3_finalize(_insert_stmt);
    sqlite3_finalize(_update_tag_stmt);
    sqlite3_finalize(_update_ranks_stmt);
    sqlite3_finalize(_child_ranks_stmt);
    sqlite3_finalize(_insert_relations_stmt);
    sqlite3_finalize(_insert_referents_stmt);
    sqlite3_finalize(_insert_context_stmt);
    sqlite3_finalize(_delete_context_stmt);
    sqlite3_finalize(_delete_tag_stmt);
    sqlite3_finalize(_delete_subject_relations_stmt);
    sqlite3_finalize(_delete_relation_stmt);
    sqlite3_finalize(_delete_refers_to_stmt);
    sqlite3_finalize(_term_pos_occurence_stmt);
    sqlite3_finalize(_truncate_context_stmt);
    sqlite3_finalize(_get_relations_stmt);
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
    _term_pos_stmt = NULL;
    _term_id_pos_stmt = NULL;
    _pos_stmt = NULL;
    _refers_to_stmt = NULL;
    _refers_stmt = NULL;
    _insert_term_stmt = NULL;
    _update_term_stmt = NULL;
    _delete_term_stmt = NULL;
    _insert_stmt = NULL;
    _update_term_stmt = NULL;
    _update_ranks_stmt = NULL;
    _child_ranks_stmt = NULL;
    _insert_relations_stmt = NULL;
    _insert_referents_stmt = NULL;
    _insert_context_stmt = NULL;
    _delete_context_stmt = NULL;
    _delete_tag_stmt = NULL;
    _delete_subject_relations_stmt = NULL;
    _delete_relation_stmt = NULL;
    _delete_refers_to_stmt = NULL;
    _term_pos_occurence_stmt = NULL;
	_truncate_context_stmt = NULL;
    _get_relations_stmt = NULL;
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
