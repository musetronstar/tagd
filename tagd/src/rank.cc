#include <iostream>
#include <sstream>
#include <cassert>

#include "tagd/config.h"
#include "tagd/rank.h"

namespace tagd {

rank::rank(const rank& rhs) : _data(NULL), _size(0) {
    if (rhs._data != NULL && rhs._size != 0)
        this->alloc_copy(rhs._data, rhs._size);
}

rank& rank::operator=(const rank& rhs) {
    if (_data != NULL)
        delete [] _data;

    this->alloc_copy(rhs._data, rhs._size);

    return *this;
}

bool rank::operator<(const rank& rhs) const {
    if (rhs._data == NULL)
            return false;

    if (_data == NULL)
            return true;

    const int c = memcmp(_data, rhs._data, _size);
    if ( c < 0 || (c == 0 && _size < rhs._size))
        return true;
    else
        return false;
}

bool rank::operator==(const rank& rhs) const {
    if (_data == NULL) {
        if (rhs._data == NULL)
            return true;
        else
            return false;
    }

    if (rhs._data == NULL) {
        if (_data == NULL)
            return true;
        else
            return false;
    }

    if (_size != rhs._size)
        return false;

    return ( memcmp(_data, rhs._data, _size) == 0 );
}

bool rank::contains(const rank& other) const {
    if (_data == NULL || other._data == NULL)
		return false;

    if (_size > other._size)
        return false;

    return ( memcmp(_data, other._data, _size) == 0 );
}

inline void rank::copy(const byte_t *bytes, const size_t sz) {
    std::memcpy(_data, bytes, sz);
    _data[sz] = '\0';
}

inline void rank::alloc_copy(const byte_t *bytes, const size_t sz, const size_t padding) {
    _alloc_size = sz + 1 + padding;
    _data = new byte_t[_alloc_size];
    this->copy(bytes, sz);
    _size = sz;
}

rank::~rank() {
    if (_data != NULL)
        delete [] _data;
}

void rank::clear() {
    if (_data != NULL)
        delete [] _data;

	_data = NULL;
	_size = 0;
}

rank_code rank::validate(const byte_t *bytes, size_t *sz) {
    *sz = 0;

    while (bytes[*sz] != '\0' && *sz < RANK_MAX_LEN) {
        if (bytes[(*sz)++] > RANK_MAX_BYTE)
            return RANK_MAX_BYTE;
    }

    if (*sz == 0) return RANK_EMPTY;

    return (*sz == RANK_MAX_LEN ? RANK_MAX_LEN : RANK_OK);
}

rank_code rank::init(const byte_t *bytes) {
    if (bytes == NULL) return RANK_EMPTY;

    size_t sz;
    rank_code r = validate(bytes, &sz);

    if (r != RANK_OK)
        return r;

    if (_data == NULL) {
        this->alloc_copy(bytes, sz);
    } else {
        if (sz < _alloc_size) { // reuse buffer
            this->copy(bytes, sz);
            _size = sz;
        } else {
            delete [] _data;
            this->alloc_copy(bytes, sz);
        }
    }

    return RANK_OK;
}

std::string rank::dotted_str() const {
    if (_size == 0) return std::string();

    return rank::dotted_str(_data);
}

std::string rank::dotted_str(const byte_t *s) {
    if (s == NULL) return std::string();

    std::stringstream ss;
    ss << static_cast<unsigned int>(s[0]);
    for (size_t i = 1; s[i]!='\0' && i<RANK_MAX_LEN; ++i) {
        ss << '.' << static_cast<unsigned int>(s[i]);
    }

    return ss.str();
}

byte_t rank::last() const {
    if (_size == 0)
        return '\0';

    return _data[_size-1];
}

byte_t rank::pop() {
    if (_size == 0)
        return '\0';

    _size--;
    byte_t b = _data[_size];
    _data[_size] = '\0';

    return b;    
}

rank_code rank::push(const byte_t b) {
    if (b == '\0') return RANK_EMPTY;
    if (b > RANK_MAX_BYTE) return RANK_MAX_BYTE;
    if (_size == RANK_MAX_LEN) return RANK_MAX_LEN;

    if (_data == NULL) {
        byte_t a[] = {b, '\0'};
        return this->init(a);
    }

    if ((_size+1) == _alloc_size) {
        byte_t *tmp = _data;
        this->alloc_copy(tmp, _size, 2);
        // _data, and _alloc_size are now different
        delete [] tmp;
    }

    assert( _data[_size] == '\0' );

    _data[_size++] = b;
    _data[_size] = '\0';

    return RANK_OK;
}

rank_code rank::increment() {
    if (_size == 0)
        return RANK_EMPTY;

    if (_data[_size-1] >= RANK_MAX_BYTE)
        return RANK_MAX_BYTE;

    _data[_size-1]++;

    return RANK_OK;
}

rank_code rank::next(rank& next, const rank_set& R) {
    rank_set::const_iterator it = R.begin();
    if (it == R.end()) return RANK_EMPTY;

    if (it->last() != 1) {
        // first slot not taken, use the lowest rank
        // by replacing last byte with 1
        next = *it;
        next.pop();
        next.push(1);
        return RANK_OK;
    }

    // all elements must conform to size of the first element
    size_t sz = it->size();

    rank_set::const_iterator prev = it;
    ++it;

    /* check for holes in the last byte of each rank element
       e.g. 1.2.1 , 1.2.2 , 1.2.4
                          ^ hole 1.2.3
    */
    while (it != R.end()) {
        if (it->size() != sz) {
            std::cerr << "mismatched sizes in rank set" << std::endl;
            return RANK_ERR;
        }

        if (prev->_data == NULL || it->_data == NULL) {
            std::cerr << "NULL data in rank set" << std::endl;
            return RANK_ERR;
        }

        // compares 1.2 in the example above
        if (memcmp(prev->_data, it->_data, (prev->_size-1)) != 0) {
            std::cerr << "mismatched branches in rank set" << std::endl;
            return RANK_ERR;
        }

        int diff = it->_data[sz-1] - prev->_data[sz-1];
        if (!(diff > 0)) {
            std::cerr << "unordered rank set" << std::endl;
            return RANK_ERR;
        }

        // hole in rank elements
        if (diff > 1)
            break;

        prev = it;
        ++it;
    }

    // prev is either pointing to the last element (not end())
    // or the element before a hole
    next = *prev;

    return next.increment();
}

const char* rank_code_str(const rank_code r) {
    const char *s;
    switch(r) {
        case RANK_OK:
            s = "RANK_OK";
            break;
        case RANK_ERR:
            s = "RANK_ERR";
            break;
        case RANK_EMPTY:
            s = "RANK_ERR";
            break;
        case RANK_MAX_BYTE:
            s = "RANK_MAX_BYTE";
            break;
        case RANK_MAX_LEN:
            s = "RANK_MAX_LEN";
            break;
        default:
            s = "RANK_UNKNOWN";
    }

    return s;
}

void print_rank_set(const tagd::rank_set& R) {
    for (tagd::rank_set::const_iterator it = R.begin(); it!=R.end(); ++it) {
        std::cout << it->dotted_str() << std::endl;;
    }
}

} // namespace tagd

