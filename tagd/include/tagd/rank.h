#pragma once

#include <cstring>  /* memcmp, size_t ... */
#include <set>

namespace tagd {

class rank;
typedef unsigned char byte_t;
typedef std::set<rank> rank_set;
typedef std::pair<rank_set::iterator, bool> rank_pair;

typedef enum {
    RANK_OK,
    RANK_ERR,
    RANK_EMPTY,
    // 255 may be reserved for variable length rank strings in the future,
    // so that the number of elements can be greater than UCHAR_MAX (our value type)
    RANK_MAX_BYTE = 254,  // max value per inividual byte
    RANK_MAX_LEN = 255 // max bytes in _data string
} rank_code;

// changes to rank_code enum need to be updated in this function
const char* rank_code_str(const rank_code);
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

        // validates bytes, sets size pointed to
        // byte string must be '\0' terminated
        rank_code validate(const byte_t*, size_t*);

        // validates bytes, allocs and copies bytes into memory
        // already alloced memory will be reused if possible
        rank_code init(const byte_t *);
 
        const char* c_str() const { return reinterpret_cast<const char*>(_data); }
        const byte_t* uc_str() const { return _data; }
        std::string dotted_str() const;

        // copy of last byte (NULL if empty)
        byte_t last() const;

        // pop the last byte off (NULL if empty)
        byte_t pop();

        // push a byte on (default 1)
        rank_code push(const byte_t=1);

        // increment the last byte, returns RANK_OK or RANK_MAX_BYTE
        rank_code increment();

        size_t size() const { return _size; }

        bool empty() const { return _size == 0; }

        // next rank in a rank set - fills holes
        static rank_code next(rank&, const rank_set&);
        static std::string dotted_str(const byte_t*);
};

} // namespace tagd
