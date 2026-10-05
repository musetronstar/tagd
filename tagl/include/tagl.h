#pragma once

#include <sstream>
#include <string>
#include "tagd.h"
#include "tagd/hard-tags.h"
#include "tagd/logger.h"
#include "tagspace.h"
#include "parser.h"

// forward declare types used by lemon parser
struct yyParser;

extern bool TAGL_TRACE_ON;
void TAGL_SET_TRACE_ON();
void TAGL_SET_TRACE_OFF();
void TAGL_SET_LOGGER(tagd::logger *);
bool TAGL_LOG_ENABLED(const std::string&, tagd::log_level);
void TAGL_LOG(const std::string&, tagd::log_level, const std::string&);
#define TAGL_LOG_TRACE(MSG) if(TAGL_TRACE_ON) \
	{ std::cerr <<  __FILE__  << ':' << __LINE__ << '\t' << MSG ; }
#define TAGL_LOG_SCANNER_DEBUG(MSG) \
	do { \
		if (TAGL_LOG_ENABLED(std::string(HARD_TAG_ROLE_SCANNER), tagd::log_level::DEBUG)) { \
			std::ostringstream _tagl_scanner_debug_os; \
			_tagl_scanner_debug_os << MSG; \
			TAGL_LOG(std::string(HARD_TAG_ROLE_SCANNER), tagd::log_level::DEBUG, _tagl_scanner_debug_os.str()); \
		} \
	} while (0)
#define TAGL_LOG_DRIVER_DEBUG(MSG) \
	do { \
		if (TAGL_LOG_ENABLED(std::string(HARD_TAG_ROLE_DRIVER), tagd::log_level::DEBUG)) { \
			std::ostringstream _tagl_driver_debug_os; \
			_tagl_driver_debug_os << MSG; \
			TAGL_LOG(std::string(HARD_TAG_ROLE_DRIVER), tagd::log_level::DEBUG, _tagl_driver_debug_os.str()); \
		} \
	} while (0)

namespace TAGL {
struct text_token {
	// Non-owning parser view; storage comes from token_store and must outlive this slice.
	const char *z;
	int n;

	bool empty() const { return z == nullptr || n == 0; }
	std::string str() const { return empty() ? std::string() : std::string(z, n); }
};

inline constexpr text_token EMPTY_VALUE{nullptr, 0};
}

#include "scanner.h"

namespace TAGL {

const char* token_str(int);

struct NoValue {};

class driver;

class callback {
	friend class TAGL::driver;

	protected:
		driver *_driver = nullptr;

	public:
		callback() {}
		virtual ~callback() {}
		callback(const callback&) = delete;
		callback& operator=(const callback&) = delete;
		callback(callback&&) = delete;
		callback& operator=(callback&&) = delete;

		// pure virtuals - consumers must override
		virtual void cmd_get(const tagd::abstract_tag&) = 0;
		virtual void cmd_put(const tagd::abstract_tag&) = 0;
		virtual void cmd_del(const tagd::abstract_tag&) = 0;
		virtual void cmd_query(const tagd::interrogator&) = 0;
		virtual void cmd_error() = 0;
		virtual void finish() {} // optional, can be overridden
};

const size_t BUF_SZ = 16384;

class driver : public tagd::errorable {
	friend class TAGL::scanner;

	protected:
		bool _own_scanner = false;
		bool _own_session = false;
		bool _error_callback_delivered = false;

		// number of items pushed on the stack by this parser
		size_t _context_level = 0;
		// pop off only what this instance pushed on (leaving previous items untouched)

		// The driver may own the scanner, or borrow one supplied by a caller.
		scanner *_scanner = nullptr;
		// RAII owner of the lemon parser; ParseFree is called automatically on reset or destruction.
		static void parser_deleter(void* p);  // defined in tagl.cc: calls ParseFree(p, ::operator delete)
		std::unique_ptr<void, void(*)(void*)> _parser{nullptr, &driver::parser_deleter};
		int _token = -1;		// last token scanned: 0 = <End of Input>, -1 = unitialized (ready for new parse tree)
		int _cmd = -1;		// _token value representing a TAGL command

		// The caller owns the tagspace and must keep it alive until the driver is destroyed.
		tagd::tagspace *_tdb = nullptr;
		// Sessions may be borrowed or owned depending on how the driver was constructed.
		tagd::tagspace_session *_session = nullptr;
		// Callbacks are always borrowed; driver only binds and invokes them.
		callback *_callback = nullptr;
		// The current statement tag is owned by this driver until transferred or deleted.
		tagd::abstract_tag *_tag = nullptr;
		std::string _path;

		// sets up scanner and parser for a fresh start
		void init();
		void free_parser();
		int parse_tokens();
		text_token store_token_text(const std::string&);
		void bind_callback(callback *);

	public:
		driver(tagd::tagspace*, tagd::tagspace_session* = nullptr);
		driver(tagd::tagspace*, scanner*, tagd::tagspace_session* = nullptr);
		driver(tagd::tagspace*, scanner*, callback*, tagd::tagspace_session* = nullptr);
		driver(tagd::tagspace*, callback*, tagd::tagspace_session* = nullptr);
		virtual ~driver();
		driver(const driver&) = delete;
		driver& operator=(const driver&) = delete;
		driver(driver&&) = delete;
		driver& operator=(driver&&) = delete;

		tagd::flags_t flags = 0;
		tagd::id_string constrain_tag_id; // if set, the assigned _tag.id() must be equal to this

		// TODO remove from here, create/destroy relator in parser
		tagd::id_string relator;    // current relator

		tagd::tagspace* tdb() { return _tdb; }
		const tagd::tagspace* tdb() const { return _tdb; }
		void session_ptr(tagd::tagspace_session *ssn) { _session = ssn; }
		tagd::tagspace_session* session_ptr() { return _session; }
		const tagd::tagspace_session* session_ptr() const { return _session; }
		// sets _session and _own_session, delete old session (if set and not the same)
		void own_session(tagd::tagspace_session *);
		/*
		 * Not [[nodiscard]]: lemon-generated parser.cc calls this and discards; cannot change
		 * that file without regenerating from parser.y.
		 */
		tagd::code push_context(tagd::id_view);

		void callback_ptr(callback *c) {
			this->bind_callback(c);
		}
		callback *callback_ptr() { return _callback; }
		const callback *callback_ptr() const { return _callback; }

		/*
		 * Not [[nodiscard]]: errors surface through the cmd_error callback and the errorable
		 * mechanism; callers frequently drive parse/execute in callback-only mode.
		 */
		tagd::code parseln(const std::string& = std::string());
		tagd::code execute(const std::string&);
		tagd::code execute(evbuffer*);
		/*
		 * Not [[nodiscard]]: lemon-generated parser.cc calls this and discards; cannot change
		 * that file without regenerating from parser.y.
		 */
		tagd::code scan_tagdurl(int, const std::string& path);
		[[nodiscard]] tagd::code scan_tagdurl(tagd::http_method, const std::string& path);
		int token() const { return _token; }
		// Not const: downstream POS lookup may mutate diagnostic state on error paths.
		int lookup_pos(const std::string&);
		virtual void parse_tok(int, text_token);
		void parse_tok(int, const std::string&);
		void parse_tok(int, const char *);
		// Not [[nodiscard]]: lemon-generated parser.cc calls this and discards.
		tagd::code include_file(const std::string&);
		int open_rel(const std::string& path, int flags);

		void clear_context_levels();

		int cmd() const { return _cmd; }
		void cmd(int c) { _cmd = c; }

		size_t line_number() const {
			return _scanner->_line_number;
		}

		void path(const std::string& f) { _path = f; }
		const std::string& path() const { return _path; }

		bool is_setup() const;
		void do_callback();

		// adds end of input to parser and frees scanner and parser
		void finish();

		void tag_ptr(tagd::abstract_tag *t) { _tag = t; }
		tagd::abstract_tag* tag_ptr() const { return _tag; }
		const tagd::abstract_tag& tag() const {
			static const tagd::abstract_tag empty_tag;
			return (_tag == nullptr ? empty_tag : *_tag);
		}

		// checks that _tag is set to nullptr, otherwise deletes _tag and sets to nullptr
		void delete_tag() {
			if (_tag == nullptr) return;

			delete _tag;
			_tag = nullptr;
		}

		/*
		 * Not [[nodiscard]]: lemon-generated parser.cc calls this and discards.
		 * creates a new tagd::url and sets the _tag ptr
		 */
		tagd::code new_url(const std::string&);
};

} // namespace TAGL
