#pragma once

#include <cassert>
#include <stdint.h>
#include "tagd.h"

namespace tagdb {

typedef enum {
	F_NO_POS_CAST	         = 1 << 0, // don't cast a tag according to its pos (i.e. url, referent...)
	F_NO_TRANSFORM_REFERENTS = 1 << 1, // don't transform to/from referent to tag when putting/getting
	F_NO_NOT_FOUND_ERROR     = 1 << 2, // don't set error when get() returns TS_NOT_FOUND
	F_IGNORE_DUPLICATES      = 1 << 3, // don't set error when TS_DUPLICATE would be set 
	F_NO_RESET               = 1 << 4  // don't call reset() at the beginning of public tagdb methods
	// ...
	//          	= 1 << 31,
} ts_flags;
const int TS_FLAGS_END     = 1 << 5;

typedef uint32_t flags_t;

struct flag_util {
	static std::string flag_str(flags_t f) {
		if (f == 0)
			return "EMPTY_FLAGS";

        switch (f) {
			case F_NO_POS_CAST:            return "F_NO_POS_CAST";
			case F_NO_TRANSFORM_REFERENTS: return "F_NO_TRANSFORM_REFERENTS";
			case F_NO_NOT_FOUND_ERROR:     return "F_NO_NOT_FOUND_ERROR";
			case F_IGNORE_DUPLICATES:      return "F_IGNORE_DUPLICATES";
			case F_NO_RESET:               return "F_NO_RESET";
            default:                       return "FLAG_UNKNOWN";
        }
    }

	static std::string flag_list_str(flags_t f) {
		if (f == 0)
			return "EMPTY_FLAGS";

		std::string s;
		int i = 0;
		ts_flags flag = (ts_flags)(1 << i);
		if ((f & flag) == flag)
			s.append( flag_str(flag) );
		while ((flag=(ts_flags)(1<<(++i))) < TS_FLAGS_END) {
			if ((f & flag) == flag) {
				if (s.size() > 0)
					s.append(",");
				s.append(flag_str(flag));
			}
		}

		return s;
	}
};

// matches sqlite_int64 type defined in sqlite.h
typedef long long int rowid_t;

class hard_tag {
	public:
		static tagd::part_of_speech pos(const tagd::id_type &id);
		static tagd::code get(tagd::abstract_tag&, const tagd::id_type &id);
		static tagd::part_of_speech term_pos(const tagd::id_type&, rowid_t* = nullptr);
		static tagd::part_of_speech term_id_pos(rowid_t, tagd::id_type* = nullptr);

		static const char ** rows();
		static size_t rows_end();
};

typedef std::vector<tagd::id_type> context_stack;

// pure virtual interface
class tagdb : public tagd::errorable {
	protected:
		// stack of tags ids as context
		tagd::id_vec _context;
		bool _trace_on;
		void reset() { _code = tagd::TAGD_OK; }

	public:
		tagdb() : tagd::errorable(tagd::TS_INIT), _trace_on{false} {}
		virtual ~tagdb() {}

		virtual void trace_on() { _trace_on = true; }
		virtual void trace_off() { _trace_on = false; }

		virtual tagd::code push_context(const tagd::id_type&);
		virtual const tagd::id_vec& context() const;
		virtual tagd::code pop_context();
		virtual tagd::code clear_context();

		/*
		 * when implemented, the follow methods should begin with a
		 * call to this->reset() that respect the F_NO_RESET flag
		 * such as:  if (!(flags & F_NO_RESET)) this->reset();
		 */
		virtual tagd::code get(tagd::abstract_tag&, const tagd::id_type&, flags_t = 0) = 0; // get into tag, given id
		virtual tagd::code put(const tagd::abstract_tag&, flags_t = 0) = 0;
		virtual tagd::code del(const tagd::abstract_tag&, flags_t = 0) = 0;
		virtual tagd::code query(tagd::tag_set&, const tagd::interrogator&, flags_t = 0) = 0;
		virtual tagd::part_of_speech pos(const tagd::id_type&, flags_t = 0) = 0; 
		virtual bool exists(const tagd::id_type&, flags_t = 0) = 0;
		virtual tagd::code dump(std::ostream& os = std::cout) = 0;

		virtual tagd::code dump_grid(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
		virtual tagd::code dump_terms(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
        virtual tagd::code dump_search(std::ostream& = std::cout) { return tagd::TS_NOT_IMPLEMENTED; }
};

struct util {
	// users default db
	static std::string user_db();
};

} // namespace tagdb
