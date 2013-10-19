#pragma once

#include <cstring>  /* memcmp, size_t ... */
#include <string>
#include <set>
#include "codes.h"

namespace tagd {

class rank;
typedef unsigned char byte_t;
typedef std::set<rank> rank_set;

// holds the value of a multibyte rank node representing each level
// ex: 1.2.3
// 1, 2, and 3 would each be represented by a node_value_t
typedef unsigned long node_value_t;

void print_rank_set(const tagd::rank_set&);

// encapsulates bytes used to order the tag heirachy
class rank {
    private:
        byte_t *_data;
        size_t _size; // size of byte string in _data
        size_t _alloc_size; // size of _data buffer (gt _size for '\0' terminator)

        // padding to allow room for pushing more bytes without allocing
        void alloc_copy(const byte_t *, const size_t, const size_t=1); // data, data_size, padding
        void copy(const byte_t *, const size_t); // copy bytes+'\0' into _data

    public:
        rank() : _data(NULL), _size(0) {};
        rank(const rank&);
        // don't allow iniatialization by constructors (other than copy),
        // because we must validate data either by return code
        // or exception - I'm not about to use exceptions
        ~rank();
		void clear();

        rank& operator=(const rank&);
        bool operator<(const rank&) const;
        bool operator==(const rank&) const;
        inline bool operator!=(const rank& rhs) const { return !(*this == rhs); }

		// this rank contains the rank being compared,
		// in other words, this rank is a prefix to the other 
        bool contains(const rank&) const;

        // validates bytes, sets size pointed to
        // byte string must be '\0' terminated
		tagd::code validate(const byte_t*, size_t*);

        // validates bytes, allocs and copies bytes into memory
        // already alloced memory will be reused if possible
		tagd::code init(const byte_t *);
 
        const char* c_str() const { return reinterpret_cast<const char*>(_data); }
        const byte_t* uc_str() const { return _data; }
        std::string dotted_str() const;

        // copy of last byte (NULL if empty)
        byte_t last() const;

        // pop the last byte off (NULL if empty)
        byte_t pop();

        // push a byte on (default 1)
		tagd::code push(const byte_t=1);

        // increment the last byte, returns TAGD_OK or RANK_MAX_BYTE
		tagd::code increment();

        size_t size() const { return _size; }

        bool empty() const { return _size == 0; }

        // next rank in a rank set - fills holes
        static tagd::code next(rank&, const rank_set&);
        static std::string dotted_str(const byte_t*);
};

} // namespace tagd
