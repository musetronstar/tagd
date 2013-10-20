#pragma once

#include <cstring>  /* memcmp, size_t ... */
#include <string>
#include <set>
#include "codes.h"

namespace tagd {

class rank;
typedef std::set<rank> rank_set;

void print_rank_set(const tagd::rank_set&);

// encapsulates bytes used to order the tag heirachy
class rank {
    private:
		std::string _data;

    public:
        rank() : _data() {}
        rank(const rank& cp) : _data(cp._data) {}
        // don't allow iniatialization by constructors (other than copy),
        // because we must validate data either by return code
        // or exception - I'm not about to use exceptions
        ~rank() {}
		void clear() { _data.clear(); }

        rank& operator=(const rank& rhs) { _data = rhs._data; return *this; }
        bool operator<(const rank& rhs) const { return _data < rhs._data; }
        bool operator==(const rank& rhs) const { return _data == rhs._data; }
        bool operator!=(const rank& rhs) const { return !(*this == rhs); }

		// this rank contains the rank being compared,
		// in other words, this rank is a prefix to the other 
        bool contains(const rank&) const;

        // validates bytes, sets size pointed to
        // byte string must be '\0' terminated
		tagd::code validate(const char*, size_t*);

        // validates bytes, allocs and copies bytes into memory
        // already alloced memory will be reused if possible
		tagd::code init(const char*);
 
        const char* c_str() const { return _data.c_str(); }
        std::string dotted_str() const;

        // copy of last byte (NULL if empty)
        char back() const;

        // pop the last byte off (NULL if empty)
        char pop_back();

        // push a byte on (default 1)
		tagd::code push_back(const char=1);

        // increment the last byte, returns TAGD_OK or RANK_MAX_BYTE
		tagd::code increment();

        size_t size() const { return _data.size(); }

        bool empty() const { return _data.empty(); }

        // next rank in a rank set - fills holes
        static tagd::code next(rank&, const rank_set&);
        static std::string dotted_str(const char*);
};

} // namespace tagd
