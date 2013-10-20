#include <iostream>
#include <sstream>
#include <cassert>

#include "tagd/config.h"
#include "tagd/rank.h"

namespace tagd {

bool rank::contains(const rank& other) const {
    if (_data.empty() || other._data.empty())
		return false;

    if (_data.size() > other._data.size())
        return false;

    return ( _data.compare(0, _data.size(), other._data.substr(0, _data.size())) == 0 );
}

tagd::code rank::validate(const char *bytes, size_t *sz) {
    *sz = 0;

    while (bytes[*sz] != '\0' && *sz < RANK_MAX_LEN) {
        if ((unsigned char)bytes[(*sz)++] > RANK_MAX_BYTE)
            return RANK_MAX_BYTE;
    }

    if (*sz == 0) return RANK_EMPTY;

    return (*sz == RANK_MAX_LEN ? RANK_MAX_LEN : TAGD_OK);
}

tagd::code rank::init(const char *bytes) {
    if (bytes == NULL) return RANK_EMPTY;

    size_t sz;
	tagd::code tc = validate(bytes, &sz);

    if (tc != TAGD_OK)
        return tc;

	_data.assign(bytes, sz);

    return TAGD_OK;
}

std::string rank::dotted_str() const {
    if (_data.empty()) return std::string();

    return rank::dotted_str(_data.c_str());
}

std::string rank::dotted_str(const char *s) {
    if (s == NULL) return std::string();

    std::stringstream ss;
    ss << static_cast<unsigned int>(s[0]);
    for (size_t i = 1; s[i]!='\0' && i<RANK_MAX_LEN; ++i) {
        ss << '.' << static_cast<unsigned int>(s[i]);
    }

    return ss.str();
}

char rank::back() const {
    if (_data.empty())
        return '\0';

    return _data[_data.size()-1];
}

char rank::pop_back() {
    if (_data.empty())
        return '\0';

	char c = _data[_data.size()-1];
	_data.erase(_data.size()-1);

    return c;    
}

tagd::code rank::push_back(const char b) {
    if (b == '\0') return RANK_EMPTY;
    if ((unsigned char)b > RANK_MAX_BYTE) return RANK_MAX_BYTE;
    if (_data.size() == RANK_MAX_LEN) return RANK_MAX_LEN;

	_data.push_back(b);

    return TAGD_OK;
}

tagd::code rank::increment() {
    if (_data.empty())
        return this->push_back();

    if ((unsigned char)_data[_data.size()-1] >= RANK_MAX_BYTE)
        return RANK_MAX_BYTE;

    _data[_data.size()-1]++;

    return TAGD_OK;
}

tagd::code rank::next(rank& next, const rank_set& R) {
    rank_set::const_iterator it = R.begin();
    if (it == R.end()) return RANK_EMPTY;

    if ((unsigned char)it->back() != 1) {
        // first slot not taken, use the lowest rank
        // by replacing last byte with 1
        next = *it;
        next.pop_back();
        next.push_back(1);
        return TAGD_OK;
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

        if (prev->_data.empty() || it->_data.empty()) {
            std::cerr << "empty data in rank set" << std::endl;
            return RANK_ERR;
        }

        // compares 1.2 in the example above
		if (prev->_data.size() > 1) {
			if (prev->_data.compare(0, (prev->_data.size()-1), it->_data.substr(0,(it->_data.size()-1))) != 0) {
				std::cerr << "mismatched branches in rank set" << std::endl;
				return RANK_ERR;
			}
		}

        int diff = (unsigned char)it->_data[sz-1] - (unsigned char)prev->_data[sz-1];
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

void print_rank_set(const tagd::rank_set& R) {
    for (tagd::rank_set::const_iterator it = R.begin(); it!=R.end(); ++it) {
        std::cout << it->dotted_str() << std::endl;;
    }
}

} // namespace tagd

