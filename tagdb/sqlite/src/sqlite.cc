#include <iostream>
#include <sstream>
#include <iomanip> // setw
#include <cstring> // strcmp
#include <cstdlib> // atoi
#include <assert.h>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdarg>

#include "tagdb/sqlite.h"

void trace_callback( void* udp, const char* sql ) {
	if (udp == nullptr) {
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
	switch (err[0]) {
		case 'c':
			if (strcmp(err, "columns subject, relator, object are not unique") == 0)
				return TS_SQLITE_UNIQUE;

			if (strcmp(err, "columns refers, refers_to are not unique") == 0)
				return TS_SQLITE_UNIQUE;

			if (strcmp(err, "columns refers, context are not unique") == 0)
				return TS_SQLITE_UNIQUE;

			if (strcmp(err, "columns refers, context = NULL are not unique") == 0)
				return TS_SQLITE_UNIQUE;

			break;

		case 'f':
			if (strcmp(err, "foreign key constraint failed") == 0)
				return TS_SQLITE_FK;

			break;

		case 'F':
			if (strcmp(err, "FOREIGN KEY constraint failed") == 0)
				return TS_SQLITE_FK;

			break;

		case 'U':
			//               0123456789012345678901234
			if (strncmp(err, "UNIQUE constraint failed", 24) == 0)
				return TS_SQLITE_UNIQUE;

			break;
	}	

	return TS_SQLITE_UNK;
}

void finalize_stmt(sqlite3_stmt **stmt) {
	sqlite3_finalize(*stmt);
	*stmt = nullptr;
}

/*\
|*| system or internal (this) errors
\*/
#define OK_OR_RET_ERR() if(_code != tagd::TAGD_OK) return _code
#define STMT_OK_OR_RET_ERR() do{if(_code != tagd::TAGD_OK) { sqlite3_finalize(stmt); return _code; }}while(0)
#define OK_OR_RET_FALSE() if(_code != tagd::TAGD_OK) return false

#define OK_OR_ROLLBACK_RET_ERR() if(_code != tagd::TAGD_OK) { \
		this->finalize(); \
		this->exec("ROLLBACK"); \
		return _code; \
	}

#define OK_OR_RET_POS_UNKNOWN() if(_code != tagd::TAGD_OK) return tagd::POS_UNKNOWN

/*\
|*| session (ssn) errors
\*/

// error(s) will already have been set before returning
#define OK_OR_RET_SSN_ERR() do{\
		if(ssn && ssn->code() != tagd::TAGD_OK) \
			return ssn->code(); \
		if(_code != tagd::TAGD_OK) \
			return _code; \
	}while(0)

#define OK_OR_ROLLBACK_RET_SSN_ERR() do{ \
		if (ssn && ssn->code() != tagd::TAGD_OK) { \
			this->finalize(); \
			this->exec("ROLLBACK"); \
			return ssn->code(); \
		} \
		if(_code != tagd::TAGD_OK) { \
			this->finalize(); \
			this->exec("ROLLBACK"); \
			return _code; \
		} \
	}while(0)

#define RET_SYS_SSN_CODE(C) return (ssn ? ssn->code(this->code(C)) : this->code(C))
#define RET_SSN_CODE(C) return (ssn ? ssn->code(C) : C)

// set session error and return code, don't set this->_code
#define RET_SSN_ERROR(C, E) return (ssn ? ssn->error(C, E) : C) // where E is {tagd::error || tagd::predicate}
#define RET_SSN_FERROR(C, ...) return (ssn ? ssn->ferror(C, __VA_ARGS__) : C) // where E is {tagd::error || tagd::predicate}

// call this->ferror(), and ssn->ferror() if ssn not null, return code
#define RET_SYS_SSN_FERROR(...) do{ \
		if(ssn) { \
			ssn->ferror(__VA_ARGS__); \
			return this->error(ssn->last_error()); \
		} else { \
			return this->ferror(__VA_ARGS__); \
		} \
	}while(0)

// if _code has error, sets/returns a ssn TS_INTERNAL_ERR given action
#define OK_OR_RET_SSN_INT_ERR_ACTION(A) if (ssn) { \
		if (_code != tagd::TAGD_OK) \
			return ssn->error(tagd::TS_INTERNAL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_ACTION, A)); \
		else if (ssn->code() != tagd::TAGD_OK) \
			return ssn->code(); \
	}

#define OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION(A) if (ssn) { \
		if (_code != tagd::TAGD_OK) { \
			this->finalize(); \
			this->exec("ROLLBACK"); \
			return ssn->error(tagd::TS_INTERNAL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_ACTION, A)); \
		} else if (ssn->code() != tagd::TAGD_OK) { \
			this->finalize(); \
			this->exec("ROLLBACK"); \
			return ssn->code(); \
		} \
	}


#define OK_OR_RET_UNKNOWN_SSN_INT_ERR_ACTION(A) if (ssn && _code != tagd::TAGD_OK) { \
		ssn->error(tagd::TS_INTERNAL_ERR, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_ACTION, A)); \
		return tagd::POS_UNKNOWN; \
	}

// creates TS_INTERNAL error object given sqlite3 return code and printf style error msg
#define SQLITE_FERROR(RC, ...) {\
			auto sqlite_err = tagd::error::ferror(tagd::TS_INTERNAL_ERR, __VA_ARGS__); \
			if (RC == SQLITE_ERROR) \
				sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
			this->error(sqlite_err); \
	}

#define RET_SQLITE_FERROR(RC, ...) {\
		auto sqlite_err = tagd::error::ferror(tagd::TS_INTERNAL_ERR, __VA_ARGS__); \
		if (RC == SQLITE_ERROR) \
			sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
		return this->error(sqlite_err); \
	}

#define RET_SQLITE_SSN_FERROR(RC, ...) {\
		auto sqlite_err = tagd::error::ferror(tagd::TS_INTERNAL_ERR, __VA_ARGS__); \
		if (ssn) ssn->error(sqlite_err); \
		if (RC == SQLITE_ERROR) \
			sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
		return this->error(sqlite_err); \
	}

namespace tagdb {

id_transform_func_t f_passthrough = [](const tagd::id_type& id) -> const tagd::id_type& { return id; };

sqlite::~sqlite() {
	this->close();
}

void sqlite::trace_on() {
	tagdb::trace_on();
	if (_db != nullptr)
		sqlite3_trace(_db, trace_callback, nullptr);
	dout << "### sqlite trace on ###" << std::endl;
}

void sqlite::trace_off() {
	tagdb::trace_off();
	sqlite3_trace(_db, nullptr, nullptr);
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
		return this->error(tagd::TS_INTERNAL_ERR, "init: fname empty");

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
		this->create_fts_tags_table();

	// We have to insert _entity and _sub manually because of the FK on _sub_relator
	// The rest of the hard tags will be inserted by bootstrap
	// UNIQUE constraints will be ignored
	if (_code == tagd::TAGD_OK) {

		sqlite3_stmt *stmt = nullptr; 
		this->prepare(&stmt,
			"INSERT OR IGNORE INTO terms (ROWID, term, term_pos) VALUES (?, ?, ?)",
			"insert term"
		);
		if (_code != tagd::TAGD_OK) {
			sqlite3_finalize(stmt);
			this->exec("ROLLBACK");
			return _code;
		}

		const char ** hard_tag_rows = hard_tag::rows();	
		const size_t rows_end = hard_tag::rows_end();

		for (size_t i=1; i<rows_end; i++) {
			tagd::part_of_speech term_pos = hard_tag::term_pos(hard_tag_rows[i]);

			this->bind_rowid(&stmt, 1, i, "insert hard_tag rowid");
			this->bind_text(&stmt, 2, hard_tag_rows[i], "insert hard_tag term");
			this->bind_int(&stmt, 3, term_pos, "insert hard_tag term_pos");

			int s_rc = sqlite3_step(stmt);
			if (s_rc != SQLITE_DONE) {
				this->ferror( tagd::TS_INTERNAL_ERR, "insert hard_tag term failed: %s, sqlite_error: %s",
						hard_tag_rows[i], sqlite3_errmsg(_db) );
			}

			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
		}

		sqlite3_finalize(stmt);

		// INSERT NULL for HARD_TAG_ENTITY rank 
		if ( _code == tagd::TAGD_OK ) {
			this->exec_mprintf(
				"INSERT OR IGNORE INTO tags (tag, sub_relator, super_object, rank, pos) "
				"VALUES (tid('%s'), tid('%s'), tid('%s'), NULL, %d)",
				HARD_TAG_ENTITY, HARD_TAG_SUB, HARD_TAG_ENTITY, tagd::POS_TAG
			);
		}

		if ( _code == tagd::TAGD_OK ) {
			stmt = nullptr;
			this->prepare(&stmt,
				"INSERT OR IGNORE INTO tags (tag, sub_relator, super_object, rank, pos) "
				"VALUES (tid(?), tid(?), tid(?), ?, ?)",
				"insert hard_tag"
			);

			// HARD_TAG_ENTITY (i==1) already inserted
			for (size_t i=2; i<rows_end; i++) {
				tagd::abstract_tag t;
				tagd::code tc = hard_tag::get(t, hard_tag_rows[i]);

				if (tc != tagd::TAGD_OK) {
					this->code(tc);
					break;
				}

				this->bind_text(&stmt, 1, t.id().c_str(), "insert hard_tag id");
				this->bind_text(&stmt, 2, t.sub_relator().c_str(), "insert hard_tag sub_relator");
				this->bind_text(&stmt, 3, t.super_object().c_str(), "insert hard_tag super_object");
				if (t.rank().empty()) {
					assert(t.id() == HARD_TAG_ENTITY);
					this->bind_null(&stmt, 4, "insert hard_tag rank");
				} else {
					this->bind_text(&stmt, 4, t.rank().c_str(), "insert hard_tag rank");
				}
				this->bind_text(&stmt, 4, t.rank().c_str(), "insert hard_tag rank");
				this->bind_int(&stmt, 5, t.pos(), "insert hard_tag pos");

				int s_rc = sqlite3_step(stmt);
				if (s_rc != SQLITE_DONE) {
					this->ferror( tagd::TS_INTERNAL_ERR, "insert hard_tag failed: %s, sqlite_error: %s",
							t.id().c_str(), sqlite3_errmsg(_db) );
					sqlite3_finalize(stmt);
					break;
				}

				sqlite3_reset(stmt);
				sqlite3_clear_bindings(stmt);
			}
		}
		sqlite3_finalize(stmt);
	}

	if (_code != tagd::TAGD_OK) {
		tagd::code tc = _code;
		this->exec("ROLLBACK");
		return this->code(tc);
	}

	this->exec("COMMIT");

	// enforce foreign keys after inserting  _entity _sub _entity
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

		rowid_t term_id;
		sqlite *tdb = (sqlite*)sqlite3_user_data(context);
		tagd::part_of_speech pos = (
			term[0] == '_'
				? hard_tag::term_pos(term, &term_id)
				: tdb->term_pos(term, &term_id)
		);

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
		sqlite *tdb = (sqlite*)sqlite3_user_data(context);

		std::string term;
		tagd::part_of_speech pos = (
			term[0] == '_'
				? hard_tag::term_id_pos(term_id, &term)
				: tdb->term_id_pos(term_id, &term)
		);

		if (pos == tagd::POS_UNKNOWN) {
			sqlite3_result_null(context);
			return;
		}

		// can't use SQLITE_STATIC because term data destroyed before results accessed
		sqlite3_result_text(context, term.data(), term.size(), SQLITE_TRANSIENT);
	};

	int rc;
	rc = sqlite3_create_function(_db, "tid", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC, this, f_tid, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function tid failed");

	rc = sqlite3_create_function(_db, "idt", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC, this, f_idt, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function idt failed");

	// check db
	sqlite3_stmt *stmt = nullptr; 
	this->prepare(&stmt,
		"SELECT 1 FROM sqlite_master "
		"WHERE type = 'table' "
		"AND sql LIKE 'CREATE TABLE terms%'",
		"terms table exists"
	);
	STMT_OK_OR_RET_ERR();

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "check table error: %s", "terms");
	
	// table exists
	if (s_rc == SQLITE_ROW) {
		if (this->term_pos(HARD_TAG_ENTITY) != tagd::POS_UNKNOWN)
			return tagd::TAGD_OK; // db already initialized
	}

	// TODO VACUUMs can reset rowids, so use INTEGER PRIMARY KEY alias
	// "CREATE TABLE terms ( "
	//  "tid INTEGER PRIMARY KEY, "
	//  "term UNIQUE NOT NULL, "
	//    "term_pos INTEGER NOT NULL"
	// ")" 

	this->exec(
	"CREATE TABLE terms ( "
		"term PRIMARY KEY NOT NULL, "
		"term_pos INTEGER NOT NULL"
	")" 
	);
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_term ON terms(term)");

	return _code;
}

tagd::code sqlite::create_tags_table() {
	// check db
	sqlite3_stmt *stmt = nullptr; 
	this->prepare(&stmt,
		"SELECT 1 FROM sqlite_master "
		"WHERE type = 'table' "
		"AND sql LIKE 'CREATE TABLE tags%'",
		"tags table exists"
	);
	STMT_OK_OR_RET_ERR();

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "check table error: %s", "tags");
	
	// table exists
	if (s_rc == SQLITE_ROW) {
		tagd::tag t;
		this->get(t, HARD_TAG_ENTITY, nullptr, F_NO_TRANSFORM_REFERENTS);
		OK_OR_RET_ERR();

		if (t.id() == HARD_TAG_ENTITY && t.super_object() == HARD_TAG_ENTITY)
			return tagd::TAGD_OK; // db already initialized
	
		return this->error(tagd::TS_INTERNAL_ERR, "tag table exists but has no _entity row; database corrupt");
	}

	//create db

	this->exec(
	"CREATE TABLE tags ( "
		"tag     INTEGER PRIMARY KEY NOT NULL, "
		"sub_relator   INTEGER NOT NULL, "
		"super_object   INTEGER NOT NULL, "
		// binary collation defeats LIKE wildcard partial indexes,
		// but enables indexing for GLOB style patterns
		"rank    TEXT UNIQUE COLLATE BINARY, "
		"pos     INTEGER NOT NULL, "
		"FOREIGN KEY(sub_relator) REFERENCES tags(tag), "
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

	return _code;
}

tagd::code sqlite::create_referents_table() {
	sqlite3_stmt *stmt = nullptr; 
	this->prepare(&stmt,
		"SELECT 1 FROM sqlite_master "
		"WHERE type = 'table' "
		"AND sql LIKE 'CREATE TABLE referents%'",
		"referents table exists"
	);
	STMT_OK_OR_RET_ERR();

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "check table error: %s", "referents");
	
	// table exists
	if (s_rc == SQLITE_ROW)
		return tagd::TAGD_OK;

	//create db
	this->exec(
	"CREATE TABLE referents ("
		"refers     INTEGER NOT NULL, "
		"refers_to  INTEGER NOT NULL, "
		"context    INTEGER NOT NULL, "
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

tagd::code sqlite::create_fts_tags_table() {
	sqlite3_stmt *stmt = nullptr;
	this->prepare(&stmt,
		"SELECT 1 FROM sqlite_master "
		"WHERE type = 'table' "
		"AND sql LIKE 'CREATE VIRTUAL TABLE fts_tags %'",
		"fts_tags table exists"
	);
	STMT_OK_OR_RET_ERR();

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "check table error: %s", "fts_tags");

	// table exists
	if (s_rc == SQLITE_ROW)
		return tagd::TAGD_OK;

	//create db
	this->exec( "CREATE VIRTUAL TABLE fts_tags USING fts4()" );

	return _code;
}

tagd::code sqlite::create_relations_table() {
	// check db
	sqlite3_stmt *stmt = nullptr; 
	this->prepare(&stmt,
		"SELECT 1 FROM sqlite_master "
		"WHERE type = 'table' "
		"AND sql LIKE 'CREATE TABLE relations%'",
		"relations table exists"
	);
	STMT_OK_OR_RET_ERR();

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "check table error: %s", "relations");
	
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
	if (_db != nullptr)
		return tagd::TAGD_OK;

	int rc = sqlite3_open(_db_fname.c_str(), &_db);
	if( rc != SQLITE_OK ){
		this->close();
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "open database failed: %s", _db_fname.c_str());
	}
	_code = tagd::TAGD_OK;  // exec() requires _code == TAGD_OK

	if ( this->exec("PRAGMA temp_store = MEMORY") != tagd::TAGD_OK)
		return this->ferror(tagd::TS_INTERNAL_ERR, "PRAGMA temp_store failed: %s", _db_fname.c_str());

	return this->code(tagd::TAGD_OK);
}

void sqlite::close() {
	if (_db == nullptr)
		return;

	this->finalize();
	auto rc = sqlite3_close(_db);
	if (rc) {
		std::cerr << "error: sqlite3_close() returned " << rc << ": " << sqlite3_errmsg(_db) << std::endl;
	}
	_db = nullptr;
}

tagd::code sqlite::get(tagd::abstract_tag& t, const tagd::id_type& term, session* ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (_trace_on)
		std::cerr << "sqlite::get: " << term << std::endl;

	if (t.id()[0] == '_') {
		tagd::code tc = hard_tag::get(t, term);
		if (tc == tagd::TAGD_OK) {
			if (_trace_on)
				std::cerr << "got hard_tag: " << t << " -- " << t.rank().dotted_str() << std::endl;
			RET_SSN_CODE(tc);
		} else if (tc != tagd::TS_NOT_FOUND) {
			RET_SYS_SSN_FERROR(tc, "hard_tag::get(%s) unexpected tag code: %s",
				t.id().c_str(), tagd::code_str(tc));
		}
	}

	tagd::part_of_speech term_pos = this->term_pos(term);
	// std::cerr << "term_pos(" << term << "): " << pos_list_str(term_pos) << std::endl;

	if (term_pos == tagd::POS_UNKNOWN) {
		if (flags & F_NO_NOT_FOUND_ERROR) {
			return tagd::TS_NOT_FOUND;
		} else {
			RET_SSN_ERROR(tagd::TS_NOT_FOUND,
				tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term) );
		}
	}

	tagd::id_type id;
	if (ssn && !(flags & F_NO_TRANSFORM_REFERENTS) && (term_pos & tagd::POS_REFERS)) {
		this->decode_referent(id, term, ssn);
	} else {
		id = term;
	}
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:decode_referent");

	this->prepare(&_get_stmt,
		"SELECT idt(tag), idt(sub_relator), idt(super_object), pos, rank "
		"FROM tags WHERE tag = tid(?)",
		"get_tag"
	);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:get_tag");

	this->bind_text(&_get_stmt, 1, id.c_str(), "get_tag_id");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:get_tag_id");

	const int F_ID = 0;
	const int F_SUB_REL = 1;
	const int F_SUB_OBJ = 2;
	const int F_POS = 3;
	const int F_RANK = 4;

	id_transform_func_t f_transform =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) ?  f_passthrough : this->f_encode_referent(ssn);

	int s_rc = sqlite3_step(_get_stmt);
	if (s_rc == SQLITE_ROW) {
		t.id( f_transform((const char*) sqlite3_column_text(_get_stmt, F_ID)) );
		t.sub_relator( f_transform((const char*) sqlite3_column_text(_get_stmt, F_SUB_REL)) );
		t.super_object( f_transform((const char*) sqlite3_column_text(_get_stmt, F_SUB_OBJ)) );
		t.pos( (tagd::part_of_speech) sqlite3_column_int(_get_stmt, F_POS) );
		t.rank( (const char*) sqlite3_column_text(_get_stmt, F_RANK) );

		this->get_relations(t.relations, id, ssn, flags);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:get_relations");

		// if id was transformed via referent, add a _refers_to the orignal id
		// don't transform _refers_to because it we want it to be accessible
		// universally and programmatically (the id will be easily recognizable
		// as a referent due to the presence of a _refers_to predicate)
		if (!(flags & F_NO_TRANSFORM_REFERENTS)) {
			if (id != t.id())
				t.relation(HARD_TAG_REFERS_TO, id);
		} 

	} else if (s_rc == SQLITE_ERROR) {
		this->ferror(tagd::TS_INTERNAL_ERR, "tagdb:get:get_tag_step failed: %s", sqlite3_errmsg(_db));
		RET_SSN_FERROR(tagd::TS_INTERNAL_ERR, "tagdb:get:get_tag_step error for id: %s", id.c_str());
	} else {
		if (term_pos & tagd::POS_REFERS) {
			RET_SSN_FERROR(tagd::TS_AMBIGUOUS,
				"%s refers to a tag with no matching context", term.c_str());
		} else {
			if (flags & F_NO_NOT_FOUND_ERROR) {
				return tagd::TS_NOT_FOUND;
			} else {
				RET_SSN_ERROR(tagd::TS_NOT_FOUND,
					tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term) );
			}
		}
	}

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::get(tagd::url& u, const tagd::id_type& id, session* ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	// id should be a canonical url
	u.init(id);
	if (!u.ok())
		RET_SSN_FERROR(u.code(), "url init failed: %s", id.c_str());

	// we use hduri to identify urls internally
	tagd::id_type hduri = u.hduri();
	this->get((tagd::abstract_tag&)u, hduri, ssn, flags);
	OK_OR_RET_SSN_ERR(); // this or ssn errors already set

	u.init_hduri(hduri);  // id() should now be canonical url
	if (!u.ok())
		RET_SSN_FERROR(u.code(), "url init_hduri failed: %s", hduri.c_str());

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::get_relations(tagd::predicate_set& P, const tagd::id_type& id, session *ssn, flags_t flags) {

	this->prepare(&_get_relations_stmt,
		"SELECT idt(relator), idt(object), idt(modifier) "
		"FROM relations WHERE subject = tid(?)",
		"get_relations"
	);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get_relations");

	this->bind_text(&_get_relations_stmt, 1, id.c_str(), "get subject");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:bind_subject");

	const int F_RELATOR = 0;
	const int F_OBJECT = 1;
	const int F_MODIFIER = 2;

	id_transform_func_t f_transform =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) ?  f_passthrough : this->f_encode_referent(ssn);

	int s_rc;
	while ((s_rc = sqlite3_step(_get_relations_stmt)) == SQLITE_ROW) {
		auto p = tagd::predicate(
			f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_RELATOR) ),
			f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_OBJECT) )
		);
		if (sqlite3_column_type(_get_relations_stmt, F_MODIFIER) != SQLITE_NULL) {
			p.modifier = f_transform( (const char*) sqlite3_column_text(_get_relations_stmt, F_MODIFIER) );
		}
		P.insert(p);
	} 

	if (s_rc == SQLITE_ERROR) {
		this->ferror(tagd::TS_INTERNAL_ERR, "tagdb:get_relations:step failed: %s", sqlite3_errmsg(_db));
		RET_SSN_FERROR(tagd::TS_INTERNAL_ERR, "tagdb:get_relations:step error for id: %s", id.c_str());
	}

	if (P.empty())
		return tagd::TS_NOT_FOUND;

	return tagd::TAGD_OK;  // don't set session code on non-public methods
}

tagd::part_of_speech sqlite::term_pos(const tagd::id_type& id, rowid_t *term_id) {

	tagd::code tc = this->prepare(&_term_pos_stmt,
		"SELECT ROWID, term_pos FROM terms WHERE term = ?",
		"term term_pos"
	);
	if (tc != tagd::TAGD_OK)
		this->print_errors();
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	tc = this->bind_text(&_term_pos_stmt, 1, id.c_str(), "term");
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	const int F_TERM_ID = 0;
	const int F_TERM_POS = 1;

	int s_rc = sqlite3_step(_term_pos_stmt);
	if (s_rc == SQLITE_ROW) {
		if (term_id != nullptr)
			*term_id = sqlite3_column_int64(_term_pos_stmt, F_TERM_ID);
		return (tagd::part_of_speech) sqlite3_column_int(_term_pos_stmt, F_TERM_POS);
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "term_pos failed: %s", id.c_str());
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::part_of_speech sqlite::term_id_pos(rowid_t term_id, std::string *term) {

	tagd::code tc = this->prepare(&_term_id_pos_stmt,
		"SELECT term, term_pos FROM terms WHERE ROWID = ?",
		"term term_pos"
	);
	if (tc != tagd::TAGD_OK)
		this->print_errors();
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	tc = this->bind_rowid(&_term_id_pos_stmt, 1, term_id, "term_id");
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	const int F_TERM = 0;
	const int F_TERM_POS = 1;

	int s_rc = sqlite3_step(_term_id_pos_stmt);
	if (s_rc == SQLITE_ROW) {
		if (term != nullptr)
			*term = (const char*)sqlite3_column_text(_term_id_pos_stmt, F_TERM);
		return (tagd::part_of_speech) sqlite3_column_int(_term_id_pos_stmt, F_TERM_POS);
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "term_id_pos failed: %ld", term_id);
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::part_of_speech sqlite::pos(const tagd::id_type& id, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (id[0] == '_')
		return hard_tag::pos(id);

	tagd::id_type refers_to;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
		refers_to = id;
	else
		this->refers_to(refers_to, id, ssn);  // refers_to set if id refers to it (in context)

	if (refers_to[0] == '_')
		return hard_tag::pos(refers_to);

	this->prepare(&_pos_stmt,
		"SELECT pos FROM tags WHERE tag = tid(?)",
		"tag pos"
	);
	assert(_code == tagd::TAGD_OK);
	OK_OR_RET_UNKNOWN_SSN_INT_ERR_ACTION("tagdb:pos");

	this->bind_text(&_pos_stmt, 1, (refers_to.empty() ? id.c_str() : refers_to.c_str()), "pos id");
	assert(_code == tagd::TAGD_OK);
	OK_OR_RET_UNKNOWN_SSN_INT_ERR_ACTION("tagdb:pos:bind");

	const int F_POS = 0;

	int s_rc = sqlite3_step(_pos_stmt);
	if (s_rc == SQLITE_ROW) {
		return (tagd::part_of_speech) sqlite3_column_int(_pos_stmt, F_POS);
	} else if (s_rc == SQLITE_ERROR) {
		auto e = tagd::error::ferror(tagd::TS_INTERNAL_ERR,
			"tagdb:pos:step failed: %s", (refers_to.empty() ? id.c_str() : refers_to.c_str()));
		if (ssn)
			ssn->error(e);
		e.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db));
		this->error(e);
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::code sqlite::refers(tagd::id_type &refers, const tagd::id_type& refers_to, session* ssn) {
	assert(!refers_to.empty());
	if(!ssn || ssn->context().empty()) return tagd::TS_NOT_FOUND;

	for (auto it = ssn->context().rbegin(); it != ssn->context().rend(); ++it) {
		this->prepare(&_refers_stmt,
			"SELECT idt(refers) "
			"FROM referents, tags "
			"WHERE refers_to = tid(?) "
			"AND context = tag "
			"AND rank GLOB ("
			 "SELECT rank FROM tags WHERE tag = tid(?)"
			") || '*' " // all context <= {_context}
			"ORDER BY rank DESC "  // closest (subordinate) rank
			"LIMIT 1",
			"refers"
		);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers");

		this->bind_text(&_refers_stmt, 1, refers_to.c_str(), "refers_to");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers:bind_refers_to");

		this->bind_text(&_refers_stmt, 2, it->c_str(), "tag in context");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers:bind_context");

		const int F_REFERS = 0;

		int s_rc = sqlite3_step(_refers_stmt);
		if (s_rc == SQLITE_ROW) {
			refers = (const char *) sqlite3_column_text(_refers_stmt, F_REFERS);
			return tagd::TAGD_OK;
		} else if (s_rc == SQLITE_ERROR) {
			return this->ferror(tagd::TS_INTERNAL_ERR, "refers failed: %s", sqlite3_errmsg(_db));
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers:step");
		}
	}

	return tagd::TS_NOT_FOUND;
}

tagd::code sqlite::refers_to(tagd::id_type &refers_to, const tagd::id_type& refers, session* ssn) {
	assert(!refers.empty());
	if(!ssn) return tagd::TS_NOT_FOUND;

	for (auto it = ssn->context().rbegin(); it != ssn->context().rend(); ++it) {
		this->prepare(&_refers_to_stmt,
			"SELECT idt(refers_to) "
			"FROM referents, tags "
			"WHERE refers = tid(?) "
			"AND context = tag "
			"AND rank GLOB ("
			 "SELECT rank FROM tags WHERE tag = tid(?)"
			") || '*' " // all context <= {_context}
			"ORDER BY rank DESC "
			"LIMIT 1",
			"refers_to"
		);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to");

		this->bind_text(&_refers_to_stmt, 1, refers.c_str(), "tagdb:refers_to:bind_refers");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:bind_refers");

		this->bind_text(&_refers_to_stmt, 2, it->c_str(), "tagdb:refers_to:bind_context");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:bind_context");

		const int F_REFERS_TO = 0;

		int s_rc = sqlite3_step(_refers_to_stmt);
		if (s_rc == SQLITE_ROW) {
			refers_to = (const char *) sqlite3_column_text(_refers_to_stmt, F_REFERS_TO);
			return tagd::TAGD_OK;
		} else if (s_rc == SQLITE_ERROR) {
			this->ferror(tagd::TS_INTERNAL_ERR, "tagdb:refers_to failed: %s", sqlite3_errmsg(_db));
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:step");
		}
	}

	return tagd::TS_NOT_FOUND;
}

// returns whether tag exists or not
// on error, error set and returns false
bool sqlite::exists(const tagd::id_type& id, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(nullptr);

	this->prepare(&_exists_stmt,
		"SELECT 1 FROM tags WHERE tag = tid(?)",
		"tag exists"
	);
	OK_OR_RET_FALSE();

	this->bind_text(&_exists_stmt, 1, id.c_str(), "exists statement id");
	OK_OR_RET_FALSE();

	int s_rc = sqlite3_step(_exists_stmt);
	if (s_rc == SQLITE_ROW) {
		return true;
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "exists failed: %", id.c_str());
		return false;
	} 

	return false; // not found
}

// tagd::TS_NOT_FOUND returned if destination undefined
tagd::code sqlite::put(const tagd::abstract_tag& put_tag, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (_trace_on)
		std::cerr << "sqlite::put: " << put_tag << " -- " << flag_util::flag_list_str(flags) << std::endl;

	if (put_tag.id().length() > tagd::MAX_TAG_LEN)
		RET_SSN_FERROR(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (put_tag.id().empty())
		RET_SSN_ERROR(tagd::TS_MISUSE, "inserting empty tag not allowed");

	if (put_tag.id()[0] == '_' && !_doing_init)
		RET_SSN_FERROR(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", put_tag.id().c_str());

	if (!(flags & F_NO_POS_CAST)) {
		switch (put_tag.pos()) {
			case tagd::POS_URL:
				RET_SSN_CODE(this->put((const tagd::url&)put_tag, ssn, flags));
			case tagd::POS_REFERENT:
				RET_SSN_CODE(this->put((const tagd::referent&)put_tag, ssn, flags));
			default:
				; // NOOP
		}
	}

	tagd::abstract_tag t;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
		t = put_tag;
	else
		this->decode_referents(t, put_tag, ssn);
	OK_OR_RET_SSN_INT_ERR_ACTION("tadb:put:decode_referents");

	if (t.id() == t.super_object() && t.id() != HARD_TAG_ENTITY)
		RET_SSN_FERROR(tagd::TS_MISUSE, "_id == _super_object not allowed: %s", t.id().c_str()); 

	tagd::abstract_tag existing;
	tagd::code existing_rc = this->get(existing, t.id(), ssn, (flags|F_NO_TRANSFORM_REFERENTS|F_NO_NOT_FOUND_ERROR));
	if (existing_rc != tagd::TAGD_OK && existing_rc != tagd::TS_NOT_FOUND)
		return existing_rc; // err set by get

	// insert/updates fts_tags and returns whatever error occurs first or the code passed in
	auto f_fts_passthru = [this, existing_rc, &t, ssn, flags](tagd::code tc) -> tagd::code {
		if ( existing_rc == tagd::TAGD_OK ) {
			this->update_fts_tag(t.id(), flags);
			OK_OR_RET_SSN_INT_ERR_ACTION("tadb:put:update_fts");
		} else if ( existing_rc == tagd::TS_NOT_FOUND ) {
			this->insert_fts_tag(t.id(), flags);
			OK_OR_RET_SSN_INT_ERR_ACTION("tadb:put:insert_fts");
		}
		return tc;
	};

	if (t.super_object().empty()) {
		if (existing_rc == tagd::TS_NOT_FOUND) {
			// can't do anything without knowing what a tag is
			RET_SSN_ERROR(tagd::TS_SUB_UNK,
				tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, t.id()) );
		} else {
			if (t.relations.empty()) {  // duplicate tag and no relations to insert 
				RET_SSN_FERROR(tagd::TS_MISUSE, "cannot put a tag without relations: %s", t.id().c_str());
			} else { // insert relations
				RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags)));
			}
		}
	}

	tagd::abstract_tag destination;
	tagd::code dest_rc = this->get(destination, t.super_object(), ssn, (flags|F_NO_TRANSFORM_REFERENTS));
	if (dest_rc != tagd::TAGD_OK) {
		if (dest_rc == tagd::TS_NOT_FOUND) {
			RET_SSN_FERROR(tagd::TS_SUB_UNK, "unknown super_object: %s", t.super_object().c_str());
		} else {
			return dest_rc; // err set by get
		}
	}

	// handle duplicate tags up-front
	tagd::code ins_upd_rc;
	if ( existing_rc == tagd::TAGD_OK ) {  // existing tag
		if ( t.sub_relator() == existing.sub_relator() &&
			t.super_object() == existing.super_object() )
		{  // same location
			if (t.relations.empty()) {  // duplicate tag and no relations to insert 
				if (flags & F_IGNORE_DUPLICATES)
					RET_SSN_CODE(tagd::TAGD_OK);
				else
					RET_SSN_FERROR(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
			} else { // insert relations
				RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags)));
			}
		}

		// use existing because it has the rank
		if (existing.sub_relator() != t.sub_relator())
			existing.sub_relator(t.sub_relator());
		// move existing to new location or relator
		ins_upd_rc = this->update(existing, destination);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:put:update");
	} else if (existing_rc == tagd::TS_NOT_FOUND) {
		// new tag
		ins_upd_rc = this->insert(t, destination);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:put:insert");
	} else {
		assert(false); // should never get here
		this->ferror(tagd::TS_INTERNAL_ERR, "put failed: %s", sqlite3_errmsg(_db));
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:put");
		return _code;
	}

	if (ins_upd_rc == tagd::TAGD_OK && !t.relations.empty())
		RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags)));

	// res from insert/update, (errors will have been set)
	RET_SSN_CODE(f_fts_passthru(ins_upd_rc));
}

tagd::code sqlite::put(const tagd::url& u, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (!u.ok())
		RET_SSN_FERROR(u.code(), "put url not ok(%s): %s",  tagd::code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hduri
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hduri());
	tagd::url::insert_url_part_relations(t.relations, u);
	RET_SSN_CODE(this->put(t, ssn, (flags|F_NO_POS_CAST)));
}

tagd::code sqlite::put(const tagd::referent& r, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	RET_SSN_CODE(this->insert_referent(r, ssn, flags));
}


void tag_affected(std::set<tagd::id_type>& terms_affected, const tagd::abstract_tag& t) {
	auto f_term_affected = [&terms_affected](const tagd::id_type& term) mutable {
		if (!term.empty())
			terms_affected.insert(term);
	};

	f_term_affected(t.id());
	f_term_affected(t.sub_relator());
	f_term_affected(t.super_object());
	for ( auto p : t.relations ) {
		f_term_affected(p.relator);
		f_term_affected(p.object);
		f_term_affected(p.modifier);
	}
}

tagd::code sqlite::del(const tagd::abstract_tag& t, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (_trace_on)
		std::cerr << "sqlite::del: " << t << std::endl;

	if (t.id().empty())
		RET_SSN_ERROR(tagd::TS_MISUSE, "deleting empty tag not allowed");

	if (t.id()[0] == '_' && !_doing_init)
		RET_SSN_FERROR(tagd::TS_MISUSE, "deleting hard tags not allowed: %s", t.id().c_str());

	if (!(flags & F_NO_POS_CAST)) {
		switch (t.pos()) {
			case tagd::POS_URL:
				RET_SSN_CODE(this->del((const tagd::url&)t, ssn, flags));
			case tagd::POS_REFERENT:
				RET_SSN_CODE(this->del((const tagd::referent&)t, ssn, flags));
			default:
				; // NOOP
		}
	}

	if (!t.super_object().empty() /*TODO && !(flags && IGNORE_SUB) */) {
		RET_SSN_FERROR(tagd::TS_MISUSE,
			"sub must not be specified when deleting tag: %s", t.id().c_str());
	}

	tagd::abstract_tag del_tag;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
		del_tag = t;
	else
		this->decode_referents(del_tag, t, ssn);

	tagd::abstract_tag existing;
	auto tc = this->get(existing, del_tag.id(), ssn, 
			// don't allow flags to override not-found error - deleting a non-existant tag is always and error
			(flags | (F_NO_TRANSFORM_REFERENTS & (~F_NO_NOT_FOUND_ERROR))) );
	OK_OR_RET_SSN_ERR();  // errors already set

	if (tc == tagd::TS_NOT_FOUND) {
		if (flags & F_NO_NOT_FOUND_ERROR)
			return tc;
		else
			RET_SSN_FERROR(tc, "cannot delete non-existing: %s", del_tag.id().c_str());
	} else if (tc != tagd::TAGD_OK) {
		return tc;
	}

	// make a set of all terms affected, so we can update the term pos after deleting tag
	std::set<tagd::id_type> terms_affected;

	this->exec("BEGIN");

	// empty del_tag relations means delete entire tag for given id
	if (del_tag.relations.empty()) {
		// don't allow deleting tags having a referent as the id
		if (t.id() != del_tag.id()) {   // referent was decoded
			if (ssn) {
				ssn->ferror(tagd::TS_MISUSE,
					"will not delete `%s', refers_to `%s'", t.id().c_str(), del_tag.id().c_str());
			}
			OK_OR_ROLLBACK_RET_SSN_ERR();
		}

		// delete referents, relations, and tag
		this->delete_refers_to(del_tag.id());
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_refers_to");

		this->delete_relations(del_tag.id());  // all relations given subject
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_relations");

		this->delete_fts_tag(del_tag.id());
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_fts_tag");

		this->delete_tag(del_tag.id(), ssn);
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_tag");

		// referents affected
		tagd::tag_set R;
		tagd::interrogator q_refers_to(HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT);
		q_refers_to.relation(HARD_TAG_REFERS_TO, del_tag.id());

		auto tc = this->query(R, q_refers_to, nullptr);
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:q_refers_to");

		if (tc == tagd::TAGD_OK) {
			for ( auto r : R )
				tag_affected(terms_affected, r);
		}

		tag_affected(terms_affected, existing);
	} else {
		// delete only del_tag.relations
		int errcnt = 0;
		for( auto p : del_tag.relations ) {
			if (!existing.related(p)) {
				errcnt++;
				if (ssn) {
					tagd::error e;
					if (p.modifier.empty()) {
						e = tagd::error::ferror(tagd::TS_NOT_FOUND,
							"cannot delete non-existent relation: %s %s %s",
							del_tag.id().c_str(), p.relator.c_str(), p.object.c_str()); 
					} else {
						e = tagd::error::ferror(tagd::TS_NOT_FOUND,
							"cannot delete non-existent relation: %s %s %s = %s",
							del_tag.id().c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str()); 
					}
					if (!this->exists(p.relator, flags|F_NO_RESET))
						e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, p.relator);
					if (!this->exists(p.object, flags|F_NO_RESET))
						e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, p.object);

					this->error(e);
				}
			}
		}
		if (errcnt) {
			OK_OR_ROLLBACK_RET_SSN_ERR();
			return tagd::TS_MISUSE;  // in case _code == TAGD_OK and ssn not set
		}

		this->delete_relations(del_tag.id(), del_tag.relations);
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_relations");

		tag_affected(terms_affected, del_tag);
	}

	for ( auto id : terms_affected ) {
		this->update_pos_occurence(id);
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:update_pos_occurence");
	}

	this->exec("COMMIT");

	return tagd::TAGD_OK;
}

tagd::code sqlite::del(const tagd::url& u, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (!u.ok())
		RET_SSN_FERROR(u.code(), "del url not ok(%s): %s",  tagd::code_str(u.code()), u.id().c_str());

	// url _id is the actual url, but we use the hduri
	// internally, so we have to convert it
	tagd::abstract_tag t = (tagd::abstract_tag) u;
	t.id(u.hduri());
	t.sub_relator(tagd::id_type());
	t.super_object(tagd::id_type());
	// don't insert url part relations
	RET_SSN_CODE(this->del(t, ssn, (flags|F_NO_POS_CAST)));
}

tagd::code sqlite::del(const tagd::referent& r, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	sqlite3_stmt *stmt = nullptr;

	if (r.refers().empty()) {
		if (r.refers_to().empty()) {
			if (r.context().empty()) {
				// don't allow deleting all referents
				RET_SSN_ERROR(tagd::TS_MISUSE, "deleting all referents not allowed");
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (r.context().empty()) {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers_to = tid(?)",
					"delete referent refers_to"
				);
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (r.context().empty()) {
				this->prepare(&stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?)",
					"delete referent refers, refers_to"
				);
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 3, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		}
	}

	int s_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete referent failed: %s", r.str().c_str());

	if (sqlite3_changes(_db) == 0)
		RET_SSN_FERROR(tagd::TS_NOT_FOUND, "delete referent not found: %s", r.str().c_str());

	// make a set off all terms affected, so we can update the term pos after deleting tag
	std::set<tagd::id_type> terms_affected;
	tag_affected(terms_affected, r);
	for ( auto id : terms_affected ) {
		this->update_pos_occurence(id);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:del:referent:terms_affected");
	}

	return tagd::TAGD_OK;
}

tagd::part_of_speech sqlite::term_pos_occurence(const tagd::id_type& id, session *ssn, bool set_fk_err) {
	if (_term_pos_occurence_stmt == nullptr) {
		// TODO there is probably a more optimal way of doing this
		// I know, WTF, but when in doubt, use brute force
		// TODO this might not even be needed
		// if we are to do it, remove the dynamic SQL and embedded tagd::POS_* values not needed 
		// next, accomodate all tagd::part_of_speech types
		std::stringstream ss;
		ss << "SELECT pos FROM tags WHERE tag = tid(?) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_SUB_RELATOR << " FROM tags WHERE sub_relator = tid(?) LIMIT 1) "
		   << "UNION "
		   << "SELECT * FROM (SELECT " << tagd::POS_SUB_OBJECT << " FROM tags WHERE super_object = tid(?) LIMIT 1) "
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
		sqlite3_reset(_term_pos_occurence_stmt);
		sqlite3_clear_bindings(_term_pos_occurence_stmt);
	}
	OK_OR_RET_POS_UNKNOWN();

	int i = 1;
	this->bind_text(&_term_pos_occurence_stmt, i, id.c_str(), "tag pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(&_term_pos_occurence_stmt, ++i, id.c_str(), "sub_relator pos occurence");
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
			// if we ever want to insert errors in a tagdb
			tagd::id_type cause;
			switch(pos) {
				case tagd::POS_SUB_RELATOR:
					cause = "sub_relator";
					break;
				case tagd::POS_SUB_OBJECT:
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
				if (ssn) {
					ssn->error(tagd::TS_RELATION_DEPENDENCY,
						tagd::predicate(HARD_TAG_CAUSED_BY, cause, id) );
				}
			}
		}
	}
	//std::cerr << "occurence_pos(" << id << "): " << pos_list_str(occurence_pos) << std::endl;

	if (s_rc == SQLITE_ERROR)
		SQLITE_FERROR(s_rc, "term pos occurence failed: %s", id.c_str());

	return occurence_pos;
}

tagd::code sqlite::update_pos_occurence(const tagd::id_type& id) {
	tagd::part_of_speech occurence_pos = this->term_pos_occurence(id, nullptr, false);
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
		RET_SQLITE_FERROR(s_rc, "delete refers_to failed: %s", id.c_str());

	return tagd::TAGD_OK;
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
		RET_SQLITE_FERROR(s_rc, "delete subject relations failed: %s", subject.c_str());

	return tagd::TAGD_OK;
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

		int s_rc = sqlite3_step(_delete_relation_stmt);
		if (s_rc != SQLITE_DONE) {
			RET_SQLITE_FERROR(s_rc, "delete relation failed: %s %s %s",
					subject.c_str(), p.relator.c_str(), p.object.c_str()); 
		}
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_tag(const tagd::id_type& id, session *ssn) {
	assert( !id.empty() );

	this->prepare(&_delete_tag_stmt,
		"DELETE FROM tags WHERE tag = tid(?)",
		"delete tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(&_delete_tag_stmt, 1, id.c_str(), "delete tag id");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_delete_tag_stmt);
	if (s_rc == SQLITE_DONE) {
		return tagd::TAGD_OK;
	} else if (s_rc == SQLITE_CONSTRAINT) {
		if (_trace_on) {
			const char* errmsg = sqlite3_errmsg(_db);
			std::cerr << "SQLITE_CONSTRAINT: " << errmsg << std::endl;
		}
		
		// set error for cause of FK constraint failure
		this->term_pos_occurence(id, ssn, true);

		assert(ssn->code() == tagd::TS_RELATION_DEPENDENCY);
		if (ssn->code() != tagd::TS_RELATION_DEPENDENCY)  // unknown cause of FK constraint failure, should't happen
			RET_SYS_SSN_FERROR(tagd::TS_INTERNAL_ERR, "delete tag unknown dependency failure: %s", id.c_str());
		else  // error(s) set
			return ssn->code();
	} else {
		RET_SYS_SSN_FERROR(tagd::TS_INTERNAL_ERR, "delete tag failed: %s", id.c_str());
	}
}

tagd::code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& sub) {
	if( !(!sub.rank().empty() || (sub.rank().empty() && sub.id() == HARD_TAG_ENTITY)) ) {
		std::cerr << "next_rank: " << sub << "\t-- " << sub.rank().dotted_str() << std::endl;
	}
	assert( !sub.rank().empty() || (sub.rank().empty() && sub.id() == HARD_TAG_ENTITY) );

	this->max_child_rank(next, sub.id());
	tagd::code r_rc;
	if (next.empty()) {
		next = sub.rank();
		r_rc = next.push_back(1);
	} else {
		r_rc = next.increment();
	}

	if (r_rc != tagd::TAGD_OK)
		return this->ferror(tagd::TS_INTERNAL_ERR, "next_rank error: %s", tagd::code_str(r_rc));

	return tagd::TAGD_OK;
}

tagd::part_of_speech sqlite::put_term(const tagd::id_type& t, const tagd::part_of_speech pos) {
	assert( !t.empty() );
	rowid_t term_id;
	tagd::part_of_speech p = this->term_pos(t, &term_id);
	if (p == tagd::POS_UNKNOWN) {
		this->insert_term(t, pos);
		return (this->ok() ? pos : tagd::POS_UNKNOWN);
	} else if (p == pos) {
		return pos;
	} else {
		this->update_term(t, (tagd::part_of_speech)(p | pos));
		return (this->ok() ? (tagd::part_of_speech)(p | pos): tagd::POS_UNKNOWN);
	}
}

std::string format_fts(const tagd::abstract_tag &t) {
	std::stringstream ss;  // captured by lambdas - return val

	auto f_format_fts = [](std::string s) -> std::string {
		if (s[0] == '_')  // hard tag no more
			s.erase(0, 1);

		for (size_t i=0; i<s.size(); i++) {
			switch (s[i]) {
				case '_':
				case '\n':
					s[i] = ' ';
					break;
				default:
					continue;
			}
		}

		return s;
	};

	/*
	auto f_is_digits = [](const std::string &str) {
		return std::all_of(str.begin(), str.end(), ::isdigit);
	};
	*/

	auto f_print_fts = [&](const tagd::id_type& s) {
				ss << f_format_fts(s);
	};

	auto f_print_object = [&](const tagd::predicate& p) {
		/*
		if (f_is_digits(p.modifier)) {
			// hackish formatting of English quantifier before object
			ss << p.modifier << ' ' << p.object;
		} else {
			ss << p.object;
			if (!p.modifier.empty()) {
				ss << ' ' << p.op_c_str() << ' ' << p.modifier;
			}
		}
		*/
		// even more hackish...
		if (!p.modifier.empty())
			ss << p.modifier << ' ' << p.object;
		else
			ss << p.object;
	};

	// urls' sub determined by nature of being a url
	if (!t.super_object().empty() && t.pos() != tagd::POS_URL) {
		f_print_fts(t.id());
		ss << ' ';
		f_print_fts(t.sub_relator());
		ss << ' ';
		f_print_fts(t.super_object());
	} else {
		ss << t.id();
	}

	auto it = t.relations.begin();
	if (it == t.relations.end())
		return ss.str();

	if (t.relations.size() == 1 && t.super_object().empty()) {
		ss << ' ';
		f_print_fts(it->relator);
		ss << ' ';
		f_print_object(*it);
		return ss.str();
	} 

	tagd::id_type last_relator;
	for (; it != t.relations.end(); ++it) {
		if (last_relator == it->relator) {
			ss << ", ";
			f_print_object(*it);
		} else {
			// use wildcard for empty relators
			ss << ' ';
			if (it->relator.empty())
				ss << '*';
			else
				f_print_fts(it->relator);
			ss << ' ';
			f_print_object(*it);
			last_relator = it->relator;
		}
	}

	return ss.str();
}

tagd::code sqlite::insert_fts_tag(const tagd::id_type& id, flags_t flags) {
	assert( !id.empty() );

	tagd::abstract_tag t;
	this->get(t, id, nullptr, flags);
	OK_OR_RET_ERR(); 

	if (_trace_on)
		std::cerr << "insert_fts_tag( " << id << " ): " << format_fts(t) << std::endl;

	this->prepare(&_insert_fts_tag_stmt,
		"INSERT INTO fts_tags (docid, content) VALUES (tid(?), ?)",
		"insert fts_tag"
	);
	OK_OR_RET_ERR(); 

	this->bind_text(&_insert_fts_tag_stmt, 1, id.c_str(), "insert fts_tag docid");
	OK_OR_RET_ERR(); 

	this->bind_text(&_insert_fts_tag_stmt, 2, format_fts(t).c_str(), "insert fts_tag content");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_insert_fts_tag_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "insert fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::update_fts_tag(const tagd::id_type& id, flags_t flags) {
	assert( !id.empty() );

	tagd::abstract_tag t;
	this->get(t, id, nullptr, flags);
	OK_OR_RET_ERR(); 

	if (_trace_on)
		std::cerr << "update_fts_tag( " << id << " ): " << format_fts(t) << std::endl;

	this->prepare(&_update_fts_tag_stmt,
		"UPDATE fts_tags SET content = ? WHERE docid = tid(?)",
		"update fts_tag"
	);
	OK_OR_RET_ERR(); 

	this->bind_text(&_update_fts_tag_stmt, 1, format_fts(t).c_str(), "update fts_tag content");
	OK_OR_RET_ERR(); 

	this->bind_text(&_update_fts_tag_stmt, 2, id.c_str(), "update fts_tag docid");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_update_fts_tag_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "update fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_fts_tag(const tagd::id_type& id) {
	assert( !id.empty() ); this->prepare(&_delete_fts_tag_stmt,
		"DELETE FROM fts_tags WHERE docid = tid(?)",
		"delete fts_tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(&_delete_fts_tag_stmt, 1, id.c_str(), "delete fts_tag docid");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_delete_fts_tag_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
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
		RET_SQLITE_FERROR(s_rc, "insert term failed: %s", t.c_str());

	return tagd::TAGD_OK;
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
		RET_SQLITE_FERROR(s_rc, "update term failed: %s", t.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_term(const tagd::id_type& id) {
	assert( !id.empty() );

	this->prepare(&_delete_term_stmt,
		"DELETE FROM terms WHERE term = ?",
		"delete term"
	);
	OK_OR_RET_ERR();

	this->bind_text(&_delete_term_stmt, 1, id.c_str(), "delete term");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_delete_term_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete term failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::insert(const tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
	if (_trace_on)
		std::cerr << "sqlite::insert " << t << std::endl;

	assert( t.super_object() == destination.id() );
	assert( !t.id().empty() );
	assert( !t.sub_relator().empty() );
	assert( !t.super_object().empty() );

	tagd::rank rank;
	next_rank(rank, destination);
	OK_OR_RET_ERR(); 

	// dout << "insert next rank: (" << rank.dotted_str() << ")" << std::endl;

	this->prepare(&_insert_stmt,
		"INSERT INTO tags (tag, sub_relator, super_object, rank, pos) "
		"VALUES (tid(?), tid(?), tid(?), ?, ?)",
		"insert tag"
	);
	OK_OR_RET_ERR(); 

	tagd::part_of_speech pos;
	if (t.pos() == tagd::POS_UNKNOWN) {
		// use pos of super object
		pos = this->pos(t.super_object(), nullptr);
		assert(pos != tagd::POS_UNKNOWN);
		if (pos == tagd::POS_UNKNOWN) {
			return this->ferror(tagd::TS_INTERNAL_ERR,
				"super_object unknown: %s", t.super_object().c_str());
		}
	} else {
		pos = t.pos();
	}

	int i = 0;
	this->put_term(t.id(), pos);
	this->bind_text(&_insert_stmt, ++i, t.id().c_str(), "insert id");
	OK_OR_RET_ERR(); 

	this->put_term(t.sub_relator(), tagd::POS_SUB_RELATOR);
	this->bind_text(&_insert_stmt, ++i, t.sub_relator().c_str(), "insert sub_relator");
	OK_OR_RET_ERR(); 

	this->put_term(t.super_object(), tagd::POS_SUB_OBJECT);
	this->bind_text(&_insert_stmt, ++i, t.super_object().c_str(), "insert super_object");
	OK_OR_RET_ERR(); 

	this->bind_text(&_insert_stmt, ++i, rank.c_str(), "insert rank");
	OK_OR_RET_ERR(); 

	this->bind_int(&_insert_stmt, ++i, pos, "insert pos");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_insert_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "insert tag failed: %s", t.id().c_str());

	return tagd::TAGD_OK;
}

// update existing with new tag
tagd::code sqlite::update(const tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
	assert( !t.id().empty() );
	assert( !t.sub_relator().empty() );
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
			RET_SQLITE_FERROR(s_rc, "update rank failed: %s", t.id().c_str());
	}

	//update tag
	this->prepare(&_update_tag_stmt, 
			"UPDATE tags SET sub_relator = tid(?), super_object = tid(?) WHERE tag = tid(?)",
			"update tag"
	);
	OK_OR_RET_ERR(); 

	this->bind_text(&_update_tag_stmt, 1, t.sub_relator().c_str(), "update sub_relator");
	OK_OR_RET_ERR(); 

	this->bind_text(&_update_tag_stmt, 2, destination.id().c_str(), "update super_object");
	OK_OR_RET_ERR(); 

	this->bind_text(&_update_tag_stmt, 3, t.id().c_str(), "update tag id");
	OK_OR_RET_ERR(); 

	int s_rc = sqlite3_step(_update_tag_stmt);
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "update tag failed: %s", t.id().c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::insert_relations(const tagd::abstract_tag& t, flags_t flags) {
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

	do {
		this->put_term(t.id(), tagd::POS_SUBJECT);
		this->bind_text(&_insert_relations_stmt, 1, t.id().c_str(), "insert subject");
		OK_OR_RET_ERR(); 

		this->put_term(it->relator, tagd::POS_RELATED);
		this->bind_text(&_insert_relations_stmt, 2, it->relator.c_str(), "insert relator");
		OK_OR_RET_ERR(); 

		this->put_term(it->object, tagd::POS_OBJECT);
		this->bind_text(&_insert_relations_stmt, 3, it->object.c_str(), "insert object");
		OK_OR_RET_ERR(); 

		if (it->modifier.empty()) {
			this->bind_null(&_insert_relations_stmt, 4, "insert NULL modifier");
		} else {
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
				if (!this->exists(it->relator))
					return this->ferror(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
			} else if (ts_sql_rc == TS_SQLITE_FK) {
				// relations.relator FK tags.tag violated
				if (!this->exists(it->relator))
					return this->ferror(tagd::TS_RELATOR_UNK, "unknown relator: %s", it->relator.c_str());
				// relations.object FK tags.tag violated
				if (!this->exists(it->object))
					return this->ferror(tagd::TS_OBJECT_UNK, "unknown object: %s", it->object.c_str());
			} else {  // TS_SQLITE_UNK
				return this->ferror(tagd::TS_INTERNAL_ERR, "insert relations error: %s", errmsg);
			}
		} else {
			if (!(sqlite_constraint_type(sqlite3_errmsg(_db)) == TS_SQLITE_UNIQUE && (flags & F_IGNORE_DUPLICATES))) {
				return this->ferror(tagd::TS_INTERNAL_ERR, "insert relations failed: %s, %s, %s",
										t.id().c_str(), it->relator.c_str(), it->object.c_str());
			}
		}

		sqlite3_reset(_insert_relations_stmt);
		sqlite3_clear_bindings(_insert_relations_stmt);
	} while(++it != t.relations.end());

	if (num_inserted == 0) {
		if (flags & F_IGNORE_DUPLICATES)
			return tagd::TAGD_OK;
		else
			return this->ferror(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
	} else {
		return tagd::TAGD_OK;
	}
}

tagd::code sqlite::insert_referent(const tagd::referent& put_ref, session *ssn, flags_t flags) {
	tagd::referent t;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) {
		t = put_ref;
	} else {
		this->decode_referents(t, put_ref, ssn);
		if (t.id() != put_ref.id())
			t.id(put_ref.id());  // don't transform the refers
	}

	if (t.refers() == t.refers_to())
		RET_SSN_ERROR(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_REFERS_TO)); 

	if ( t.refers().empty() || t.refers() == HARD_TAG_ENTITY
	    || t.refers_to().empty()  // <refers> refers_to _entity -- OK
		|| t.context().empty() || t.context() == HARD_TAG_ENTITY )
	{
		if (ssn) {
			tagd::error e(tagd::TS_MISUSE, "illegal value in referent");

			if (t.refers().empty())
				e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_EMPTY);
			else if (t.refers() == HARD_TAG_ENTITY)
				e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_ENTITY);

			if (t.refers_to().empty())
				e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS_TO, HARD_TAG_EMPTY);

			if (t.context().empty())
				e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_EMPTY);
			else if (t.context() == HARD_TAG_ENTITY)
				e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_ENTITY);

			ssn->error(e);
		}

		return tagd::TS_MISUSE;
	}

	this->prepare(&_insert_referents_stmt,
		"INSERT INTO referents (refers, refers_to, context) "
		"VALUES (tid(?), tid(?), tid(?))",
		"insert_referent"
	);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent");

	this->put_term(t.refers(), tagd::POS_REFERS);
	this->bind_text(&_insert_referents_stmt, 1, t.refers().c_str(), "insert refers");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_refers");

	this->put_term(t.refers_to(), tagd::POS_REFERS_TO);
	this->bind_text(&_insert_referents_stmt, 2, t.refers_to().c_str(), "insert refers_to");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_refers_to");

	if (t.context().empty())
		this->bind_null(&_insert_referents_stmt, 3, "insert NULL context");
	else {
		this->put_term(t.context(), tagd::POS_CONTEXT);
		this->bind_text(&_insert_referents_stmt, 3, t.context().c_str(), "insert context");
	}
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_context");

	int s_rc = sqlite3_step(_insert_referents_stmt);
	
	if (s_rc == SQLITE_DONE) {
		// TODO update fts_tags with referent
		return tagd::TAGD_OK;
	}

	if (s_rc != SQLITE_CONSTRAINT) {
		SQLITE_FERROR(s_rc, "insert referents failed: %s, %s, %s",
			t.refers().c_str(), t.refers_to().c_str(), t.context().c_str());
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:step");
	}

	assert(s_rc == SQLITE_CONSTRAINT);	

	// sqlite does't tell us whether the context or refers_to caused the violation
	const char* errmsg = sqlite3_errmsg(_db);

	if (_trace_on)
		std::cerr << "SQLITE_CONSTRAINT: " << errmsg << std::endl;

	ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
	if (ts_sql_rc == TS_SQLITE_UNIQUE) { // referents UNIQUE violation
		if (flags & F_IGNORE_DUPLICATES)
			return tagd::TAGD_OK;
		else
			RET_SSN_FERROR(tagd::TS_DUPLICATE, "duplicate referent: %s", t.refers().c_str());
	}

	if (ts_sql_rc == TS_SQLITE_FK) {
		if (!this->exists(t.refers_to()))
			RET_SSN_FERROR(tagd::TS_REFERS_TO_UNK, "unknown refers_to: %s", t.refers_to().c_str());

		if (!t.context().empty()) {
			// referents.context FK tags.tag violated
			if (!this->exists(t.context()))
				RET_SSN_FERROR(tagd::TS_CONTEXT_UNK, "unknown context: %s", t.context().c_str());
		}
	}

	// TS_SQLITE_UNK or other unknown errors
	RET_SQLITE_SSN_FERROR(s_rc, "insert referent failed: %s", put_ref.str().c_str());
}

void sqlite::encode_referent(tagd::id_type &to, const tagd::id_type &from, session *ssn) {
	if (from.empty()) return;

	if (this->refers(to, from, ssn) == tagd::TAGD_OK)  // to set
		return;

	to = from;
}

void sqlite::encode_referents(tagd::predicate_set&to, const tagd::predicate_set&from, session* ssn) {
	for (auto it = from.begin(); it != from.end(); ++it) {
		tagd::predicate p;
		this->encode_referent(p.relator, it->relator, ssn);
		this->encode_referent(p.object, it->object, ssn);
		if (!it->modifier.empty())
			this->encode_referent(p.modifier, it->modifier, ssn);
		to.insert(p);
	}
}

void sqlite::encode_referents(tagd::abstract_tag& to, const tagd::abstract_tag& from, session* ssn) {
		// transform referents in put_tag to their "refers_to" given context
		to.pos(from.pos());

		tagd::id_type rt;
		if (!from.id().empty()) {
			this->encode_referent(rt, from.id(), ssn);
			to.id(rt);
		}

		if (!from.super_object().empty()) {
			this->encode_referent(rt, from.super_object(), ssn);
			to.super_object(rt);
		}

		if (!from.rank().empty()) {
			to.rank(from.rank());
		}

		this->encode_referents(to.relations, from.relations, ssn);
}


void sqlite::decode_referent(tagd::id_type &to, const tagd::id_type &from, session* ssn) {
	if (from.empty()) return;

	if (from[0] == '_') { // don't lookup hard tags
		to = from;
		return;
	}

	if (this->refers_to(to, from, ssn) == tagd::TAGD_OK)  // to set
		return;

	to = from;
}

void sqlite::decode_referents(tagd::predicate_set& to, const tagd::predicate_set& from, session *ssn) {
	for (auto it = from.begin(); it != from.end(); ++it) {
		tagd::predicate p = *it; // retain type, opr8r
		this->decode_referent(p.relator, it->relator, ssn);
		this->decode_referent(p.object, it->object, ssn);
		// TODO only modifiers of type TAG should be decoded (not primitive numbers, empty strings, etc.)
		if (!it->modifier.empty())
			this->decode_referent(p.modifier, it->modifier, ssn);
		to.insert(p);
	}
}

void sqlite::decode_referents(tagd::abstract_tag& to, const tagd::abstract_tag& from, session *ssn) {
		// transform referents in put_tag to their "refers_to" given context
		to.pos(from.pos());

		tagd::id_type rt;
		if (!from.id().empty()) {
			this->decode_referent(rt, from.id(), ssn);
			to.id(rt);
		}

		if (!from.sub_relator().empty()) {
			this->decode_referent(rt, from.sub_relator(), ssn);
			to.sub_relator(rt);
		}

		if (!from.super_object().empty()) {
			this->decode_referent(rt, from.super_object(), ssn);
			to.super_object(rt);
		}

		this->decode_referents(to.relations, from.relations, ssn);
}

tagd::code sqlite::related(tagd::tag_set& R, const tagd::predicate& rel, const tagd::id_type& sup, session* ssn, flags_t flags) {
	tagd::predicate p;
	tagd::id_type super_object;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) {
		p = rel;
		super_object = sup;
	} else {
		this->decode_referent(super_object, sup, ssn);
		this->decode_referent(p.relator, rel.relator, ssn);
		this->decode_referent(p.object, rel.object, ssn);
		this->decode_referent(p.modifier, rel.modifier, ssn);
		p.opr8r = rel.opr8r;
	}

	int pos_i = 0;  // binding position

	auto f_bind_null_text = [this, &pos_i](bool not_null, const tagd::id_type &id) {
			if (not_null) {
				this->bind_text(&_related_stmt, ++pos_i, id.c_str(), "test");
				this->bind_text(&_related_stmt, ++pos_i, id.c_str(), "value");
			} else {
				this->bind_null(&_related_stmt, ++pos_i, "null test");
				this->bind_null(&_related_stmt, ++pos_i, "null");
			}
	};

	auto f_bind_null_int_text = [this, &pos_i](bool not_null, const tagd::predicate &p) {
			if (not_null) {
				if (p.modifier_type == tagd::TYPE_TEXT) {
					this->bind_text(&_related_stmt, ++pos_i, p.modifier.c_str(), "test");
					this->bind_text(&_related_stmt, ++pos_i, p.modifier.c_str(), "value");
				} else {
					this->bind_int(&_related_stmt, ++pos_i, atoi(p.modifier.c_str()), "test");
					this->bind_int(&_related_stmt, ++pos_i, atoi(p.modifier.c_str()), "value");
				}
			} else {
				this->bind_null(&_related_stmt, ++pos_i, "null test");
				this->bind_null(&_related_stmt, ++pos_i, "null");
			}
	};

	this->prepare(&_related_stmt, 
		"SELECT idt(subject), idt(sub_relator), idt(super_object), pos, rank, "
		"idt(relator), idt(object), idt(modifier) "
		"FROM tags, relations "
		"WHERE tag = subject "
		"AND ("
			"? IS NULL OR subject IN ( "
			"SELECT tag FROM tags WHERE rank GLOB ( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			") || '*'"
			") "
		") "
		"AND ("
			"? IS NULL OR relator IN ( "
			"SELECT tag FROM tags WHERE rank GLOB ( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			") || '*'"
			") "
		") "
		"AND ("
			"? IS NULL OR object IN ( "
			"SELECT tag FROM tags WHERE rank GLOB ( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			") || '*'"
			") "
		") "
		"AND (? IS NULL OR modifier = tid(?)) "
		"AND (? IS NULL OR modifier IN(SELECT ROWID FROM terms WHERE CAST(term AS INTEGER) > ?)) "
		"AND (? IS NULL OR modifier IN(SELECT ROWID FROM terms WHERE CAST(term AS INTEGER) >= ?)) "
		"AND (? IS NULL OR modifier IN(SELECT ROWID FROM terms WHERE CAST(term AS INTEGER) < ?)) "
		"AND (? IS NULL OR modifier IN(SELECT ROWID FROM terms WHERE CAST(term AS INTEGER) <= ?)) "
		"ORDER BY rank",
		"select related"
	);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related");

	f_bind_null_text(!super_object.empty(), super_object);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:super_object");
	f_bind_null_text(!p.relator.empty(), p.relator); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:relator");
	f_bind_null_text(!p.object.empty(), p.object); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:object");
	f_bind_null_text((!p.modifier.empty() && p.opr8r == tagd::OP_EQ), p.modifier); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:modifier:OP_EQ");
	f_bind_null_int_text((!p.modifier.empty() && p.opr8r == tagd::OP_GT), p); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:modifier:OP_GT");
	f_bind_null_int_text((!p.modifier.empty() && p.opr8r == tagd::OP_GT_EQ), p); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:modifier:OP_GT_EQ");
	f_bind_null_int_text((!p.modifier.empty() && p.opr8r == tagd::OP_LT), p); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:modifier:OP_LT");
	f_bind_null_int_text((!p.modifier.empty() && p.opr8r == tagd::OP_LT_EQ), p); 
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:bind:modifier:OP_LT_EQ");

	const int F_SUBJECT = 0;
	const int F_SUB_REL = 1;
	const int F_SUB_OBJ = 2;
	const int F_POS = 3;
	const int F_RANK = 4;
	const int F_RELATOR = 5;
	const int F_OBJECT = 6;
	const int F_MODIFIER = 7;

	id_transform_func_t f_transform =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) ?  f_passthrough : this->f_encode_referent(ssn);

	R.clear();
	tagd::tag_set::iterator it = R.begin();
	tagd::rank rank;
	int s_rc;

	while ((s_rc = sqlite3_step(_related_stmt)) == SQLITE_ROW) {
		rank.init( (const char*) sqlite3_column_text(_related_stmt, F_RANK));
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(_related_stmt, F_POS);

		tagd::abstract_tag *t;
		if (pos != tagd::POS_URL) {
			t = new tagd::abstract_tag( f_transform((const char*) sqlite3_column_text(_related_stmt, F_SUBJECT)) );
		} else {
			t = new tagd::url();
			((tagd::url*)t)->init_hduri( (const char*) sqlite3_column_text(_related_stmt, F_SUBJECT) );
			if (t->code() != tagd::TAGD_OK) {
				auto tc = t->code();
				if (ssn) {
					ssn->ferror(tc, "failed to init related url: %s",
							(const char*) sqlite3_column_text(_related_stmt, F_SUBJECT) );
				}
				delete t;
				return tc;
			}
		}

		t->sub_relator( f_transform((const char*) sqlite3_column_text(_related_stmt, F_SUB_REL)) );
		t->super_object( f_transform((const char*) sqlite3_column_text(_related_stmt, F_SUB_OBJ)) );
		t->pos(pos);
		t->rank(rank);

		auto pred = tagd::predicate(
			f_transform( (const char*) sqlite3_column_text(_related_stmt, F_RELATOR) ),
			f_transform( (const char*) sqlite3_column_text(_related_stmt, F_OBJECT) )
		);

		if (sqlite3_column_type(_related_stmt, F_MODIFIER) != SQLITE_NULL) {
			pred.modifier = f_transform( (const char*) sqlite3_column_text(_related_stmt, F_MODIFIER) );
		}
		t->relation(pred);

		if (_trace_on)
			std::cerr << "related R.insert: " << *t << std::endl;

		it = R.insert(it, *t);
		delete t;
	}

	if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc,
			"related failed(super_object, relator, object, modifier, opr8r): %s, %s, %s, %s, %s",
			super_object.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str(), p.op_c_str());
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:step");
	}

	return (R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::get_children(tagd::tag_set& R, const tagd::id_type& super_object, session *ssn, flags_t flags) {
	this->prepare(&_get_children_stmt,
		"SELECT idt(tag), idt(sub_relator), idt(super_object), pos, rank "
		"FROM tags WHERE super_object = tid(?) "
		"ORDER BY rank",
		"select children"
	);
	OK_OR_RET_ERR(); 

	this->bind_text(&_get_children_stmt, 1, super_object.c_str(), "get children super_object");
	OK_OR_RET_ERR(); 

	const int F_ID = 0;
	const int F_SUB_REL = 1;
	const int F_SUB_OBJ = 2;
	const int F_POS = 3;
	const int F_RANK = 4;

	id_transform_func_t f_transform =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) ?  f_passthrough : this->f_encode_referent(ssn);

	R.clear();
	tagd::tag_set::iterator it = R.begin();
	int s_rc;

	while ((s_rc = sqlite3_step(_get_children_stmt)) == SQLITE_ROW) {
		tagd::abstract_tag *t;
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(_get_children_stmt, F_POS);
		if (pos != tagd::POS_URL) {
			t = new tagd::abstract_tag(
				f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_ID))
			);
		} else {
			t = new tagd::url();
			((tagd::url*)t)->init_hduri( (const char*) sqlite3_column_text(_get_children_stmt, F_ID) );
			if (t->code() != tagd::TAGD_OK) {
				this->ferror( t->code(), "failed to init related url: %s",
						(const char*) sqlite3_column_text(_get_children_stmt, F_ID) );
				delete t;
				return this->code();
			}
		}
		t->sub_relator( f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_SUB_REL)) );
		t->super_object( f_transform((const char*) sqlite3_column_text(_get_children_stmt, F_SUB_OBJ)) );
		t->pos(pos);
		t->rank( (const char*) sqlite3_column_text(_get_children_stmt, F_RANK) );

		it = R.insert(it, *t);
		delete t;
	}

	if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "get_children failed: %s", super_object.c_str());
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get_children:step");
	}

	return (R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query_referents(tagd::tag_set& R, const tagd::interrogator& intr) {
	if (_trace_on)
		std::cerr << "sqlite::query: " << intr << std::endl;

	tagd::id_type refers, refers_to, context;
	for (auto it = intr.relations.begin(); it != intr.relations.end(); ++it) {
		if (it->relator == HARD_TAG_REFERS)
			refers = it->object;
		else if (it->relator == HARD_TAG_REFERS_TO)
			refers_to = it->object;
		else if (it->relator == HARD_TAG_CONTEXT)
			context = it->object;
	}

	sqlite3_stmt *stmt = nullptr;

	if (refers.empty()) {
		if (refers_to.empty()) {
			if (context.empty()) {
				// all referents
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents",
					"query referent context"
				);
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context.empty()) {
				this->prepare(&stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers_to = tid(?)",
					"query referent refers_to"
				);
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
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
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 2, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				this->bind_text(&stmt, 3, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		}
	}

	const int F_REFERS = 0;
	const int F_REFERS_TO = 1;
	const int F_CONTEXT = 2;

	R.clear();
	int s_rc;

	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		assert(sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL);
		tagd::referent r(
			(const char*) sqlite3_column_text(stmt, F_REFERS),
			(const char*) sqlite3_column_text(stmt, F_REFERS_TO),
			(const char*) sqlite3_column_text(stmt, F_CONTEXT)
		);

		auto pr = R.insert(r);
		assert( pr.second );
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR) {
		RET_SQLITE_FERROR(s_rc, "query referents failed(refers, refers_to, context): %s, %s, %s",
							refers.c_str(), refers_to.c_str(), context.c_str());
	}

	return (R.size() == 0 ? tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query(tagd::tag_set& R, const tagd::interrogator& q, session *ssn, flags_t flags) {
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	//TODO use the id (who, what, when, where, why, how_many...)
	// to distinguish types of queries
	assert(!q.empty());

	tagd::interrogator intr;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
		intr = q;
	else
		this->decode_referents(intr, q, ssn);
	OK_OR_RET_SSN_INT_ERR_ACTION("tadb:query:decode_referents");

	if (intr.super_object() == HARD_TAG_REFERENT)
		RET_SSN_CODE(this->query_referents(R, intr));

	if (intr.relations.empty()) {
		if (intr.super_object().empty())
			RET_SSN_ERROR(tagd::TS_MISUSE, "interrogator with empty relations and empty super_object");
		else
			RET_SSN_CODE(this->get_children(R, intr.super_object(), ssn, flags));
	}

	size_t n = 0;
	tagd::tag_set S;  // related per predicate
	for (auto p : intr.relations) {
		S.clear();

		if (p.object == HARD_TAG_TERMS) {
			this->search(S, p.modifier, flags);
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:query:search");
		} else {
			// only super_object is advantagious in related query (sub_relator not needed) 
			this->related(S, p, intr.super_object(), ssn, flags);
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:query:related");
		}

		if (_trace_on) {
			if (p.object == HARD_TAG_TERMS)
				std::cerr << "search: " << p.modifier << std::endl;
			else  // TODO predicate iostream op
				std::cerr << "related: " << p << std::endl;
			tagd::print_tag_ids(S, std::cerr);
			std::cerr << std::endl;
		}

		OK_OR_RET_ERR();
		n += merge_containing_tags(R, S);
	}


	if (n == 0)
		RET_SSN_CODE(tagd::TS_NOT_FOUND);

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::search(tagd::tag_set& R, const std::string &terms, flags_t flags) {
	//TODO use the id (who, what, when, where, why, how_many...)
	// to distinguish types of queries

	if (terms.empty())
		return tagd::TS_NOT_FOUND;

	this->prepare(&_search_stmt,
		"SELECT idt(docid) FROM fts_tags "
		"WHERE content MATCH ?",
		"search"
	);
	OK_OR_RET_ERR();

	this->bind_text(&_search_stmt, 1, terms.c_str(), "search terms");
	OK_OR_RET_ERR();

	const int F_TAG_ID = 0;

	if (_trace_on)
		std::cerr << "search: " << terms << std::endl;

	int s_rc;
	size_t n = 0;
	while ((s_rc = sqlite3_step(_search_stmt)) == SQLITE_ROW) {
			std::string tag_id( (const char*) sqlite3_column_text(_search_stmt, F_TAG_ID) );
			tagd::abstract_tag t;
			if ( this->get(t, tag_id, nullptr, flags) == tagd::TAGD_OK ) {
				R.insert(t);
				n++;
			} else {
				return this->ferror( tagd::TAGD_ERR, "search result failed(%s): %s", tag_id.c_str(), terms.c_str() );
			}
	}

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "search failed: %s", terms.c_str());

	if (n == 0)
		return tagd::TS_NOT_FOUND;

	return tagd::TAGD_OK;
}


tagd::code sqlite::dump_grid(std::ostream& os) {
	this->reset(nullptr);

	sqlite3_stmt *stmt = nullptr;
	tagd::code tc = this->prepare(&stmt,
		"SELECT idt(tag), idt(sub_relator), idt(super_object), rank, pos FROM tags ORDER BY rank",
		"dump grid"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

	const int F_ID = 0;
	const int F_SUB_REL = 1;
	const int F_SUB_OBJ = 2;
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
		   << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUB_REL)
		   << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_SUB_OBJ)
		   << std::setw(colw) << std::left << pos_str(pos)
		   << std::setw(colw) << std::left
		   << tagd::rank::dotted_str( (const char*) sqlite3_column_text(stmt, F_RANK))
		   << std::endl; 
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump grid failed");

	stmt = nullptr;
	tc = this->prepare(&stmt,
		"SELECT idt(refers), idt(refers_to), idt(context) "
		"FROM referents "
		"ORDER BY context",
		"dump referent grid"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

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
		RET_SQLITE_FERROR(s_rc, "dump referent grid failed");

	return tagd::TAGD_OK;
}

tagd::code sqlite::dump_terms(std::ostream& os) {
	this->reset(nullptr);

	sqlite3_stmt *stmt = nullptr;
	tagd::code tc = this->prepare(&stmt,
		"SELECT term, term_pos, ROWID FROM terms",
		"dump terms"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

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

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump terms failed");

	return tagd::TAGD_OK;
}

tagd::code sqlite::dump(std::ostream& os) {
	this->reset(nullptr);

	// dump tag identities before relations, so that they are
	// all known by the time relations are added
	sqlite3_stmt *stmt = nullptr;
	tagd::code tc = this->prepare(&stmt,
		"SELECT idt(tag), idt(sub_relator), idt(super_object), pos "
		"FROM tags "
		"ORDER BY rank",
		"dump tags"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

	const int F_ID = 0;
	const int F_SUB_REL = 1;
	const int F_SUB_OBJ = 2;
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
				(const char*) sqlite3_column_text(stmt, F_SUB_REL),
				(const char*) sqlite3_column_text(stmt, F_SUB_OBJ),
				pos );
		os << ">> " << t << std::endl << std::endl;
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump tags failed");

	// dump relations
	stmt = nullptr;
	tc = this->prepare(&stmt,
		"SELECT idt(subject), idt(relator), idt(object), idt(modifier), pos "
		"FROM relations, tags "
		"WHERE subject = tag "
		"ORDER BY rank",
		"dump relations"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

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
				os << ">> " << *t << std::endl << std::endl; 
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
		os << ">> " << *t << std::endl;
		delete t;
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump relations failed");

	stmt = nullptr;
	tc = this->prepare(&stmt,
		"SELECT idt(refers), idt(refers_to), idt(context) "
		"FROM referents "
		"ORDER BY refers",
		"dump referents"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

	const int F_REFERS = 0;
	const int F_REFERS_TO = 1;
	const int F_CONTEXT = 2;

	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		assert(sqlite3_column_type(stmt, F_CONTEXT) != SQLITE_NULL);

		tagd::referent r{
			(const char*) sqlite3_column_text(stmt, F_REFERS),
			(const char*) sqlite3_column_text(stmt, F_REFERS_TO),
			(const char*) sqlite3_column_text(stmt, F_CONTEXT)
		};

		os << std::endl << ">> " << r << std::endl; 
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump referents failed");

	return tagd::TAGD_OK;
}

tagd::code sqlite::dump_search(std::ostream& os) {
	this->reset(nullptr);

	sqlite3_stmt *stmt = nullptr;
	tagd::code tc = this->prepare(&stmt,
		"SELECT docid, idt(docid), content FROM fts_tags",
		"dump search"
	);
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}

	const int F_DOCID = 0;
	const int F_TAG_ID = 1;
	const int F_CONTENT = 2;

	int s_rc;
	const int colw = 20;
	os << std::setw(colw) << std::left << "-- tag_id"
	   << std::setw(colw) << std::left << "docid"
	   << std::setw(colw) << std::left << "content"
	   << std::endl;
	os << std::setw(colw) << std::left << "---------"
	   << std::setw(colw) << std::left << "-----"
	   << std::setw(colw) << std::left << "-------"
	   << std::endl;

	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		os << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_TAG_ID) 
		   << std::setw(colw) << std::left << sqlite3_column_int64(stmt, F_DOCID) 
		   << std::setw(colw) << std::left << sqlite3_column_text(stmt, F_CONTENT)
		   << std::endl; 
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump search failed");

	return tagd::TAGD_OK;
}


tagd::code sqlite::max_child_rank(tagd::rank& next, const tagd::id_type& super_object) {
	this->open();
	OK_OR_RET_ERR();

	int s_rc = this->prepare(&_max_child_rank_stmt,
		"SELECT rank FROM tags WHERE super_object = tid(?) ORDER BY rank DESC LIMIT 1",
		"child ranks"
	);

	this->bind_text(&_max_child_rank_stmt, 1, super_object.c_str(), "rank super_object");
	OK_OR_RET_ERR();

	const int F_RANK = 0;
	tagd::code rc;

	s_rc = sqlite3_step(_max_child_rank_stmt);

	if (s_rc == SQLITE_ROW) {
		rc = next.init( (const char*) sqlite3_column_text(_max_child_rank_stmt, F_RANK));
		switch (rc) {
			case tagd::TAGD_OK:
				break;
			case tagd::RANK_EMPTY:
				// _entity is the only tag having NULL rank
				if (super_object != HARD_TAG_ENTITY) {
					return this->error(tagd::TS_INTERNAL_ERR, "integrity error: NULL rank");
				}
				next.clear();  // ensure its empty
				break;
			default:
				return this->ferror(tagd::TS_INTERNAL_ERR, "get_childs rank.init() error: %s", tagd::code_str(rc));
		}
	} else {
		next.clear();  // ensure its empty
	}

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "max_child_rank failed: %s", super_object.c_str());

	return tagd::TAGD_OK;
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
				if (super_object == HARD_TAG_ENTITY) {
					// _entity is the only tag having NULL rank
					// don't insert _entity in the rank_set,
					// only its children
					continue;
				} else {
					return this->error(tagd::TS_INTERNAL_ERR, "integrity error: NULL rank");
				}
				break;
			default:
				return this->ferror(tagd::TS_INTERNAL_ERR, "get_childs rank.init() error: %s", tagd::code_str(rc));
		}

		R.insert(rank);
	}

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "childe_ranks failed: %s", super_object.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::exec(const char *sql, const char *label) {
	this->open();

	char *msg = nullptr;

	int s_rc = sqlite3_exec(_db, sql, nullptr, nullptr, &msg);
	std::stringstream ss;
	if (s_rc != SQLITE_OK) {
		if (label != nullptr)
			ss << "'" << label << "` failed: ";
		else
			ss << "exec failed: " << msg;
		//ss << std::endl << "sql: " << std::endl << sql << std::endl;
		sqlite3_free(msg);
		return this->error(tagd::TS_INTERNAL_ERR, ss.str());
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::exec_mprintf(const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	va_end (args);

	char *sql = sqlite3_vmprintf(fmt, args);
	auto tc = this->exec(sql);
	sqlite3_free(sql);

	return tc;
}

tagd::code sqlite::prepare(sqlite3_stmt **stmt, const char *sql, const char *label) {
	if (*stmt == nullptr) {
		int s_rc = sqlite3_prepare_v2(
			_db, 
			sql,
			-1,         // -1 copies \0 term string, otherwise use nbytes
			stmt,      // OUT: Statement handle
			nullptr        // OUT: Pointer to unused portion of zSql
		);
		if (s_rc != SQLITE_OK) {
			*stmt = nullptr;
			if (label != nullptr)
				return this->ferror(tagd::TS_INTERNAL_ERR, "'%s` prepare statement failed: %s", label, sqlite3_errmsg(_db));
			else
				return this->ferror(tagd::TS_INTERNAL_ERR, "prepare statement failed: %s", sqlite3_errmsg(_db));
		}
	} else {
		sqlite3_reset(*stmt);
		sqlite3_clear_bindings(*stmt);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_text(sqlite3_stmt**stmt, int i, const char *text, const char*label) {
	int s_rc;
	if ((s_rc = sqlite3_bind_text(*stmt, i, text, -1, SQLITE_TRANSIENT)) != SQLITE_OK) {
		finalize_stmt(stmt);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind_text returned %s for label %s: %s", sqlite_err_code_str(s_rc), label, text);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_int(sqlite3_stmt**stmt, int i, int val, const char*label) {
	if (sqlite3_bind_int(*stmt, i, val) != SQLITE_OK) {
		finalize_stmt(stmt);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_rowid(sqlite3_stmt**stmt, int i, rowid_t id, const char*label) {
	if (id <= 0)  // interpret as NULL
		return this->bind_null(stmt, i, label);
 
	if (sqlite3_bind_int64(*stmt, i, id) != SQLITE_OK) {
		finalize_stmt(stmt);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_null(sqlite3_stmt**stmt, int i, const char*label) {
	if (sqlite3_bind_null(*stmt, i) != SQLITE_OK) {
		finalize_stmt(stmt);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

#define FINALIZE(S) \
	sqlite3_finalize(S); \
	S = nullptr;

void sqlite::finalize() {
	FINALIZE(_get_stmt);
	FINALIZE(_exists_stmt);
	FINALIZE(_term_pos_stmt);
	FINALIZE(_term_id_pos_stmt);
	FINALIZE(_pos_stmt);
	FINALIZE(_refers_to_stmt);
	FINALIZE(_refers_stmt);
	FINALIZE(_insert_term_stmt);
	FINALIZE(_update_term_stmt);
	FINALIZE(_delete_term_stmt);
	FINALIZE(_insert_fts_tag_stmt);
	FINALIZE(_update_fts_tag_stmt);
	FINALIZE(_delete_fts_tag_stmt);
	FINALIZE(_search_stmt);
	FINALIZE(_insert_stmt);
	FINALIZE(_update_tag_stmt);
	FINALIZE(_update_ranks_stmt);
	FINALIZE(_child_ranks_stmt);
	FINALIZE(_max_child_rank_stmt);
	FINALIZE(_insert_relations_stmt);
	FINALIZE(_insert_referents_stmt);
	FINALIZE(_delete_tag_stmt);
	FINALIZE(_delete_subject_relations_stmt);
	FINALIZE(_delete_relation_stmt);
	FINALIZE(_delete_refers_to_stmt);
	FINALIZE(_term_pos_occurence_stmt);
	FINALIZE(_get_relations_stmt);
	FINALIZE(_related_stmt);
	FINALIZE(_get_children_stmt);
}

} // namespace tagdb
