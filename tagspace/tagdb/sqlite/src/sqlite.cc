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

struct tagdb_debug_scope {
	tagdb::sqlite *_tdb;
	tagdb::session *_ssn;

	tagdb_debug_scope(tagdb::sqlite *tdb, tagdb::session *ssn) : _tdb{tdb}, _ssn{ssn} {}

	~tagdb_debug_scope() {
		TAGDB_LOG_DEBUG("tagdb code=" << tagd::code_str(_ssn ? _ssn->code() : _tdb->code()));
	}
};

static tagd::rank make_rank(const char *bytes) {
	tagd::rank rank;
	(void)rank.init(bytes);
	return rank;
}

static tagd::code populate_row_id(
	tagd::id_string& populated_id,
	const char *raw_id,
	tagd::part_of_speech pos,
	const tagdb::id_transform_func_t& f_transform
) {
	if (pos == tagd::POS_URL) {
		tagd::HDURI hduri(raw_id);
		if (!hduri.ok())
			return hduri.code();

		populated_id = hduri.id();
		return tagd::TAGD_OK;
	}

	populated_id = f_transform(raw_id);
	return tagd::TAGD_OK;
}

static const char *tagdb_command_prefix(tagd::command_t cmd) {
	switch (cmd) {
		case tagd::CMD_PUT:
			return ">> ";
		case tagd::CMD_DELETE:
			return "!! ";
		default:
			assert(false);
			return "";
	}
}

static const char *tagdb_command_event_type(tagd::command_t cmd) {
	switch (cmd) {
		case tagd::CMD_PUT:
			return HARD_TAG_TAGDB_PUT_EVENT.data();
		case tagd::CMD_DELETE:
			return HARD_TAG_TAGDB_DEL_EVENT.data();
		default:
			assert(false);
			return HARD_TAG_TAGDB_EVENT.data();
	}
}

struct tagdb_mutation_log_scope {
	tagdb::sqlite *_tdb;
	tagdb::session *_ssn;
	tagd::command_t _cmd;
	std::string _msg;

	tagdb_mutation_log_scope(tagdb::sqlite *tdb, tagdb::session *ssn, tagd::command_t cmd, const tagd::abstract_tag& t) :
		_tdb{tdb}, _ssn{ssn}, _cmd{cmd}
	{
		assert(cmd == tagd::CMD_PUT || cmd == tagd::CMD_DELETE);

		std::ostringstream os;
		os << tagdb_command_prefix(cmd);
		os << t;
		_msg = os.str();
	}

	tagd::code code() const {
		return (_ssn ? _ssn->code() : _tdb->code());
	}

	void log(tagd::log_level lvl) const {
		// Validation rolls its writes back and must not emit committed-mutation events.
		if (_msg.empty() || _tdb->checking_put())
			return;

		std::ostringstream os;
		os << _msg << '\n'
		   << "-- tagdb code=" << tagd::code_str(this->code());
		TAGDB_LOG(std::string(HARD_TAG_ROLE_TAGDB), lvl, os.str());
		TAGDB_LOG_EVENT(_ssn, lvl, tagdb_command_event_type(_cmd));
	}
};

/*
 * Scope guard for committed mutation logging: emit the canonical TAGL
 * operation and result code only when the enclosing put/del succeeds.
 */
struct tagdb_notice_scope : public tagdb_mutation_log_scope {
	tagdb_notice_scope(tagdb::sqlite *tdb, tagdb::session *ssn, tagd::command_t cmd, const tagd::abstract_tag& t) :
		tagdb_mutation_log_scope(tdb, ssn, cmd, t) {}

	~tagdb_notice_scope() {
		if (this->code() != tagd::TAGD_OK)
			return;

		this->log(tagd::log_level::NOTICE);
	}
};

/*
 * Scope guard for failed mutation logging: emit the attempted TAGL
 * operation and result code only when the enclosing put/del fails.
 */
struct tagdb_error_scope : public tagdb_mutation_log_scope {
	tagdb_error_scope(tagdb::sqlite *tdb, tagdb::session *ssn, tagd::command_t cmd, const tagd::abstract_tag& t) :
		tagdb_mutation_log_scope(tdb, ssn, cmd, t) {}

	~tagdb_error_scope() {
		if (this->code() == tagd::TAGD_OK)
			return;

		this->log(tagd::log_level::ERROR);
	}
};

static void finalize_stmt(sqlite3_stmt *&stmt) {
	sqlite3_finalize(stmt);
	stmt = nullptr;
}

/*
 * Static free functions for temporary (non-cached) statement patterns in _init and dump functions.
 * These are translation-unit implementation details; the class header stays clean.
 */
static tagd::code bind_null_stmt(tagdb::sqlite *self, sqlite3_stmt *&stmt, int i, const char *label) {
	if (sqlite3_bind_null(stmt, i) != SQLITE_OK) {
		finalize_stmt(stmt);
		return self->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}
	return tagd::TAGD_OK;
}

static tagd::code bind_text_stmt(tagdb::sqlite *self, sqlite3_stmt *&stmt, int i, const char *text, const char *label) {
	int s_rc;
	if ((s_rc = sqlite3_bind_text(stmt, i, text, -1, SQLITE_TRANSIENT)) != SQLITE_OK) {
		finalize_stmt(stmt);
		return self->ferror(tagd::TS_INTERNAL_ERR, "bind_text returned %s for label %s: %s",
				tagdb::sqlite::sqlite_err_code_str(s_rc), label, text);
	}
	return tagd::TAGD_OK;
}

static tagd::code bind_int_stmt(tagdb::sqlite *self, sqlite3_stmt *&stmt, int i, int val, const char *label) {
	if (sqlite3_bind_int(stmt, i, val) != SQLITE_OK) {
		finalize_stmt(stmt);
		return self->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}
	return tagd::TAGD_OK;
}

static tagd::code bind_rowid_stmt(tagdb::sqlite *self, sqlite3_stmt *&stmt, int i, tagdb::rowid_t id, const char *label) {
	if (id <= 0)
		return bind_null_stmt(self, stmt, i, label);
	if (sqlite3_bind_int64(stmt, i, id) != SQLITE_OK) {
		finalize_stmt(stmt);
		return self->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}
	return tagd::TAGD_OK;
}

static tagd::code prepare_stmt(tagdb::sqlite *self, sqlite3 *db, sqlite3_stmt **stmt, const char *sql, const char *label) {
	if (*stmt == nullptr) {
		int s_rc = sqlite3_prepare_v2(db, sql, -1, stmt, nullptr);
		if (s_rc != SQLITE_OK) {
			*stmt = nullptr;
			if (label != nullptr)
				return self->ferror(tagd::TS_INTERNAL_ERR, "'%s` prepare statement failed: %s", label, sqlite3_errmsg(db));
			else
				return self->ferror(tagd::TS_INTERNAL_ERR, "prepare statement failed: %s", sqlite3_errmsg(db));
		}
	} else {
		sqlite3_reset(*stmt);
		sqlite3_clear_bindings(*stmt);
	}
	return tagd::TAGD_OK;
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
			/* TODO(cpp23-return-contracts): keep (void) discard only for this \
			 * explicitly best-effort error-decoration path, or replace it with a \
			 * named helper that makes the lossy policy obvious. */ \
			if (RC == SQLITE_ERROR) \
				(void)sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
			this->error(sqlite_err); \
	}

#define RET_SQLITE_FERROR(RC, ...) {\
		auto sqlite_err = tagd::error::ferror(tagd::TS_INTERNAL_ERR, __VA_ARGS__); \
		/* TODO(cpp23-return-contracts): same best-effort decoration issue as \
		 * SQLITE_FERROR; make the intentional discard self-describing. */ \
		if (RC == SQLITE_ERROR) \
			(void)sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
		return this->error(sqlite_err); \
	}

#define RET_SQLITE_SSN_FERROR(RC, ...) {\
		auto sqlite_err = tagd::error::ferror(tagd::TS_INTERNAL_ERR, __VA_ARGS__); \
		if (ssn) ssn->error(sqlite_err); \
		/* TODO(cpp23-return-contracts): same best-effort decoration issue as \
		 * SQLITE_FERROR; prefer a named helper over repeated raw discards. */ \
		if (RC == SQLITE_ERROR) \
			(void)sqlite_err.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db)); \
		return this->error(sqlite_err); \
	}

namespace tagdb {

id_transform_func_t f_passthrough = [](const tagd::id_string& id) -> const tagd::id_string& { return id; };

sqlite::~sqlite() {
	this->close();
}

/*
 * TODO
 * profile_callback
 * void *sqlite3_profile(sqlite3*, void(*xProfile)(void*,const char*,sqlite3_uint64), void*);
 */


tagd::code sqlite::init(const std::string& fname, bool create_missing) {
	mutation_scope mutation(*this);
	_create_missing = create_missing;
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
	_write_poisoned = false;
	(void)this->open();	// open will create db if not exists
	OK_OR_RET_ERR();

	if (check_storage_schema() != tagd::TAGD_OK)
		return _code;

	this->exec("BEGIN");

	if (_code == tagd::TAGD_OK)
		this->create_terms_table();

	if (_code == tagd::TAGD_OK)
		this->create_tags_table();

	if (_code == tagd::TAGD_OK)
		this->create_relations_table();

	if (_code == tagd::TAGD_OK) {
		this->exec("CREATE TABLE IF NOT EXISTS type_relations ("
			"subject INTEGER NOT NULL, type_relator INTEGER NOT NULL, type_object INTEGER NOT NULL, "
			"PRIMARY KEY(subject,type_relator,type_object) ON CONFLICT FAIL, "
			"FOREIGN KEY(subject) REFERENCES tags(tag), "
			"FOREIGN KEY(type_relator) REFERENCES tags(tag), "
			"FOREIGN KEY(type_object) REFERENCES tags(tag))");
	}
	if (_code == tagd::TAGD_OK) {
		this->exec("CREATE INDEX IF NOT EXISTS idx_sub_relator ON tags(sub_relator)");
	}
	if (_code == tagd::TAGD_OK) {
		this->exec("CREATE INDEX IF NOT EXISTS idx_type_object ON type_relations(type_object)");
	}
	if (_code == tagd::TAGD_OK) {
		this->exec("CREATE INDEX IF NOT EXISTS idx_type_relator ON type_relations(type_relator)");
	}
	if (_code == tagd::TAGD_OK) {
		this->exec_mprintf("PRAGMA user_version = %d", SCHEMA_VERSION);
	}
	if (_code == tagd::TAGD_OK)
		this->create_referents_table();

	if (_code == tagd::TAGD_OK)
		this->create_fts_tags_table();

	/*
	 * We have to insert _entity and _sub manually because of the FK on _sub_relator
	 * The rest of the hard tags will be inserted by bootstrap
	 * UNIQUE constraints will be ignored
	 */
	if (_code == tagd::TAGD_OK) {

		sqlite3_stmt *stmt = nullptr;
		prepare_stmt(this, _db, &stmt,
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

			bind_rowid_stmt(this, stmt, 1, i, "insert hard_tag rowid");
			bind_text_stmt(this, stmt, 2, hard_tag_rows[i], "insert hard_tag term");
			bind_int_stmt(this, stmt, 3, term_pos, "insert hard_tag term_pos");

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
				HARD_TAG_ENTITY.data(), HARD_TAG_SUB.data(), HARD_TAG_ENTITY.data(), tagd::POS_TAG
			);
		}

		if ( _code == tagd::TAGD_OK ) {
			stmt = nullptr;
			prepare_stmt(this, _db, &stmt,
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

				bind_text_stmt(this, stmt, 1, t.id().c_str(), "insert hard_tag id");
				bind_text_stmt(this, stmt, 2, t.sub_relator().c_str(), "insert hard_tag sub_relator");
				bind_text_stmt(this, stmt, 3, t.super_object().c_str(), "insert hard_tag super_object");
				if (t.rank().empty()) {
					assert(t.id() == HARD_TAG_ENTITY);
					bind_null_stmt(this, stmt, 4, "insert hard_tag rank");
				} else {
					bind_text_stmt(this, stmt, 4, t.rank().c_str(), "insert hard_tag rank");
				}
				bind_int_stmt(this, stmt, 5, t.pos(), "insert hard_tag pos");

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
			return this->error(tagd::TS_INTERNAL_ERR, "database PRAGMA foreign_keys failed");
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

		tagd::id_string term(
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

	// Rank characters are literal components, including GLOB metacharacters.
	auto f_rank_glob = [](sqlite3_context* context, int, sqlite3_value** argv) {
		const auto* prefix = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
		if (!prefix) {
			sqlite3_result_null(context);
			return;
		}

		std::string pattern;
		for (const char c : std::string_view(prefix)) {
			if (c == '*' || c == '?' || c == '[') {
				pattern += '[';
				pattern += c;
				pattern += ']';
			} else {
				pattern += c;
			}
		}
		pattern += '*';
		sqlite3_result_text(context, pattern.data(), pattern.size(), SQLITE_TRANSIENT);
	};

	int rc = sqlite3_create_function(_db, "rank_glob", 1,
		SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, f_rank_glob, nullptr, nullptr);
	if (rc != SQLITE_OK) {
		return error(tagd::TS_INTERNAL_ERR, "create function rank_glob failed");
	}

	rc = sqlite3_create_function(_db, "tid", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC, this, f_tid, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function tid failed");

	rc = sqlite3_create_function(_db, "idt", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC, this, f_idt, 0, 0);
	if( rc != SQLITE_OK )
		return this->error(tagd::TS_INTERNAL_ERR, "create function idt failed");

	// check db
	sqlite3_stmt *stmt = nullptr;
	prepare_stmt(this, _db, &stmt,
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
	prepare_stmt(this, _db, &stmt,
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
		tagd::abstract_tag t;
		(void)this->get(t, HARD_TAG_ENTITY, nullptr, F_NO_TRANSFORM_REFERENTS);
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
		/*
		 * binary collation defeats LIKE wildcard partial indexes,
		 * but enables indexing for GLOB style patterns
		 */
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
	prepare_stmt(this, _db, &stmt,
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
		/*
		 * whatever statement throws a unique constraint will fail
		 * but the trasaction overall can still succeed
		 */
		"PRIMARY KEY (refers, refers_to) ON CONFLICT FAIL, "
		"UNIQUE(refers, context), "
		"FOREIGN KEY (refers_to) REFERENCES tags(tag), "
		"FOREIGN KEY (context) REFERENCES tags(tag)"
	")"
	);
	OK_OR_RET_ERR();

	/*
	 * UNIQUE index on (refers, context) only enforces non-NULL
	 * values, so we have to create our own check
	 * Only one distinct refers value can refer_to a NULL context
	 */
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
	prepare_stmt(this, _db, &stmt,
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
	prepare_stmt(this, _db, &stmt,
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
		// Preserve the existing predicate metadata; the shared primitive-value redesign remains separate.
		"modifier_type INTEGER NOT NULL, operator INTEGER NOT NULL, "
		/*
		 * whatever statement throws a unique constraint will fail
		 * but the trasaction overall can still succeed
		 */
		"PRIMARY KEY (subject, relator, object) ON CONFLICT FAIL, " // makes unique
		"FOREIGN KEY (subject) REFERENCES tags(tag), "
		"FOREIGN KEY (relator) REFERENCES tags(tag), "
		"FOREIGN KEY (object) REFERENCES tags(tag)"
	")"
	);
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_subject ON relations(subject)");
	OK_OR_RET_ERR();

	/*
	 * relator index added because of use in _term_pos_occurence_stmt
	 * TODO look into optimizing
	 */
	this->exec("CREATE INDEX idx_relator ON relations(relator)");
	OK_OR_RET_ERR();

	this->exec("CREATE INDEX idx_object ON relations(object)");
	OK_OR_RET_ERR();

	/*
	 * relator index added because of use in _term_pos_occurence_stmt
	 * TODO look into optimizing
	 */
	this->exec("CREATE INDEX idx_modifier ON relations(modifier)");

	return _code;
}

tagd::code sqlite::open() {
	if (_db != nullptr)
		return tagd::TAGD_OK;

	/*
	 * tagspace decides whether creation is allowed. Keep that decision when
	 * reopening, so SQLite cannot silently replace a missing database file.
	 */
	int rc = sqlite3_open_v2(_db_fname.c_str(), &_db,
		SQLITE_OPEN_READWRITE | (_create_missing ? SQLITE_OPEN_CREATE : 0), nullptr);
	if( rc != SQLITE_OK ){
		this->close();
		RET_SQLITE_FERROR(tagd::TS_INTERNAL_ERR, "open database failed");
	}
	_code = tagd::TAGD_OK;

	if ( this->exec("PRAGMA temp_store = MEMORY") != tagd::TAGD_OK)
		return this->error(tagd::TS_INTERNAL_ERR, "database PRAGMA temp_store failed");

	return this->code(tagd::TAGD_OK);
}

void sqlite::close() {
	clear_cache();
	_cache_data_version = -1;
	if (_db == nullptr)
		return;

	this->finalize();
	auto rc = sqlite3_close(_db);
	if (rc) {
		LOG_ERROR( "error: sqlite3_close() returned "
				<< rc << ": " << sqlite3_errmsg(_db) << std::endl )
	}
	_db = nullptr;
}

tagd::code sqlite::format_declaration(tagd::abstract_tag& result,
	const tagd::abstract_tag& canonical, session* ssn, flags_t flags) {
	// GET and QUERY share URL/referent presentation without re-reading canonical declarations.
	if (tagd::hard_tag_axiom_for(canonical.id())) {
		return hard_tag::get(result, canonical.id());
	}

	const bool transform = ssn && !(flags & F_NO_TRANSFORM_REFERENTS);
	id_transform_func_t f_transform = transform ? f_encode_referent(ssn) : f_passthrough;
	tagd::id_string displayed_id;
	if (canonical.pos() == tagd::POS_URL) {
		tagd::HDURI u(canonical.id());
		if (!u.ok()) {
			return ferror(u.code(), "failed to init HDURI: %s", canonical.id().c_str());
		}
		displayed_id = u.id();
	} else {
		displayed_id = f_transform(canonical.id());
	}

	tagd::abstract_tag semantic(displayed_id, f_transform(canonical.sub_relator()),
		f_transform(canonical.super_object()), canonical.pos());
	tagd::abstract_tag populated = canonical.rank().empty()
		? std::move(semantic) : tagd::abstract_tag(semantic, canonical.rank());
	populated.relations = canonical.relations;
	populated.type_relations = canonical.type_relations;
	if (transform) {
		populated.relations.transform(f_transform);
		populated.type_relations.transform(f_transform);
	}

	/*
	 * A displayed alias keeps a universal reference to its canonical identity.
	 * A URL is displayed from its HDURI, which is its stored ID and collation key,
	 * not a referent.
	 */
	if (!(flags & F_NO_TRANSFORM_REFERENTS) && canonical.pos() != tagd::POS_URL
		&& canonical.id() != populated.id()) {
		tagd::predicate_set marker{{HARD_TAG_REFERS_TO, canonical.id()}};
		const auto rc = order_predicates(marker);
		if (rc != tagd::TAGD_OK) {
			return rc;
		}
		populated.relations.insert(marker.begin(), marker.end());
	}

	result = std::move(populated);
	return tagd::TAGD_OK;
}

tagd::code sqlite::get(tagd::abstract_tag& t, tagd::id_view term, session* ssn, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	if (!(flags & F_NO_RESET)) this->reset(ssn);
	tagdb_debug_scope debug_scope(this, ssn);
	tagd::id_string term_id(term);

	TAGDB_LOG_DEBUG( "sqlite::get: " << term << std::endl );
	TAGDB_LOG_DEBUG("tagdb get subject=" << term);
	TAGDB_LOG_EVENT(ssn, tagd::log_level::INFO, std::string(HARD_TAG_TAGDB_GET_EVENT));

	if (!term.empty() && term[0] == '_') {
		tagd::code tc = hard_tag::get(t, term);
		if (tc == tagd::TAGD_OK) {
			TAGDB_LOG_DEBUG( "got hard_tag: " << t << " -- " << t.rank().dotted_str() << std::endl );
			if ((flags & F_INCLUDE_INHERITED_PREDICATES)
			&& _relation_semantics == relation_semantics::assertions) {
				relation_resolver state;
				tc = load_relation_declarations(state, {term_id});
				if (tc == tagd::TAGD_OK) {
					tc = expand_relations(t, term_id, state, ssn, flags);
				}
			}
			RET_SSN_CODE(tc);
		} else if (tc != tagd::TS_NOT_FOUND) {
			RET_SYS_SSN_FERROR(tc, "hard_tag::get(%s) unexpected tag code: %s",
				t.id().c_str(), tagd::code_str(tc));
		}
	}

	tagd::part_of_speech term_pos = this->term_pos(term);
	TAGDB_LOG_DEBUG( "term_pos(" << term << "): " << pos_list_str(term_pos) << std::endl );

	if (term_pos == tagd::POS_UNKNOWN) {
		if (flags & F_NO_NOT_FOUND_ERROR) {
			return tagd::TS_NOT_FOUND;
		} else {
			RET_SSN_ERROR(tagd::TS_NOT_FOUND,
				tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term_id) );
		}
	}

	tagd::id_string id;
	if (ssn && !(flags & F_NO_TRANSFORM_REFERENTS) && (term_pos & tagd::POS_REFERS)) {
		this->decode_referent(id, term_id, ssn);
	} else {
		id = term_id;
	}
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:decode_referent");

	canonical_identity identity;
	tagd::rank rank;
	const auto identity_rc = load_identity(identity, rank, id);

	if (identity_rc == tagd::TAGD_OK) {
		tagd::abstract_tag semantic(id, identity.sub_relator, identity.super_object, identity.pos);
		tagd::abstract_tag canonical = rank.empty()
			? std::move(semantic) : tagd::abstract_tag(semantic, rank);
		const auto relation_flags = _relation_semantics == relation_semantics::profile_overrides
			? flags : flags & ~F_INCLUDE_INHERITED_PREDICATES;
		const auto relations_rc = get_relations(canonical.relations, id, ssn,
			relation_flags | F_NO_TRANSFORM_REFERENTS);
		// An empty relation set is valid; unresolved components in a nonempty set are not.
		if (relations_rc != tagd::TAGD_OK
			&& !(relations_rc == tagd::TS_NOT_FOUND && canonical.relations.empty())) {
			RET_SSN_CODE(relations_rc);
		}
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:get_relations");

		const auto types_rc = get_type_relations(canonical.type_relations, id);
		if (types_rc != tagd::TAGD_OK) {
			RET_SSN_CODE(types_rc);
		}
		tagd::abstract_tag populated;
		const auto display_rc = format_declaration(populated, canonical, ssn, flags);
		if (display_rc != tagd::TAGD_OK) {
			RET_SSN_CODE(display_rc);
		}

		if ((flags & F_INCLUDE_INHERITED_PREDICATES)
			&& _relation_semantics == relation_semantics::assertions) {
			relation_resolver state;
			auto rc = load_relation_declarations(state, {id});
			if (rc == tagd::TAGD_OK) {
				rc = expand_relations(populated, id, state, ssn, flags);
			}
			if (rc != tagd::TAGD_OK) {
				RET_SSN_CODE(rc);
			}
		}
		t = std::move(populated);

	} else if (identity_rc != tagd::TS_NOT_FOUND) {
		RET_SSN_CODE(identity_rc);
	} else {
		if (term_pos & tagd::POS_REFERS) {
			RET_SSN_FERROR(tagd::TS_AMBIGUOUS,
				"%s refers to a tag with no matching context", term_id.c_str());
		} else {
			if (flags & F_NO_NOT_FOUND_ERROR) {
				return tagd::TS_NOT_FOUND;
			} else {
				RET_SSN_ERROR(tagd::TS_NOT_FOUND,
					tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, term_id) );
			}
		}
	}

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::get(tagd::url& get_url, tagd::id_view id, session* ssn, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	if (!(flags & F_NO_RESET)) this->reset(ssn);
	tagd::id_string owned_id(id);

	// id should be a canonical url
	tagd::url u{owned_id};
	if (!u.ok())
		RET_SSN_FERROR(u.code(), "url init failed: %s", owned_id.c_str());

	// we use hduri to identify urls internally
	auto hduri = u.hduri();

	get_url = u;
	assert(get_url.ok());
	assert(hduri == get_url.hduri());

	(void)this->get((tagd::abstract_tag&)get_url, hduri, ssn, flags);
	OK_OR_RET_SSN_ERR(); // this or ssn errors already set

	if (!get_url.ok())
		RET_SSN_FERROR(u.code(), "get url failed: %s", get_url.id().c_str());

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::get_relations(tagd::predicate_set& P, const tagd::id_string& id,
	session* ssn, flags_t flags) {
	// Inherited results are not complete direct assertions and cannot populate this cache.
	auto rc = _canonical.predicates(P, id, !(flags & F_INCLUDE_INHERITED_PREDICATES),
		[&](auto& rows) {
			return load_predicates(rows, id, ssn, flags);
		},
		[this](auto& values) {
			return order_predicates(values);
		});
	if (rc != tagd::TAGD_OK)
		return rc;

	// GET treats this sentinel with an empty set as a successful tag with no predicates.
	if (P.empty())
		return tagd::TS_NOT_FOUND;

	id_transform_func_t f_transform =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) ? f_passthrough : this->f_encode_referent(ssn);
	P.transform(f_transform);
	return tagd::TAGD_OK;  // don't set session code on non-public methods
}

tagd::code sqlite::load_predicates(std::vector<canonical_predicate>& rows,
	const tagd::id_string& id, session* ssn, flags_t flags) {
	_canonical.record_predicate_read();

	const bool inherited = flags & F_INCLUDE_INHERITED_PREDICATES;
	const auto statement = inherited ? stmt_t::GET_INHERITED_RELATIONS : stmt_t::GET_RELATIONS;
	if (inherited) {
		/*
		 * Configuration profiles use their structural tree; only _entity is its own parent.
		 * Nearest subjects come first so an empty modifier also shadows ancestors.
		 */
		this->prepare(statement,
			"WITH RECURSIVE ancestry(tag, super_object, depth) AS ("
			"SELECT tag, super_object, 0 FROM tags WHERE tag = tid(?) "
			"UNION ALL "
			"SELECT t.tag, t.super_object, a.depth + 1 FROM tags t "
			"JOIN ancestry a ON t.tag = a.super_object WHERE a.tag != a.super_object"
			") SELECT idt(r.relator), idt(r.object), idt(r.modifier), r.modifier_type, r.operator "
			"FROM ancestry a JOIN relations r ON r.subject = a.tag ORDER BY a.depth",
			"get_inherited_relations"
		);
	} else {
		this->prepare(statement,
			"SELECT idt(relator), idt(object), idt(modifier), modifier_type, operator "
			"FROM relations WHERE subject = tid(?)",
			"get_relations"
		);
	}
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get_relations");

	this->bind_text(statement, 1, id.c_str(), "get subject");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get:bind_subject");

	const int F_RELATOR = 0;
	const int F_OBJECT = 1;
	const int F_MODIFIER = 2;

	int s_rc;
	// Profile overrides are application configuration, separate from semantic inheritance.
	std::set<std::pair<tagd::id_string, tagd::id_string>> seen;
	sqlite3_stmt *stmt = get_stmt(statement);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const tagd::id_string relator((const char*) sqlite3_column_text(stmt, F_RELATOR));
		const tagd::id_string object((const char*) sqlite3_column_text(stmt, F_OBJECT));
		// Mask by canonical IDs before referent encoding can change their spelling.
		if (inherited && !seen.emplace(relator, object).second)
			continue;

		canonical_predicate p{relator, object, {}};
		p.modifier_type = static_cast<tagd::data_t>(sqlite3_column_int(stmt, 3));
		p.opr8r = static_cast<tagd::operator_t>(sqlite3_column_int(stmt, 4));
		if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			p.modifier = (const char*) sqlite3_column_text(stmt, F_MODIFIER);
		}
		rows.push_back(std::move(p));
	}

	if (s_rc != SQLITE_DONE) {
		this->ferror(tagd::TS_INTERNAL_ERR, "tagdb:get_relations:step failed: %s", sqlite3_errmsg(_db));
		RET_SSN_FERROR(tagd::TS_INTERNAL_ERR, "tagdb:get_relations:step error for id: %s", id.c_str());
	}

	sqlite3_reset(stmt);
	return tagd::TAGD_OK;
}

tagd::part_of_speech sqlite::term_pos(tagd::id_view id, rowid_t *term_id) {
	tagd::id_string term(id);

	tagd::code tc = this->prepare(stmt_t::TERM_POS,
		"SELECT ROWID, term_pos FROM terms WHERE term = ?",
		"term term_pos"
	);
	if (tc != tagd::TAGD_OK)
		this->print_errors();
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	tc = this->bind_text(stmt_t::TERM_POS, 1, term.c_str(), "term");
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	const int F_TERM_ID = 0;
	const int F_TERM_POS = 1;

	sqlite3_stmt *stmt = get_stmt(stmt_t::TERM_POS);
	int s_rc = sqlite3_step(stmt);
	if (s_rc == SQLITE_ROW) {
		if (term_id != nullptr)
			*term_id = sqlite3_column_int64(stmt, F_TERM_ID);
		return (tagd::part_of_speech) sqlite3_column_int(stmt, F_TERM_POS);
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "term_pos failed: %s", term.c_str());
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::part_of_speech sqlite::term_id_pos(rowid_t term_id, std::string *term) {

	tagd::code tc = this->prepare(stmt_t::TERM_ID_POS,
		"SELECT term, term_pos FROM terms WHERE ROWID = ?",
		"term term_pos"
	);
	if (tc != tagd::TAGD_OK)
		this->print_errors();
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	tc = this->bind_rowid(stmt_t::TERM_ID_POS, 1, term_id, "term_id");
	assert(tc == tagd::TAGD_OK);
	if (tc != tagd::TAGD_OK) return tagd::POS_UNKNOWN;

	const int F_TERM = 0;
	const int F_TERM_POS = 1;

	sqlite3_stmt *stmt = get_stmt(stmt_t::TERM_ID_POS);
	int s_rc = sqlite3_step(stmt);
	if (s_rc == SQLITE_ROW) {
		if (term != nullptr)
			*term = (const char*)sqlite3_column_text(stmt, F_TERM);
		return (tagd::part_of_speech) sqlite3_column_int(stmt, F_TERM_POS);
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "term_id_pos failed: %ld", term_id);
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::part_of_speech sqlite::pos(tagd::id_view id, session *ssn, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK) {
		if (ssn)
			ssn->code(snapshot.status);
		return tagd::POS_UNKNOWN;
	}
	if (!(flags & F_NO_RESET)) this->reset(ssn);
	tagd::id_string term(id);

	if (id[0] == '_')
		return hard_tag::pos(id);

	tagd::id_string refers_to;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
		refers_to = term;
	else
		(void)this->refers_to(refers_to, term, ssn);  // refers_to set if id refers to it (in context)

	if (refers_to[0] == '_')
		return hard_tag::pos(refers_to);

	this->prepare(stmt_t::POS,
		"SELECT pos FROM tags WHERE tag = tid(?)",
		"tag pos"
	);
	assert(_code == tagd::TAGD_OK);
	OK_OR_RET_UNKNOWN_SSN_INT_ERR_ACTION("tagdb:pos");

	this->bind_text(stmt_t::POS, 1, (refers_to.empty() ? term.c_str() : refers_to.c_str()), "pos id");
	assert(_code == tagd::TAGD_OK);
	OK_OR_RET_UNKNOWN_SSN_INT_ERR_ACTION("tagdb:pos:bind");

	const int F_POS = 0;

	sqlite3_stmt *stmt = get_stmt(stmt_t::POS);
	int s_rc = sqlite3_step(stmt);
	if (s_rc == SQLITE_ROW) {
		return (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
	} else if (s_rc == SQLITE_ERROR) {
		auto e = tagd::error::ferror(tagd::TS_INTERNAL_ERR,
			"tagdb:pos:step failed: %s", (refers_to.empty() ? term.c_str() : refers_to.c_str()));
		if (ssn)
			ssn->error(e);
		// TODO(cpp23-return-contracts): use a helper for adding optional error details.
		// The original error must still be reported if adding a relation fails.
		(void)e.relation(HARD_TAG_HAS, HARD_TAG_MESSAGE, sqlite3_errmsg(_db));
		this->error(e);
		return tagd::POS_UNKNOWN;
	} else {
		return tagd::POS_UNKNOWN;
	}
}

tagd::code sqlite::refers(tagd::id_string &refers, const tagd::id_string& refers_to, session* ssn) {
	assert(!refers_to.empty());
	if(!ssn || ssn->context().empty()) return tagd::TS_NOT_FOUND;

	const bool memoize = _read_depth && !_mutation_depth;
	if (memoize) {
		// Nested calls can use another session; equal context contents have equal substitution rules.
		if (_operation_refers_context != ssn->context()) {
			_operation_refers.clear();
			_operation_refers_context = ssn->context();
		}
		if (auto found = _operation_refers.find(refers_to); found != _operation_refers.end()) {
			_canonical.record_referent_hit();
			if (!found->second)
				return tagd::TS_NOT_FOUND;

			refers = *found->second;
			return tagd::TAGD_OK;
		}
	}

	for (auto it = ssn->context().rbegin(); it != ssn->context().rend(); ++it) {
		this->prepare(stmt_t::REFERS,
			"SELECT idt(refers) "
			"FROM referents, tags "
			"WHERE refers_to = tid(?) "
			"AND context = tag "
			"AND rank GLOB rank_glob(("
			 "SELECT rank FROM tags WHERE tag = tid(?)"
			")) " // all context <= {_context}
			"ORDER BY rank DESC "  // closest (subordinate) rank
			"LIMIT 1",
			"refers"
		);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers");

		this->bind_text(stmt_t::REFERS, 1, refers_to.c_str(), "refers_to");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers:bind_refers_to");

		this->bind_text(stmt_t::REFERS, 2, it->c_str(), "tag in context");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb::refers:bind_context");

		const int F_REFERS = 0;

		sqlite3_stmt *stmt = get_stmt(stmt_t::REFERS);
		_canonical.record_referent_read();
		int s_rc = sqlite3_step(stmt);
		if (s_rc == SQLITE_ROW) {
			refers = (const char *) sqlite3_column_text(stmt, F_REFERS);
			sqlite3_reset(stmt);
			if (memoize)
				_operation_refers.emplace(refers_to, refers);
			return tagd::TAGD_OK;
		} else if (s_rc != SQLITE_DONE) {
			return this->ferror(tagd::TS_INTERNAL_ERR, "refers failed: %s", sqlite3_errmsg(_db));
		}
		sqlite3_reset(stmt);
	}

	if (memoize)
		_operation_refers.emplace(refers_to, std::nullopt);
	return tagd::TS_NOT_FOUND;
}

tagd::code sqlite::refers_to(tagd::id_string &refers_to, const tagd::id_string& refers, session* ssn) {
	assert(!refers.empty());
	if(!ssn) return tagd::TS_NOT_FOUND;

	for (auto it = ssn->context().rbegin(); it != ssn->context().rend(); ++it) {
		this->prepare(stmt_t::REFERS_TO,
			"SELECT idt(refers_to) "
			"FROM referents, tags "
			"WHERE refers = tid(?) "
			"AND context = tag "
			"AND rank GLOB rank_glob(("
			 "SELECT rank FROM tags WHERE tag = tid(?)"
			")) " // all context <= {_context}
			"ORDER BY rank DESC "
			"LIMIT 1",
			"refers_to"
		);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to");

		this->bind_text(stmt_t::REFERS_TO, 1, refers.c_str(), "tagdb:refers_to:bind_refers");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:bind_refers");

		this->bind_text(stmt_t::REFERS_TO, 2, it->c_str(), "tagdb:refers_to:bind_context");
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:bind_context");

		const int F_REFERS_TO = 0;

		sqlite3_stmt *stmt = get_stmt(stmt_t::REFERS_TO);
		int s_rc = sqlite3_step(stmt);
		if (s_rc == SQLITE_ROW) {
			refers_to = (const char *) sqlite3_column_text(stmt, F_REFERS_TO);
			return tagd::TAGD_OK;
		} else if (s_rc == SQLITE_ERROR) {
			this->ferror(tagd::TS_INTERNAL_ERR, "tagdb:refers_to failed: %s", sqlite3_errmsg(_db));
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:refers_to:step");
		}
	}

	return tagd::TS_NOT_FOUND;
}

/*
 * returns whether tag exists or not
 * on error, error set and returns false
 */
bool sqlite::exists(tagd::id_view id, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return false;
	if (!(flags & F_NO_RESET)) this->reset(nullptr);
	tagd::id_string term(id);

	this->prepare(stmt_t::EXISTS,
		"SELECT 1 FROM tags WHERE tag = tid(?)",
		"tag exists"
	);
	OK_OR_RET_FALSE();

	this->bind_text(stmt_t::EXISTS, 1, term.c_str(), "exists statement id");
	OK_OR_RET_FALSE();

	int s_rc = sqlite3_step(get_stmt(stmt_t::EXISTS));
	if (s_rc == SQLITE_ROW) {
		return true;
	} else if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "exists failed: %s", term.c_str());
		return false;
	}

	return false; // not found
}

// tagd::TS_NOT_FOUND returned if destination undefined
tagd::code sqlite::validate_put_identity(const tagd::abstract_tag& t, session* ssn) {
	const auto id_rule = tagd::valid_tag_id(t.id());
	if (id_rule == tagd::tag_id_rule::too_long)
		RET_SSN_FERROR(tagd::TS_ERR_MAX_TAG_LEN, "tag exceeds MAX_TAG_LEN of %d", tagd::MAX_TAG_LEN);

	if (id_rule == tagd::tag_id_rule::empty)
		RET_SSN_ERROR(tagd::TS_MISUSE, "inserting empty tag not allowed");
	if (id_rule == tagd::tag_id_rule::reserved_prefix)
		RET_SSN_ERROR(tagd::TS_MISUSE, "tag ID starts with reserved ':': " + t.id());

	if (t.id()[0] == '_' && !_doing_init)
		RET_SSN_FERROR(tagd::TS_MISUSE, "inserting hard tags not allowed: %s", t.id().c_str());

	if (t.id() == t.super_object() && t.id() != HARD_TAG_ENTITY)
		RET_SSN_FERROR(tagd::TS_MISUSE, "_id == _super_object not allowed: %s", t.id().c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::put_impl(const tagd::abstract_tag& t, session *ssn, flags_t flags) {
	// Internal write reads must describe stored assertions, not inherited values.
	flags &= ~F_INCLUDE_INHERITED_PREDICATES;
	tagdb_debug_scope debug_scope(this, ssn);
	tagdb_notice_scope notice_scope(this, ssn, tagd::CMD_PUT, t);
	tagdb_error_scope error_scope(this, ssn, tagd::CMD_PUT, t);

	TAGDB_LOG_DEBUG( "sqlite::put: " << t << " -- " << flag_util::flag_list_str(flags) << std::endl );
	TAGDB_LOG_DEBUG("tagdb put subject=" << t.id());

	const auto identity = validate_put_identity(t, ssn);
	if (identity != tagd::TAGD_OK)
		return identity;

	const auto type_validation = validate_declaration(t);
	if (type_validation != tagd::TAGD_OK) {
		RET_SSN_CODE(type_validation);
	}

	tagd::abstract_tag existing;
	tagd::code existing_rc = this->get(existing, t.id(), ssn, (flags|F_NO_TRANSFORM_REFERENTS|F_NO_NOT_FOUND_ERROR));
	if (existing_rc != tagd::TAGD_OK && existing_rc != tagd::TS_NOT_FOUND)
		return existing_rc; // err set by get

	return put_validated(t, std::move(existing), ssn, flags);
}

tagd::code sqlite::put_validated(const tagd::abstract_tag& t, tagd::abstract_tag existing,
	session* ssn, flags_t flags) {
	const auto existing_rc = existing.empty() ? tagd::TS_NOT_FOUND : tagd::TAGD_OK;

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
			if (t.relations.empty() && t.type_relations.empty()) {  // duplicate tag and no relations to insert
				RET_SSN_FERROR(tagd::TS_MISUSE, "cannot put a tag without relations: %s", t.id().c_str());
			} else { // insert relations
				RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags)));
			}
		}
	}

	if (existing_rc == tagd::TAGD_OK && t.sub_relator() == existing.sub_relator() &&
		t.super_object() == existing.super_object() )
	{  // same location
		if (t.relations.empty() && t.type_relations.empty()) {  // duplicate tag and no relations to insert
			if (flags & F_IGNORE_DUPLICATES)
				RET_SSN_CODE(tagd::TAGD_OK);
			else
				RET_SSN_FERROR(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
		} else { // insert relations
			RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags)));
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

	tagd::code ins_upd_rc;
	if ( existing_rc == tagd::TAGD_OK ) {  // existing tag
		// use existing because it has the rank
		if (existing.sub_relator() != t.sub_relator()) {
			auto relations = std::move(existing.relations);
			auto types = std::move(existing.type_relations);
			tagd::abstract_tag semantic(
				existing.id(),
				t.sub_relator(),
				existing.super_object(),
				existing.pos()
			);
			existing = existing.rank().empty()
				? std::move(semantic)
				: tagd::abstract_tag(semantic, existing.rank());
			existing.relations = std::move(relations);
			existing.type_relations = std::move(types);
		}
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

	if (ins_upd_rc == tagd::TAGD_OK && (!t.relations.empty() || !t.type_relations.empty()))
		// Creating or moving the subject already changed this PUT's stored state.
		RET_SSN_CODE(f_fts_passthru(this->insert_relations(t, flags | F_IGNORE_DUPLICATES)));

	// res from insert/update, (errors will have been set)
	RET_SSN_CODE(f_fts_passthru(ins_upd_rc));
}

tagd::code sqlite::put(const tagd::url& u, session *ssn, flags_t flags) {
	return put_transaction(u, ssn, flags, nullptr);
}

tagd::code sqlite::put(const tagd::referent& r, session *ssn, flags_t flags) {
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	mutation_scope mutation(*this);
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	RET_SSN_CODE(this->insert_referent(r, ssn, flags));
}


void sqlite::tag_affected(std::set<tagd::id_string>& terms_affected, const tagd::abstract_tag& t) {
	auto f_term_affected = [&terms_affected](const tagd::id_string& term) mutable {
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
	for (const auto& p : t.type_relations) {
		f_term_affected(p.relator);
		f_term_affected(p.object);
	}
}

tagd::code sqlite::del(const tagd::abstract_tag& t, session *ssn, flags_t flags) {
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	mutation_scope mutation(*this);
	// Inherited predicates are read results, never local deletion targets.
	flags &= ~F_INCLUDE_INHERITED_PREDICATES;
	if (!(flags & F_NO_RESET)) this->reset(ssn);
	tagdb_debug_scope debug_scope(this, ssn);
	tagdb_notice_scope notice_scope(this, ssn, tagd::CMD_DELETE, t);
	tagdb_error_scope error_scope(this, ssn, tagd::CMD_DELETE, t);

	TAGDB_LOG_DEBUG( "sqlite::del: " << t << std::endl );
	TAGDB_LOG_DEBUG("tagdb del subject=" << t.id());

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

	tagd::abstract_tag del_tag =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
			? tagd::abstract_tag(t)
			: this->decode_referents(t, ssn);

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

	const auto inherited_delete = check_inherited_deletion(del_tag, existing);
	if (inherited_delete != tagd::TAGD_OK) {
		RET_SSN_CODE(inherited_delete);
	}

	// make a set of all terms affected, so we can update the term pos after deleting tag
	std::set<tagd::id_string> terms_affected;

	this->exec("BEGIN");

	// empty del_tag relations means delete entire tag for given id
	if (del_tag.relations.empty() && del_tag.type_relations.empty()) {
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

		this->exec_mprintf("DELETE FROM type_relations WHERE subject=tid(%Q)", del_tag.id().c_str());
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:type_relations");

		this->delete_relations(del_tag.id());  // all relations given subject
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_relations");

		this->delete_fts_tag(del_tag.id());
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_fts_tag");

		const auto deleted = this->delete_tag(del_tag.id(), ssn);
		if (deleted != tagd::TAGD_OK) {
			// Preserve the failure and undo preceding deletes even without a session.
			this->finalize();
			this->exec("ROLLBACK");
			RET_SSN_CODE(deleted);
		}
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:delete_tag");

		// referents affected
		tagd::tag_set R;
		tagd::interrogator q_refers_to{HARD_TAG_INTERROGATOR, HARD_TAG_REFERENT};
		(void)q_refers_to.relation(HARD_TAG_REFERS_TO, del_tag.id());

		auto tc = this->query(R, q_refers_to, nullptr);
		OK_OR_ROLLBACK_RET_SSN_INT_ERR_ACTION("tagdb:del:q_refers_to");

		if (tc == tagd::TAGD_OK) {
			for ( auto r : R )
				tag_affected(terms_affected, r);
		}

		tag_affected(terms_affected, existing);
	} else {
		const auto deleted_types = delete_type_relations(del_tag);
		if (deleted_types != tagd::TAGD_OK) {
			this->exec("ROLLBACK");
			RET_SSN_CODE(deleted_types);
		}
		// delete only del_tag.relations
		int errcnt = 0;
		for( auto p : del_tag.relations ) {
			const bool declared = std::any_of(existing.relations.begin(), existing.relations.end(),
				[&](const auto& stored) {
					return p.relator == stored.relator && p.object == stored.object
						&& (p.modifier.empty() || (p.modifier == stored.modifier
							&& p.modifier_type == stored.modifier_type && p.opr8r == stored.opr8r));
				});
			if (!declared) {
				errcnt++;
				// Record errors without a session too, so the transaction always rolls back.
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
				// TODO(cpp23-return-contracts): check whether missing-tag details are required.
				// Currently, failure to add these details leaves the original error.
				if (!this->exists(p.relator, flags|F_NO_RESET))
					(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, p.relator);
				if (!this->exists(p.object, flags|F_NO_RESET))
					(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_UNKNOWN_TAG, p.object);

				this->error(e);
			}
		}
		if (errcnt) {
			OK_OR_ROLLBACK_RET_SSN_ERR();
		}

		if (!del_tag.relations.empty())
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
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	mutation_scope mutation(*this);
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	if (!u.ok())
		RET_SSN_FERROR(u.code(), "del url not ok(%s): %s",  tagd::code_str(u.code()), u.id().c_str());

	/*
	 * url _id is the actual url, but we use the hduri
	 * internally, so we have to convert it
	 */
	tagd::abstract_tag t_sem(u.hduri(), u.pos());
	tagd::abstract_tag t = u.rank().empty() ? std::move(t_sem) : tagd::abstract_tag(t_sem, u.rank());
	t.relations = u.relations;
	t.type_relations = u.type_relations;
	// don't insert url part relations
	RET_SSN_CODE(this->del(t, ssn, (flags|F_NO_POS_CAST)));
}

tagd::code sqlite::del(const tagd::referent& r, session *ssn, flags_t flags) {
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	mutation_scope mutation(*this);
	if (!(flags & F_NO_RESET)) this->reset(ssn);

	sqlite3_stmt *stmt = nullptr;

	if (r.refers().empty()) {
		if (r.refers_to().empty()) {
			if (r.context().empty()) {
				// don't allow deleting all referents
				RET_SSN_ERROR(tagd::TS_MISUSE, "deleting all referents not allowed");
			} else {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE context = tid(?)",
					/* //TODO implement RECURSIVE flag to delete all contained contexts
					"WHERE context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					  "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					*/
					"delete referent context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (r.context().empty()) {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers_to = tid(?)",
					"delete referent refers_to"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers_to = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					*/
					"delete referent refers_to, context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		}
	} else {  // !refers.empty()
		if (r.refers_to().empty()) {
			if (r.context().empty()) {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?)",
					"delete referent refers"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					*/
					"delete referent refers context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, r.context().c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (r.context().empty()) {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?)",
					"delete referent refers, refers_to"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"DELETE FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?) "
					"AND context = tid(?)",
					/*
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					*/
					"delete referent refers, refers_to, context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, r.refers().c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, r.refers_to().c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 3, r.context().c_str(), "context");
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
	std::set<tagd::id_string> terms_affected;
	tag_affected(terms_affected, r);
	for ( auto id : terms_affected ) {
		this->update_pos_occurence(id);
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:del:referent:terms_affected");
	}

	return tagd::TAGD_OK;
}

tagd::part_of_speech sqlite::term_pos_occurence(const tagd::id_string& id, session *ssn, bool set_fk_err) {
	if (get_stmt(stmt_t::TERM_POS_OCCURENCE) == nullptr) {
		/*
		 * Combine the tag's declared part of speech with the positions where
		 * it is used in tags, relations, and referents.
		 *
		 * The first SELECT reads tags.pos, including POS_TRUE and POS_FALSE.
		 * They need no separate UNION branches: those branches describe uses
		 * of a tag, such as being another tag's super_object, not its type.
		 *
		 * TODO: Review whether this occurrence query can be simplified or
		 * avoided. Also consider replacing SQL string construction without
		 * duplicating the numeric values defined by tagd::part_of_speech.
		 */
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
		this->prepare(stmt_t::TERM_POS_OCCURENCE, ss.str().c_str(), "pos occurence statement");
	} else {
		sqlite3_stmt *tpo_stmt = get_stmt(stmt_t::TERM_POS_OCCURENCE);
		sqlite3_reset(tpo_stmt);
		sqlite3_clear_bindings(tpo_stmt);
	}
	OK_OR_RET_POS_UNKNOWN();

	int i = 1;
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, i, id.c_str(), "tag pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "sub_relator pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "super_object pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "subject pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "relator pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "object pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "modifier pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "refers pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "refers_to pos occurence");
	OK_OR_RET_POS_UNKNOWN();
	this->bind_text(stmt_t::TERM_POS_OCCURENCE, ++i, id.c_str(), "context pos occurence");
	OK_OR_RET_POS_UNKNOWN();

	const int F_POS = 0;

	int s_rc;
	tagd::part_of_speech occurence_pos = tagd::POS_UNKNOWN;
	sqlite3_stmt *stmt = get_stmt(stmt_t::TERM_POS_OCCURENCE);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		// Bitwise OR promotes enum values to int; cast back to part_of_speech.
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
		TAGDB_LOG_DEBUG( id << ": " << pos_list_str(occurence_pos) << " |= " << pos_str(pos) << std::endl );
		occurence_pos = ((tagd::part_of_speech)(occurence_pos | pos));

		/*
		 * Report uses that prevent deletion through a foreign-key constraint.
		 * POS_TRUE and POS_FALSE describe the tag itself, not a reference to
		 * it, so they need no cases in this switch.
		 */
		if (set_fk_err) {
			// TODO causes will need to be defined as hard tags
			// if we ever want to insert errors in a tagdb
			tagd::id_string cause;
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
					ssn->error(tagd::TS_DEPENDENCY,
						tagd::predicate(HARD_TAG_CAUSED_BY, cause, id) );
				}
			}
		}
	}
	TAGDB_LOG_DEBUG( "occurence_pos(" << id << "): " << pos_list_str(occurence_pos) << std::endl );

	if (s_rc == SQLITE_ERROR)
		SQLITE_FERROR(s_rc, "term pos occurence failed: %s", id.c_str());

	return occurence_pos;
}

tagd::code sqlite::update_pos_occurence(const tagd::id_string& id) {
	tagd::part_of_speech occurence_pos = this->term_pos_occurence(id, nullptr, false);
	OK_OR_RET_ERR();

	return ( occurence_pos == tagd::POS_UNKNOWN
		? this->delete_term(id) : this->update_term(id, occurence_pos) );
}

tagd::code sqlite::delete_refers_to(const tagd::id_string& id) {
	assert( !id.empty() );

	this->prepare(stmt_t::DELETE_REFERS_TO,
		"DELETE FROM referents WHERE refers_to = tid(?)",
		"delete refers_to"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::DELETE_REFERS_TO, 1, id.c_str(), "delete refers_to");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::DELETE_REFERS_TO));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete refers_to failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_relations(const tagd::id_string& subject) {
	assert( !subject.empty() );

	this->prepare(stmt_t::DELETE_SUBJECT_RELATIONS,
		"DELETE FROM relations WHERE subject = tid(?)",
		"delete relations"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::DELETE_SUBJECT_RELATIONS, 1, subject.c_str(), "delete subject relations");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::DELETE_SUBJECT_RELATIONS));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete subject relations failed: %s", subject.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_relations(const tagd::id_string& subject, const tagd::predicate_set& P) {
	assert( !subject.empty() );
	assert( !P.empty() );

	for (auto p : P) {
		// A qualified DELETE retracts only the matching value, not the predicate.
		const auto statement = p.modifier.empty() ? stmt_t::DELETE_RELATION : stmt_t::CLEAR_MODIFIER;
		const auto sql = p.modifier.empty()
			? std::string("DELETE FROM relations WHERE subject = tid(?) AND relator = tid(?) AND object = tid(?)")
			: "UPDATE relations SET modifier = NULL, modifier_type = "
			  + std::to_string(tagd::TYPE_STRING) + ", operator = " + std::to_string(tagd::OP_EQ)
			  + " WHERE subject = tid(?) AND relator = tid(?) AND object = tid(?)";
		this->prepare(statement,
			sql.c_str(),
			"delete relations"
		);
		OK_OR_RET_ERR();

		this->bind_text(statement, 1, subject.c_str(), "delete relation subject");
		OK_OR_RET_ERR();

		this->bind_text(statement, 2, p.relator.c_str(), "delete relation relator");
		OK_OR_RET_ERR();

		this->bind_text(statement, 3, p.object.c_str(), "delete relation object");
		OK_OR_RET_ERR();

		int s_rc = sqlite3_step(get_stmt(statement));
		if (s_rc != SQLITE_DONE) {
			RET_SQLITE_FERROR(s_rc, "delete relation failed: %s %s %s",
					subject.c_str(), p.relator.c_str(), p.object.c_str());
		}
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_tag(const tagd::id_string& id, session *ssn) {
	assert( !id.empty() );

	this->prepare(stmt_t::DELETE_TAG,
		"DELETE FROM tags WHERE tag = tid(?)",
		"delete tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::DELETE_TAG, 1, id.c_str(), "delete tag id");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::DELETE_TAG));
	if (s_rc == SQLITE_DONE) {
		return tagd::TAGD_OK;
	} else if (s_rc == SQLITE_CONSTRAINT) {
		TAGDB_LOG_DEBUG( "SQLITE_CONSTRAINT: " << sqlite3_errmsg(_db) << std::endl );
		// Sessions are optional; dependency failures must still permit rollback.
		if (!ssn)
			return this->ferror(tagd::TS_DEPENDENCY,
				"cannot delete tag with dependencies: %s", id.c_str());
		// Preserve ordinary dependency details and add the independent type references.
		this->term_pos_occurence(id, ssn, true);
		const auto dependencies = this->type_dependency_errors(id, ssn);
		if (dependencies != tagd::TAGD_OK) {
			RET_SSN_CODE(dependencies);
		}

		assert(ssn->code() == tagd::TS_DEPENDENCY);
		if (ssn->code() != tagd::TS_DEPENDENCY)  // unknown cause of FK constraint failure, should't happen
			RET_SYS_SSN_FERROR(tagd::TS_INTERNAL_ERR, "delete tag unknown dependency failure: %s", id.c_str());
		else  // error(s) set
			return ssn->code();
	} else {
		RET_SYS_SSN_FERROR(tagd::TS_INTERNAL_ERR, "delete tag failed: %s", id.c_str());
	}
}

tagd::code sqlite::next_rank(tagd::rank& next, const tagd::abstract_tag& sub) {
	bool is_entity_or_ranked =
		sub.id() == HARD_TAG_ENTITY || sub.has_rank();

	if(!is_entity_or_ranked)
		LOG_ERROR( "next_rank: " << sub << "\t-- " << sub.rank().dotted_str() << std::endl )
	assert(is_entity_or_ranked);

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

tagd::part_of_speech sqlite::put_term(const tagd::id_string& t, const tagd::part_of_speech pos) {
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

	auto f_print_fts = [&](const tagd::id_string& s) {
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

	tagd::id_string last_relator;
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

tagd::code sqlite::insert_fts_tag(const tagd::id_string& id, flags_t flags) {
	assert( !id.empty() );

	tagd::abstract_tag t;
	(void)this->get(t, id, nullptr, flags);
	OK_OR_RET_ERR();

	TAGDB_LOG_DEBUG( "insert_fts_tag( " << id << " ): " << format_fts(t) << std::endl );

	this->prepare(stmt_t::INSERT_FTS_TAG,
		"INSERT INTO fts_tags (docid, content) VALUES (tid(?), ?)",
		"insert fts_tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::INSERT_FTS_TAG, 1, id.c_str(), "insert fts_tag docid");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::INSERT_FTS_TAG, 2, format_fts(t).c_str(), "insert fts_tag content");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::INSERT_FTS_TAG));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "insert fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::update_fts_tag(const tagd::id_string& id, flags_t flags) {
	assert( !id.empty() );

	tagd::abstract_tag t;
	(void)this->get(t, id, nullptr, flags);
	OK_OR_RET_ERR();

	TAGDB_LOG_DEBUG( "update_fts_tag( " << id << " ): " << format_fts(t) << std::endl );

	this->prepare(stmt_t::UPDATE_FTS_TAG,
		"UPDATE fts_tags SET content = ? WHERE docid = tid(?)",
		"update fts_tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_FTS_TAG, 1, format_fts(t).c_str(), "update fts_tag content");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_FTS_TAG, 2, id.c_str(), "update fts_tag docid");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::UPDATE_FTS_TAG));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "update fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_fts_tag(const tagd::id_string& id) {
	assert( !id.empty() ); this->prepare(stmt_t::DELETE_FTS_TAG,
		"DELETE FROM fts_tags WHERE docid = tid(?)",
		"delete fts_tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::DELETE_FTS_TAG, 1, id.c_str(), "delete fts_tag docid");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::DELETE_FTS_TAG));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete fts_tag failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::insert_term(const tagd::id_string& t, const tagd::part_of_speech pos) {
	assert( !t.empty() );

	this->prepare(stmt_t::INSERT_TERM,
		"INSERT INTO terms (term, term_pos) VALUES (?, ?)",
		"insert term"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::INSERT_TERM, 1, t.c_str(), "insert term");
	OK_OR_RET_ERR();

	this->bind_int(stmt_t::INSERT_TERM, 2, pos, "insert term_pos");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::INSERT_TERM));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "insert term failed: %s", t.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::update_term(const tagd::id_string& t, const tagd::part_of_speech pos) {
	assert( !t.empty() );

	this->prepare(stmt_t::UPDATE_TERM,
		"UPDATE terms SET term_pos = ? WHERE term = ? AND term_pos <> ?",
		"update term"
	);
	OK_OR_RET_ERR();

	this->bind_int(stmt_t::UPDATE_TERM, 1, pos, "update term_pos");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_TERM, 2, t.c_str(), "update term");
	OK_OR_RET_ERR();

	this->bind_int(stmt_t::UPDATE_TERM, 3, pos, "update <> term_pos");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::UPDATE_TERM));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "update term failed: %s", t.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::delete_term(const tagd::id_string& id) {
	assert( !id.empty() );

	this->prepare(stmt_t::DELETE_TERM,
		"DELETE FROM terms WHERE term = ?",
		"delete term"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::DELETE_TERM, 1, id.c_str(), "delete term");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::DELETE_TERM));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "delete term failed: %s", id.c_str());

	return tagd::TAGD_OK;
}

tagd::code sqlite::insert(const tagd::abstract_tag& t, const tagd::abstract_tag& destination) {
	TAGDB_LOG_DEBUG( "sqlite::insert " << t << std::endl );

	assert( t.super_object() == destination.id() );
	assert( !t.id().empty() );
	assert( !t.sub_relator().empty() );
	assert( !t.super_object().empty() );

	tagd::rank rank;
	next_rank(rank, destination);
	OK_OR_RET_ERR();

	// TODO: rank race condition (atomic next_rank inside transaction)
	// When inserting a new tag we already know its super_object.
	// Bind a deterministic C function:
	//
	//   next_rank(parent_tid INTEGER) → TEXT
	//
	/*
	 * that computes the next child rank for that parent *inside* the
	 * INSERT transaction. Register with SQLITE_DETERMINISTIC flag
	 * (same as tid()/idt()). The single ? parameter is bound to the
	 * super_object's tid:
	 */
	//
	// INSERT INTO tags (tag, sub_relator, super_object, rank, pos)
	// VALUES (tid(?), tid(?), tid(?), next_rank(tid(?)), ?);
	//
	/*
	 * NOTICE: Even with the bound function, a concurrent INSERT for the
	 * same super_object can still hit the UNIQUE constraint on rank.
	 * Recommendation: Let SQLite raise the constraint error (no retry
	 * in C++). The error is clear and the transaction stays atomic.
	 */

	this->prepare(stmt_t::INSERT,
		"INSERT INTO tags (tag, sub_relator, super_object, rank, pos) "
		"VALUES (tid(?), tid(?), tid(?), ?, ?)",
		"insert tag"
	);
	OK_OR_RET_ERR();

	tagd::part_of_speech pos;
	if (t.pos() == tagd::POS_UNKNOWN || destination.pos() != tagd::POS_TAG) {
		// Hard-tag families determine grammatical roles even for generic tag inputs.
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
	this->bind_text(stmt_t::INSERT, ++i, t.id().c_str(), "insert id");
	OK_OR_RET_ERR();

	this->put_term(t.sub_relator(), tagd::POS_SUB_RELATOR);
	this->bind_text(stmt_t::INSERT, ++i, t.sub_relator().c_str(), "insert sub_relator");
	OK_OR_RET_ERR();

	this->put_term(t.super_object(), tagd::POS_SUB_OBJECT);
	this->bind_text(stmt_t::INSERT, ++i, t.super_object().c_str(), "insert super_object");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::INSERT, ++i, rank.c_str(), "insert rank");
	OK_OR_RET_ERR();

	this->bind_int(stmt_t::INSERT, ++i, pos, "insert pos");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::INSERT));
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

	// An empty source prefix matches every rank; only a stored non-root tag may move.
	if (t.rank().empty())
		return error(tagd::TS_INTERNAL_ERR, "cannot update a tag with an empty rank");

	// Changing only the subordinate relator does not move the tag or its descendants.
	if (t.super_object() != destination.id()) {
		tagd::rank rank;
		next_rank(rank, destination);
		OK_OR_RET_ERR();

		// Rank lengths are bytes; preserve complete UTF-8 suffixes when replacing a prefix.
		this->prepare(stmt_t::UPDATE_RANKS,
			"UPDATE tags "
			"SET rank = (? || substr(CAST(rank AS BLOB), ?)) "
			"WHERE rank GLOB rank_glob(?)",
			"update ranks"
		);
		OK_OR_RET_ERR();

		this->bind_text(stmt_t::UPDATE_RANKS, 1, rank.c_str(), "new rank");
		OK_OR_RET_ERR();

		this->bind_int(stmt_t::UPDATE_RANKS, 2, (t.rank().size()+1), "rank size");
		OK_OR_RET_ERR();

		this->bind_text(stmt_t::UPDATE_RANKS, 3, t.rank().c_str(), "sub rank");
		OK_OR_RET_ERR();

		int s_rc = sqlite3_step(get_stmt(stmt_t::UPDATE_RANKS));
		if (s_rc != SQLITE_DONE)
			RET_SQLITE_FERROR(s_rc, "update rank failed: %s", t.id().c_str());
	}

	//update tag
	this->prepare(stmt_t::UPDATE_TAG,
			"UPDATE tags SET sub_relator = tid(?), super_object = tid(?) WHERE tag = tid(?)",
			"update tag"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_TAG, 1, t.sub_relator().c_str(), "update sub_relator");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_TAG, 2, destination.id().c_str(), "update super_object");
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::UPDATE_TAG, 3, t.id().c_str(), "update tag id");
	OK_OR_RET_ERR();

	int s_rc = sqlite3_step(get_stmt(stmt_t::UPDATE_TAG));
	if (s_rc != SQLITE_DONE)
		RET_SQLITE_FERROR(s_rc, "update tag failed: %s", t.id().c_str());

	if (t.pos() != tagd::POS_URL) {
		// Keep scanner occurrence metadata aligned with the moved family's grammatical role.
		this->exec_mprintf("UPDATE terms SET term_pos=(term_pos & "
			"~(SELECT pos FROM tags WHERE tag=terms.ROWID)) | %d WHERE ROWID IN "
			"(SELECT tag FROM tags WHERE pos<>%d AND rank GLOB rank_glob("
			"(SELECT rank FROM tags WHERE tag=tid(%Q))))",
			destination.pos(), tagd::POS_URL, t.id().c_str());
		OK_OR_RET_ERR();
		this->exec_mprintf("UPDATE tags SET pos=%d WHERE rank GLOB rank_glob("
			"(SELECT rank FROM tags WHERE tag=tid(%Q))) AND pos<>%d",
			destination.pos(), t.id().c_str(), tagd::POS_URL);
		OK_OR_RET_ERR();
	}

	return validate_reparented_roles(t.id());
}

tagd::code sqlite::insert_relations(const tagd::abstract_tag& t, flags_t flags) {
	assert( !t.id().empty() );
	size_t num_changed = 0;
	auto type_rc = insert_type_relations(t, num_changed);
	if (type_rc != tagd::TAGD_OK) {
		return type_rc;
	}
	if (t.relations.empty()) {
		return num_changed || only_type_axioms(t) || (flags & F_IGNORE_DUPLICATES) ? tagd::TAGD_OK
			: ferror(tagd::TS_DUPLICATE, "duplicate type assertion on: %s", t.id().c_str());
	}

	this->prepare(stmt_t::INSERT_RELATIONS,
		"INSERT INTO relations (subject, relator, object, modifier, modifier_type, operator) "
		"VALUES (tid(?), tid(?), tid(?), tid(?), ?, ?) "
		"ON CONFLICT(subject, relator, object) DO UPDATE SET modifier = excluded.modifier, "
		"modifier_type = excluded.modifier_type, operator = excluded.operator "
		"WHERE excluded.modifier IS NOT NULL AND (relations.modifier IS NOT excluded.modifier "
		"OR relations.modifier_type != excluded.modifier_type OR relations.operator != excluded.operator)",
		"insert relations"
	);
	OK_OR_RET_ERR();

	tagd::predicate_set::iterator it = t.relations.begin();
	if (it == t.relations.end())
		return this->error(tagd::TS_INTERNAL_ERR, "insert empty relations");

	/*
	 * A bare PUT asserts the relation without retracting an existing modifier.
	 * NULL-safe comparison leaves identical values untouched, so SQLite's direct
	 * change count distinguishes duplicate assertions from inserts and updates.
	 */
	do {
		this->put_term(t.id(), tagd::POS_SUBJECT);
		this->bind_text(stmt_t::INSERT_RELATIONS, 1, t.id().c_str(), "insert subject");
		OK_OR_RET_ERR();

		this->put_term(it->relator, tagd::POS_RELATED);
		this->bind_text(stmt_t::INSERT_RELATIONS, 2, it->relator.c_str(), "insert relator");
		OK_OR_RET_ERR();

		this->put_term(it->object, tagd::POS_OBJECT);
		this->bind_text(stmt_t::INSERT_RELATIONS, 3, it->object.c_str(), "insert object");
		OK_OR_RET_ERR();

		/*
		 * TODO(TAGL-types): Persist TAGL type and value separately from PUT intent.
		 * Preserve on bare PUT, clear on explicit NULL, and retain empty strings.
		 * Replace the pre-beta representation directly; no compatibility migration.
		 * Both absent and empty currently arrive here as an empty string.
		 * See workspace TASKS.d/2026-10-22-TAGL-types-and-modifiers.md.
		 */
		if (it->modifier.empty()) {
			this->bind_null(stmt_t::INSERT_RELATIONS, 4, "insert NULL modifier");
		} else {
			this->put_term(it->modifier, tagd::POS_MODIFIER);
			this->bind_text(stmt_t::INSERT_RELATIONS, 4, it->modifier.c_str(), "insert modifier");
		}
		OK_OR_RET_ERR();


		this->bind_int(stmt_t::INSERT_RELATIONS, 5, it->modifier_type, "modifier type");
		this->bind_int(stmt_t::INSERT_RELATIONS, 6, it->opr8r, "modifier operator");
		OK_OR_RET_ERR();

		sqlite3_stmt *stmt = get_stmt(stmt_t::INSERT_RELATIONS);
		int s_rc = sqlite3_step(stmt);

		if (s_rc == SQLITE_DONE) {
			num_changed += sqlite3_changes(_db);
		} else if (s_rc == SQLITE_CONSTRAINT) {
			// sqlite does't tell us whether the object or relator caused the violation
			const char* errmsg = sqlite3_errmsg(_db);

			TAGDB_LOG_DEBUG( "SQLITE_CONSTRAINT: " << errmsg << std::endl );

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

		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	} while(++it != t.relations.end());

	if (num_changed == 0) {
		if (flags & F_IGNORE_DUPLICATES)
			return tagd::TAGD_OK;
		else
			return this->ferror(tagd::TS_DUPLICATE, "duplicate tag: %s", t.id().c_str());
	} else {
		return tagd::TAGD_OK;
	}
}

tagd::code sqlite::insert_referent(const tagd::referent& put_ref, session *ssn, flags_t flags) {
	tagd::referent t =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
			? tagd::referent(put_ref)
			: [&]() {
				tagd::abstract_tag decoded = this->decode_referents(put_ref, ssn);
				if (decoded.id() == put_ref.id())
					return tagd::referent(decoded);

				/*
				 * Referent insertion decodes refers_to/context, but the refers symbol
				 * itself must remain the caller-facing lexeme.
				 */
				tagd::abstract_tag semantic(
					put_ref.id(),
					decoded.sub_relator(),
					decoded.super_object(),
					decoded.pos()
				);
				semantic.relations = decoded.relations;
				return tagd::referent(semantic);
			}();

	if (t.refers() == t.refers_to())
		RET_SSN_ERROR(tagd::TS_MISUSE, tagd::predicate(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_REFERS_TO));

	if ( t.refers().empty() || t.refers() == HARD_TAG_ENTITY
		|| t.refers_to().empty()  // <refers> refers_to _entity -- OK
		|| t.context().empty() || t.context() == HARD_TAG_ENTITY )
	{
		if (ssn) {
			tagd::error e(tagd::TS_MISUSE, "illegal value in referent");

			// TODO(cpp23-return-contracts): use a helper for optional error details.
			// This branch reports misuse even when adding its cause fails.
			if (t.refers().empty())
				(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_EMPTY);
			else if (t.refers() == HARD_TAG_ENTITY)
				(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS, HARD_TAG_ENTITY);

			if (t.refers_to().empty())
				(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_REFERS_TO, HARD_TAG_EMPTY);

			if (t.context().empty())
				(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_EMPTY);
			else if (t.context() == HARD_TAG_ENTITY)
				(void)e.relation(HARD_TAG_CAUSED_BY, HARD_TAG_CONTEXT, HARD_TAG_ENTITY);

			ssn->error(e);
		}

		return tagd::TS_MISUSE;
	}

	this->prepare(stmt_t::INSERT_REFERENTS,
		"INSERT INTO referents (refers, refers_to, context) "
		"VALUES (tid(?), tid(?), tid(?))",
		"insert_referent"
	);
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent");

	this->put_term(t.refers(), tagd::POS_REFERS);
	this->bind_text(stmt_t::INSERT_REFERENTS, 1, t.refers().c_str(), "insert refers");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_refers");

	this->put_term(t.refers_to(), tagd::POS_REFERS_TO);
	this->bind_text(stmt_t::INSERT_REFERENTS, 2, t.refers_to().c_str(), "insert refers_to");
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_refers_to");

	if (t.context().empty())
		this->bind_null(stmt_t::INSERT_REFERENTS, 3, "insert NULL context");
	else {
		this->put_term(t.context(), tagd::POS_CONTEXT);
		this->bind_text(stmt_t::INSERT_REFERENTS, 3, t.context().c_str(), "insert context");
	}
	OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:insert_referent:bind_context");

	int s_rc = sqlite3_step(get_stmt(stmt_t::INSERT_REFERENTS));

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

	TAGDB_LOG_DEBUG( "SQLITE_CONSTRAINT: " << errmsg << std::endl );

	ts_sqlite_code ts_sql_rc = sqlite_constraint_type(errmsg);
	if (ts_sql_rc == TS_SQLITE_UNIQUE) { // referents UNIQUE violation
		// Only the complete binding is a duplicate; a colliding alias promises different state.
		prepare(stmt_t::REFERENT_BINDING,
			"SELECT 1 FROM referents WHERE refers = tid(?) "
			"AND refers_to = tid(?) AND context = tid(?)", "referent binding");
		OK_OR_RET_SSN_INT_ERR_ACTION("insert_referent:binding");
		bind_text(stmt_t::REFERENT_BINDING, 1, t.refers().c_str(), "refers");
		bind_text(stmt_t::REFERENT_BINDING, 2, t.refers_to().c_str(), "refers_to");
		bind_text(stmt_t::REFERENT_BINDING, 3, t.context().c_str(), "context");
		OK_OR_RET_SSN_INT_ERR_ACTION("insert_referent:binding_values");
		const auto match = sqlite3_step(get_stmt(stmt_t::REFERENT_BINDING));
		sqlite3_reset(get_stmt(stmt_t::REFERENT_BINDING));
		if (match == SQLITE_DONE)
			RET_SSN_FERROR(tagd::TS_AMBIGUOUS, "conflicting referent: %s", t.refers().c_str());
		if (match != SQLITE_ROW)
			RET_SQLITE_SSN_FERROR(match, "cannot check referent binding: %s", t.refers().c_str());
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

void sqlite::encode_referent(tagd::id_string &to, const tagd::id_string &from, session *ssn) {
	if (from.empty()) return;

	if (this->refers(to, from, ssn) == tagd::TAGD_OK)  // to set
		return;

	to = from;
}

/*
 * TODO(tag-modifiers): Modifiers must not undergo referent encoding/decoding
 * until modifier types are extended with a tag type. Only that future type
 * may resolve referents; existing literal types must retain their values.
 */
void sqlite::encode_referents(tagd::predicate_set& to, const tagd::predicate_set& from, session* ssn) {
	auto encoded = from;
	encoded.transform(this->f_encode_referent(ssn));
	to.insert(encoded.begin(), encoded.end());
}

tagd::abstract_tag sqlite::encode_referents(const tagd::abstract_tag& from, session* ssn) {
	// transform referents in put_tag to their "refers_to" given context
	tagd::id_string encoded_id;
	tagd::id_string encoded_super_object;

	if (!from.id().empty())
		this->encode_referent(encoded_id, from.id(), ssn);
	if (!from.super_object().empty())
		this->encode_referent(encoded_super_object, from.super_object(), ssn);

	tagd::abstract_tag semantic(
		encoded_id,
		from.sub_relator(),
		encoded_super_object,
		from.pos()
	);
	tagd::abstract_tag encoded =
		from.rank().empty() ? std::move(semantic) : tagd::abstract_tag(semantic, from.rank());
	this->encode_referents(encoded.relations, from.relations, ssn);
	return encoded;
}


void sqlite::decode_referent(tagd::id_string &to, const tagd::id_string &from, session* ssn) {
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
		to.insert(p);
	}
}

tagd::abstract_tag sqlite::decode_referents(const tagd::abstract_tag& from, session *ssn) {
	// transform referents in put_tag to their "refers_to" given context
	tagd::id_string decoded_id;
	tagd::id_string decoded_sub_relator;
	tagd::id_string decoded_super_object;

	if (!from.id().empty())
		this->decode_referent(decoded_id, from.id(), ssn);
	if (!from.sub_relator().empty())
		this->decode_referent(decoded_sub_relator, from.sub_relator(), ssn);
	if (!from.super_object().empty())
		this->decode_referent(decoded_super_object, from.super_object(), ssn);

	tagd::abstract_tag semantic(
		decoded_id,
		decoded_sub_relator,
		decoded_super_object,
		from.pos()
	);
	tagd::abstract_tag decoded =
		from.rank().empty() ? std::move(semantic) : tagd::abstract_tag(semantic, from.rank());
	this->decode_referents(decoded.relations, from.relations, ssn);
	decoded.type_relations = from.type_relations;
	decoded.type_relations.transform([this, ssn](const tagd::id_string& id) {
		tagd::id_string result;
		decode_referent(result, id, ssn);
		return result;
	});
	return decoded;
}

tagd::interrogator sqlite::decode_referents(const tagd::interrogator& from, session *ssn) {
	tagd::id_string decoded_id;
	tagd::id_string decoded_sub_relator;
	tagd::id_string decoded_super_object;

	if (!from.id().empty())
		this->decode_referent(decoded_id, from.id(), ssn);
	if (!from.sub_relator().empty())
		this->decode_referent(decoded_sub_relator, from.sub_relator(), ssn);
	if (!from.super_object().empty())
		this->decode_referent(decoded_super_object, from.super_object(), ssn);

	tagd::interrogator decoded(decoded_id, decoded_sub_relator, decoded_super_object);
	this->decode_referents(decoded.relations, from.relations, ssn);
	decoded.type_relations = from.type_relations;
	decoded.type_relations.transform([this, ssn](const tagd::id_string& id) {
		tagd::id_string result;
		decode_referent(result, id, ssn);
		return result;
	});
	return decoded;
}

tagd::code sqlite::related(tagd::tag_set& R, const tagd::predicate& rel, const tagd::id_string& sup, session* ssn, flags_t flags) {
	/*
	 * TODO(filepile-integrity): The optional-filter query plan scans relations for
	 * both filepile history and content lookup. Measure queries with only active
	 * filters before adding indexes in the verification/cache follow-on.
	 */
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	tagd::predicate p = rel;
	tagd::id_string super_object;
	if (!ssn || (flags & F_NO_TRANSFORM_REFERENTS)) {
		super_object = sup;
	} else {
		this->decode_referent(super_object, sup, ssn);
		this->decode_referent(p.relator, rel.relator, ssn);
		this->decode_referent(p.object, rel.object, ssn);
	}

	int pos_i = 0;  // binding position

	auto f_bind_null_text = [this, &pos_i](bool not_null, const tagd::id_string &id) {
			if (not_null) {
				this->bind_text(stmt_t::RELATED, ++pos_i, id.c_str(), "test");
				this->bind_text(stmt_t::RELATED, ++pos_i, id.c_str(), "value");
			} else {
				this->bind_null(stmt_t::RELATED, ++pos_i, "null test");
				this->bind_null(stmt_t::RELATED, ++pos_i, "null");
			}
	};

	auto f_bind_null_int_text = [this, &pos_i](bool not_null, const tagd::predicate &p) {
			if (not_null) {
				if (p.modifier_type == tagd::TYPE_STRING) {
					this->bind_text(stmt_t::RELATED, ++pos_i, p.modifier.c_str(), "test");
					this->bind_text(stmt_t::RELATED, ++pos_i, p.modifier.c_str(), "value");
				} else {
					this->bind_int(stmt_t::RELATED, ++pos_i, atoi(p.modifier.c_str()), "test");
					this->bind_int(stmt_t::RELATED, ++pos_i, atoi(p.modifier.c_str()), "value");
				}
			} else {
				this->bind_null(stmt_t::RELATED, ++pos_i, "null test");
				this->bind_null(stmt_t::RELATED, ++pos_i, "null");
			}
	};

	this->prepare(stmt_t::RELATED,
		"SELECT idt(subject), idt(sub_relator), idt(super_object), pos, rank, "
		"idt(relator), idt(object), idt(modifier), modifier_type, operator "
		"FROM tags, relations "
		"WHERE tag = subject "
		"AND ("
			"? IS NULL OR subject IN ( "
			"SELECT tag FROM tags WHERE rank GLOB rank_glob(( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			"))"
			") "
		") "
		"AND ("
			"? IS NULL OR relator IN ( "
			"SELECT tag FROM tags WHERE rank GLOB rank_glob(( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			"))"
			") "
		") "
		"AND ("
			"? IS NULL OR object IN ( "
			"SELECT tag FROM tags WHERE rank GLOB rank_glob(( "
			"SELECT rank FROM tags WHERE tag = tid(?) "
			"))"
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
	int s_rc;

	sqlite3_stmt *stmt = get_stmt(stmt_t::RELATED);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
		const char *raw_subject = (const char*) sqlite3_column_text(stmt, F_SUBJECT);
		tagd::id_string populated_id;
		auto tc = populate_row_id(populated_id, raw_subject, pos, f_transform);
		if (tc != tagd::TAGD_OK) {
			if (ssn)
				ssn->ferror(tc, "failed to populate related tag: %s", raw_subject);
			return tc;
		}

		tagd::abstract_tag semantic(
			populated_id,
			f_transform((const char*) sqlite3_column_text(stmt, F_SUB_REL)),
			f_transform((const char*) sqlite3_column_text(stmt, F_SUB_OBJ)),
			pos
		);
		tagd::rank rank = make_rank((const char*) sqlite3_column_text(stmt, F_RANK));
		tagd::abstract_tag populated =
			rank.empty() ? std::move(semantic) : tagd::abstract_tag(semantic, rank);

		auto pred = tagd::predicate(
			(const char*) sqlite3_column_text(stmt, F_RELATOR),
			(const char*) sqlite3_column_text(stmt, F_OBJECT)
		);

		if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			pred.modifier = (const char*) sqlite3_column_text(stmt, F_MODIFIER);
		}
		pred.modifier_type = static_cast<tagd::data_t>(sqlite3_column_int(stmt, F_MODIFIER + 1));
		pred.opr8r = static_cast<tagd::operator_t>(sqlite3_column_int(stmt, F_MODIFIER + 2));
		const auto inserted = populated.relation(pred);
		if (inserted != tagd::TAGD_OK) {
			return inserted;
		}
		auto ordered = order_predicates(populated.relations);
		if (ordered != tagd::TAGD_OK)
			return ordered;
		populated.relations.transform(f_transform);

		TAGDB_LOG_DEBUG( "related R.insert: " << populated << std::endl );

		it = R.insert(it, populated);
	}

	if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc,
			"related failed(super_object, relator, object, modifier, opr8r): %s, %s, %s, %s, %s",
			super_object.c_str(), p.relator.c_str(), p.object.c_str(), p.modifier.c_str(), p.op_c_str());
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:related:step");
	}

	return (R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::get_children(tagd::tag_set& R, const tagd::id_string& super_object, session *ssn, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	this->prepare(stmt_t::GET_CHILDREN,
		"SELECT idt(tag), idt(sub_relator), idt(super_object), pos, rank "
		"FROM tags WHERE super_object = tid(?) "
		"ORDER BY rank",
		"select children"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::GET_CHILDREN, 1, super_object.c_str(), "get children super_object");
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

	sqlite3_stmt *stmt = get_stmt(stmt_t::GET_CHILDREN);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::part_of_speech pos = (tagd::part_of_speech) sqlite3_column_int(stmt, F_POS);
		const char *raw_id = (const char*) sqlite3_column_text(stmt, F_ID);
		tagd::id_string populated_id;
		auto tc = populate_row_id(populated_id, raw_id, pos, f_transform);
		if (tc != tagd::TAGD_OK) {
			this->ferror(tc, "failed to populate child tag: %s", raw_id);
			return this->code();
		}

		tagd::abstract_tag semantic(
			populated_id,
			f_transform((const char*) sqlite3_column_text(stmt, F_SUB_REL)),
			f_transform((const char*) sqlite3_column_text(stmt, F_SUB_OBJ)),
			pos
		);
		tagd::rank rank = make_rank((const char*) sqlite3_column_text(stmt, F_RANK));
		tagd::abstract_tag populated =
			rank.empty() ? std::move(semantic) : tagd::abstract_tag(semantic, rank);

		if (flags & F_INCLUDE_INHERITED_PREDICATES) {
			tagd::id_string id(raw_id);
			tc = get(populated, id, ssn, flags);
			if (tc != tagd::TAGD_OK) {
				return tc;
			}
		}
		it = R.insert(it, populated);
	}

	if (s_rc == SQLITE_ERROR) {
		SQLITE_FERROR(s_rc, "get_children failed: %s", super_object.c_str());
		OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:get_children:step");
	}

	return (R.size() == 0 ?  tagd::TS_NOT_FOUND : tagd::TAGD_OK);
}

tagd::code sqlite::query_referents(tagd::tag_set& R, const tagd::interrogator& intr) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	TAGDB_LOG_DEBUG( "sqlite::query: " << intr << std::endl );

	tagd::id_string refers, refers_to, context;
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
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents",
					"query referent context"
				);
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",

					"query referent context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context.empty()) {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers_to = tid(?)",
					"query referent refers_to"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers_to = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					"query referent refers_to, context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		}
	} else {  // !refers.empty()
		if (refers_to.empty()) {
			if (context.empty()) {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?)",
					"query referent refers"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					"query referent refers context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, context.c_str(), "context");
				STMT_OK_OR_RET_ERR();
			}
		} else {  // !refers_to.empty()
			if (context.empty()) {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?)",
					"query referent refers, refers_to"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();
			} else {
				prepare_stmt(this, _db, &stmt,
					"SELECT idt(refers), idt(refers_to), idt(context) "
					"FROM referents "
					"WHERE refers = tid(?) "
					"AND refers_to = tid(?) "
					"AND context IN("
					 "SELECT tag FROM tags "
					 "WHERE rank GLOB rank_glob(("
					 "SELECT rank FROM tags WHERE tag = tid(?)"
					 ")) " // all context <= {context}
					")",
					"query referent refers, refers_to, context"
				);
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 1, refers.c_str(), "refers");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 2, refers_to.c_str(), "refers_to");
				STMT_OK_OR_RET_ERR();

				bind_text_stmt(this, stmt, 3, context.c_str(), "context");
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

		auto ordered = order_predicates(r.relations);
		if (ordered != tagd::TAGD_OK) {
			sqlite3_finalize(stmt);
			return ordered;
		}
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
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	// Matching is independent of the inherited-display flag.
	if (!(flags & F_NO_RESET)) this->reset(ssn);
	tagdb_debug_scope debug_scope(this, ssn);

	//TODO use the id (who, what, when, where, why, how_many...)
	// to distinguish types of queries
	assert(!q.empty());
	TAGDB_LOG_DEBUG("tagdb query subject=" << q.id());
	TAGDB_LOG_EVENT(ssn, tagd::log_level::INFO, std::string(HARD_TAG_TAGDB_QUERY_EVENT));

	tagd::interrogator intr =
		(!ssn || (flags & F_NO_TRANSFORM_REFERENTS))
			? q
			: this->decode_referents(q, ssn);
	OK_OR_RET_SSN_INT_ERR_ACTION("tadb:query:decode_referents");

	if (intr.super_object() == HARD_TAG_REFERENT)
		RET_SSN_CODE(this->query_referents(R, intr));

	if (intr.relations.empty() && intr.type_relations.empty()) {
		if (intr.super_object().empty()) {
			RET_SSN_ERROR(tagd::TS_MISUSE, "interrogator with empty relations and empty super_object");
		} else {
			auto tc = this->get_children(R, intr.super_object(), ssn, flags);
			if (tc == tagd::TS_NOT_FOUND && (flags & F_NO_NOT_FOUND_ERROR))
				return tagd::TS_NOT_FOUND;
			else
				RET_SSN_CODE(tc);
		}
	}

	if (_relation_semantics == relation_semantics::assertions) {
		const auto rc = query_effective(R, intr, ssn, flags);
		if (rc == tagd::TS_NOT_FOUND && (flags & F_NO_NOT_FOUND_ERROR)) {
			return rc;
		}
		RET_SSN_CODE(rc);
	}

	size_t n = 0;
	tagd::tag_set S;  // related per predicate
	/*
	 * Query intersection retains predicates from its first condition. Preserve
	 * semantic condition order independently of the returned snapshot's rank order.
	 */
	std::vector<tagd::predicate> conditions(intr.relations.begin(), intr.relations.end());
	std::sort(conditions.begin(), conditions.end());
	for (const auto& p : conditions) {
		S.clear();

		if (p.object == HARD_TAG_TERMS) {
			(void)this->search(S, p.modifier, flags);
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:query:search");
		} else {
			// only super_object is advantagious in related query (sub_relator not needed)
			auto tc = this->related(S, p, intr.super_object(), ssn, flags);
			if (tc == tagd::TS_NOT_FOUND && (flags & F_NO_NOT_FOUND_ERROR))
				return tagd::TS_NOT_FOUND;
			if (tc != tagd::TAGD_OK)
				RET_SSN_CODE(tc);
			OK_OR_RET_SSN_INT_ERR_ACTION("tagdb:query:related");
		}

		if (p.object == HARD_TAG_TERMS) {
			TAGDB_LOG_DEBUG( "search: " << p.modifier << std::endl );
		} else {  // TODO predicate iostream op
			TAGDB_LOG_DEBUG( "related: " << p << std::endl );
		}

		OK_OR_RET_ERR();
		n += merge_containing_tags(R, S);
	}

	if (n == 0) {
		if (flags & F_NO_NOT_FOUND_ERROR)
			return tagd::TS_NOT_FOUND;
		else
			RET_SSN_CODE(tagd::TS_NOT_FOUND);
	}

	RET_SSN_CODE(tagd::TAGD_OK);
}

tagd::code sqlite::search(tagd::tag_set& R, const std::string &terms, flags_t flags) {
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	//TODO use the id (who, what, when, where, why, how_many...)
	// to distinguish types of queries

	if (terms.empty())
		return tagd::TS_NOT_FOUND;

	this->prepare(stmt_t::SEARCH,
		"SELECT idt(docid) FROM fts_tags "
		"WHERE content MATCH ?",
		"search"
	);
	OK_OR_RET_ERR();

	this->bind_text(stmt_t::SEARCH, 1, terms.c_str(), "search terms");
	OK_OR_RET_ERR();

	const int F_TAG_ID = 0;

	TAGDB_LOG_DEBUG( "search: " << terms << std::endl );

	int s_rc;
	size_t n = 0;
	sqlite3_stmt *stmt = get_stmt(stmt_t::SEARCH);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			std::string tag_id( (const char*) sqlite3_column_text(stmt, F_TAG_ID) );
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
	tagd::code tc = prepare_stmt(this, _db, &stmt,
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
	tc = prepare_stmt(this, _db, &stmt,
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
	tagd::code tc = prepare_stmt(this, _db, &stmt,
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
	read_scope snapshot(*this);
	if (snapshot.status != tagd::TAGD_OK)
		return snapshot.status;
	this->reset(nullptr);

	/*
	 * Declare the hierarchy before restoring custom subordinate relators and
	 * predicates. Rank order puts parents first, but a relator may be declared
	 * later than its users or participate in a cycle of relator dependencies.
	 */
	sqlite3_stmt *stmt = nullptr;
	tagd::code tc = prepare_stmt(this, _db, &stmt,
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

	// URL syntax owns subordinate identity; stored-file classification is a type relation.
	const auto stored_url = [this](tagd::url& value, const tagd::id_string& id,
		const tagd::id_string& parent) {
		tagd::HDURI decoded(id);
		if (!decoded.ok())
			return this->ferror(decoded.code(), "invalid stored URL: %s", id.c_str());

		if (parent == tagd::HARD_TAG_FILE_STORED)
			return this->error(tagd::TS_MISUSE, "incompatible stored URL identity; recreate this pre-beta tagspace");
		value = tagd::url(decoded.id());
		if (!value.ok())
			return this->ferror(value.code(), "invalid stored URL: %s", id.c_str());

		return tagd::TAGD_OK;
	};

	std::vector<tagd::abstract_tag> custom_sub_relations;
	int s_rc;
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::id_string id{(const char*) sqlite3_column_text(stmt, F_ID)};
		tagd::part_of_speech pos{(tagd::part_of_speech) sqlite3_column_int(stmt, F_POS)};

		// ignore hard tags
		if (id[0] == '_') continue;

		// Links can precede URLs in rank order; declare every identity before predicates.
		if (pos == tagd::POS_URL) {
			tagd::url u;
			tc = stored_url(u, id, (const char*)sqlite3_column_text(stmt, F_SUB_OBJ));
			if (tc != tagd::TAGD_OK) {
				sqlite3_finalize(stmt);
				return tc;
			}
			os << ">> " << u << std::endl << std::endl;
			continue;
		}

		tagd::abstract_tag t(
				id,
				(const char*) sqlite3_column_text(stmt, F_SUB_REL),
				(const char*) sqlite3_column_text(stmt, F_SUB_OBJ),
				pos );
		if (t.sub_relator() != HARD_TAG_SUB) {
			custom_sub_relations.push_back(t);
			os << ">> " << tagd::abstract_tag(id, HARD_TAG_SUB, t.super_object(), pos)
				<< std::endl << std::endl;
		} else {
			os << ">> " << t << std::endl << std::endl;
		}
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump tags failed");

	for (const auto& t : custom_sub_relations) {
		os << ">> " << t << std::endl << std::endl;
	}

	// URL PUT reconstructs component predicates; repeating those alone is a duplicate PUT.
	auto emit_predicates = [this, &os](tagd::abstract_tag& value) {
		if (value.pos() == tagd::POS_URL) {
			tagd::predicate_set implicit;
			tagd::url::insert_url_part_relations(implicit, static_cast<const tagd::url&>(value));
			for (const auto& p : implicit) {
				value.relations.erase(p);
			}
		}
		if (value.relations.empty())
			return tagd::TAGD_OK;
		auto rc = order_predicates(value.relations);
		if (rc == tagd::TAGD_OK)
			os << ">> " << value << std::endl << std::endl;
		return rc;
	};

	stmt = nullptr;
	tc = prepare_stmt(this, _db, &stmt,
		"SELECT DISTINCT idt(subject) FROM type_relations JOIN tags ON tag=subject ORDER BY rank",
		"dump type relations");
	if (tc != tagd::TAGD_OK) {
		sqlite3_finalize(stmt);
		return tc;
	}
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::abstract_tag declaration((const char*)sqlite3_column_text(stmt, 0));
		tc = get_type_relations(declaration.type_relations, declaration.id());
		if (tc != tagd::TAGD_OK) {
			sqlite3_finalize(stmt);
			return tc;
		}
		os << ">> " << declaration << std::endl << std::endl;
	}
	sqlite3_finalize(stmt);
	if (s_rc != SQLITE_DONE) {
		return error(tagd::TS_INTERNAL_ERR, "cannot dump type relations");
	}

	// dump relations
	stmt = nullptr;
	tc = prepare_stmt(this, _db, &stmt,
		"SELECT idt(subject), idt(relator), idt(object), idt(modifier), pos, idt(super_object), modifier_type, operator "
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
	tagd::id_string previous_subject;
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		tagd::id_string id{(const char*) sqlite3_column_text(stmt, F_ID)};
		tagd::part_of_speech pos{(tagd::part_of_speech) sqlite3_column_int(stmt, F_POS)};
		tagd::id_string object{(const char*) sqlite3_column_text(stmt, F_OBJECT)};

		// ignore hard tags
		if (id[0] == '_') continue;

		// A mutable subject's predicates remain data even when their objects are hard tags.

		if (t == nullptr || previous_subject != id) {
			// Stored subject IDs are HDURIs, while URL objects expose canonical URLs.
			previous_subject = id;
			if (t != nullptr) {
				auto ordered = emit_predicates(*t);
				if (ordered != tagd::TAGD_OK) {
					delete t;
					sqlite3_finalize(stmt);
					return ordered;
				}
				delete t;
			}

			if (pos == tagd::POS_URL) {
				tagd::url u;
				tc = stored_url(u, id, (const char*)sqlite3_column_text(stmt, 5));
				if (tc != tagd::TAGD_OK) {
					sqlite3_finalize(stmt);
					return tc;
				}
				t = new tagd::url(u);
			} else {
				t = new tagd::abstract_tag(id);
			}
		}

		if (sqlite3_column_type(stmt, F_MODIFIER) != SQLITE_NULL) {
			// TODO(cpp23-return-contracts): handle relation() failure when building a dump.
			// It is currently ignored, so the dump can omit an invalid relation.
			(void)t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				object,
				(const char*) sqlite3_column_text(stmt, F_MODIFIER),
				static_cast<tagd::operator_t>(sqlite3_column_int(stmt, 7)),
				static_cast<tagd::data_t>(sqlite3_column_int(stmt, 6)) );
		} else {
			(void)t->relation(
				(const char*) sqlite3_column_text(stmt, F_RELATOR),
				object );
		}
	}

	if (t != nullptr) {
		auto ordered = emit_predicates(*t);
		if (ordered != tagd::TAGD_OK) {
			delete t;
			sqlite3_finalize(stmt);
			return ordered;
		}
		delete t;
	}

	sqlite3_finalize(stmt);

	if (s_rc == SQLITE_ERROR)
		RET_SQLITE_FERROR(s_rc, "dump relations failed");

	stmt = nullptr;
	tc = prepare_stmt(this, _db, &stmt,
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
	tagd::code tc = prepare_stmt(this, _db, &stmt,
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


tagd::code sqlite::max_child_rank(tagd::rank& next, const tagd::id_string& super_object) {
	(void)this->open();
	OK_OR_RET_ERR();

	int s_rc = this->prepare(stmt_t::MAX_CHILD_RANK,
		"SELECT rank FROM tags WHERE super_object = tid(?) ORDER BY rank DESC LIMIT 1",
		"child ranks"
	);

	this->bind_text(stmt_t::MAX_CHILD_RANK, 1, super_object.c_str(), "rank super_object");
	OK_OR_RET_ERR();

	const int F_RANK = 0;
	tagd::code rc;

	sqlite3_stmt *stmt = get_stmt(stmt_t::MAX_CHILD_RANK);
	s_rc = sqlite3_step(stmt);

	if (s_rc == SQLITE_ROW) {
		rc = next.init( (const char*) sqlite3_column_text(stmt, F_RANK));
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

tagd::code sqlite::child_ranks(tagd::rank_set& R, const tagd::id_string& super_object) {
	(void)this->open();
	OK_OR_RET_ERR();


	int s_rc = this->prepare(stmt_t::CHILD_RANKS,
		"SELECT rank FROM tags WHERE super_object = tid(?)",
		"child ranks"
	);

	this->bind_text(stmt_t::CHILD_RANKS, 1, super_object.c_str(), "rank super_object");
	OK_OR_RET_ERR();

	const int F_RANK = 0;
	tagd::rank rank;
	tagd::code rc;

	sqlite3_stmt *stmt = get_stmt(stmt_t::CHILD_RANKS);
	while ((s_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		rc = rank.init( (const char*) sqlite3_column_text(stmt, F_RANK));
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
	if (_write_poisoned) {
		return error(tagd::TS_INTERNAL_ERR, "commit outcome unknown; reopen and check storage");
	}
	// Internal SQL can mutate schema or data without a public PUT/DELETE call.
	mutation_scope mutation(*this);
	// ROLLBACK must execute even while the triggering operation's error is recorded.
	auto rc = this->open();
	if (rc != tagd::TAGD_OK)
		return rc;

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

tagd::code sqlite::prepare(stmt_t k, const char *sql, const char *label) {
	sqlite3_stmt *&stmt = _stmts[k];  // reference into map; operator[] inserts nullptr if absent
	if (stmt == nullptr) {
		int s_rc = sqlite3_prepare_v2(
			_db,
			sql,
			-1,         // -1 copies \0 term string, otherwise use nbytes
			&stmt,      // OUT: Statement handle
			nullptr     // OUT: Pointer to unused portion of zSql
		);
		if (s_rc != SQLITE_OK) {
			if (label != nullptr)
				return this->ferror(tagd::TS_INTERNAL_ERR, "'%s` prepare statement failed: %s", label, sqlite3_errmsg(_db));
			else
				return this->ferror(tagd::TS_INTERNAL_ERR, "prepare statement failed: %s", sqlite3_errmsg(_db));
		}
	} else {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}

	return tagd::TAGD_OK;
}

sqlite3_stmt* sqlite::get_stmt(stmt_t k) {
	auto it = _stmts.find(k);
	return (it != _stmts.end()) ? it->second : nullptr;
}

tagd::code sqlite::bind_text(stmt_t k, int i, const char *text, const char*label) {
	sqlite3_stmt *stmt = get_stmt(k);
	if (stmt == nullptr)
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind missing statement: %s", label);
	int s_rc;
	if ((s_rc = sqlite3_bind_text(stmt, i, text, -1, SQLITE_TRANSIENT)) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		_stmts.erase(k);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind_text returned %s for label %s: %s", sqlite_err_code_str(s_rc), label, text);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_int(stmt_t k, int i, int val, const char*label) {
	sqlite3_stmt *stmt = get_stmt(k);
	if (stmt == nullptr)
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind missing statement: %s", label);
	if (sqlite3_bind_int(stmt, i, val) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		_stmts.erase(k);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_rowid(stmt_t k, int i, rowid_t id, const char*label) {
	if (id <= 0)  // interpret as NULL
		return this->bind_null(k, i, label);

	sqlite3_stmt *stmt = get_stmt(k);
	if (stmt == nullptr)
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind missing statement: %s", label);
	if (sqlite3_bind_int64(stmt, i, id) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		_stmts.erase(k);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

tagd::code sqlite::bind_null(stmt_t k, int i, const char*label) {
	sqlite3_stmt *stmt = get_stmt(k);
	if (stmt == nullptr)
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind missing statement: %s", label);
	if (sqlite3_bind_null(stmt, i) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		_stmts.erase(k);
		return this->ferror(tagd::TS_INTERNAL_ERR, "bind failed: %s", label);
	}

	return tagd::TAGD_OK;
}

// Keep cached statements until the connection closes; finalize them together.
void sqlite::finalize() {
	for (auto& [k, stmt] : _stmts)
		sqlite3_finalize(stmt);
	_stmts.clear();
}

} // namespace tagdb
