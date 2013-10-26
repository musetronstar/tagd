#include <iostream>
#include <sstream>
#include <cassert>

#include "tagd/config.h"
#include "tagd/rank.h"
#include "tagd/utf8.h"

namespace tagd {

bool rank::contains(const rank& other) const {
    if (_data.empty() || other._data.empty())
		return false;

    if (_data.size() > other._data.size())
        return false;

    return ( _data.compare(0, _data.size(), other._data.substr(0, _data.size())) == 0 );
}

tagd::code rank::validate(const std::string& bytes) {
	if (bytes.size() > RANK_MAX_LEN)
		return RANK_MAX_LEN;

	size_t pos = 0;
    while (pos < bytes.size()) {
		// whether 0xFFFD was part of the byte string,
		// or replaced - either way, it's invalid
        if (utf8_read(bytes, &pos) == 0xFFFD)
			return RANK_ERR;
    }

    if (pos == 0) return RANK_EMPTY;

    return TAGD_OK;
}

// take in raw bytes instead of string because we want to handle NULL
// without throwing a std::string exception
tagd::code rank::init(const char *bytes) {
    if (bytes == NULL) return RANK_EMPTY;

	std::string tmp(bytes);
	tagd::code tc = validate(tmp);

    if (tc != TAGD_OK)
        return tc;

	_data = tmp;

    return TAGD_OK;
}

std::string rank::dotted_str() const {
    if (_data.empty()) return std::string();

    return rank::dotted_str(_data.c_str());
}

std::string rank::dotted_str(const char *s) {
    if (s == NULL) return std::string();

	// if cp == 0xFFFD, use it (�) as the utf8 replacement character
    std::stringstream ss;
	std::string data(s);
	size_t pos = 0;
	uint32_t cp = utf8_read(data, &pos);
    ss << (cp == 0xFFFD ? (char)cp : cp);
	while (pos < data.size()) {
		cp = utf8_read(data, &pos);
		ss << '.' << (cp == 0xFFFD ? (char)cp : cp);
    }

    return ss.str();
}

uint32_t rank::back() const {
    if (_data.empty())
        return 0;

	size_t pos = utf8_pos_back(_data);
	if (pos == std::string::npos) // not found (malformed)
		return 0xFFFD;

	return utf8_read(_data, &pos);
}

uint32_t rank::pop_back() {
    if (_data.empty())
        return 0;

	size_t pos = utf8_pos_back(_data);
	if (pos == std::string::npos) // not found (malformed)
		return 0xFFFD;

	size_t tmp = pos; 
	uint32_t cp = utf8_read(_data, &tmp);
	_data.erase(pos);

    return cp;    
}

tagd::code rank::push_back(uint32_t cp) {
    if (cp == 0) return RANK_EMPTY;
	if (cp > UTF8_MAX_CODE_POINT) return RANK_MAX_VALUE;
    if (!utf8_is_valid(cp)) return RANK_ERR;

	std::string utf8;
	size_t sz = utf8_append(utf8, cp);
	if ((_data.size() + sz) > RANK_MAX_LEN) return RANK_MAX_LEN;

	_data.append(utf8);

    return TAGD_OK;
}

tagd::code rank::increment() {
    if (_data.empty())
        return this->push_back(1);

	size_t pos = utf8_pos_back(_data);
	if (pos == std::string::npos) // not found (malformed)
		return RANK_ERR;

	size_t tmp = pos;
	uint32_t cp = utf8_read(_data, &tmp);
	if (cp == 0xFFFD) return RANK_ERR;  // malformed data
	if (cp > UTF8_MAX_CODE_POINT) return RANK_MAX_VALUE;

	cp = utf8_increment(cp);
	// returns replacement if no room for value
	if (cp == 0xFFFD) return RANK_MAX_VALUE;

	std::string utf8;
	size_t sz = utf8_append(utf8, cp);
	if ((pos + sz) > RANK_MAX_LEN) return RANK_MAX_LEN;

	_data.erase(pos).append(utf8);

    return TAGD_OK;
}

tagd::code rank::next(rank& next, const rank_set& R) {
    rank_set::const_iterator it = R.begin();
    if (it == R.end()) return RANK_EMPTY;
	if (it->_data.empty()) {
		std::cerr << "empty data in rank set" << std::endl;
		return RANK_EMPTY;
	}
	
	uint32_t back = it->back();

	// next rank cannot fit in leading utf8 byte
	// so increment from the last rank in the set
	if (back > (0x80 - 2)) {
		it = R.end();
		--it;
		if (it->_data.empty()) {
			std::cerr << "empty data in rank set" << std::endl;
			return RANK_EMPTY;
		}
		next = *it;
		return next.increment();
	}

	// rank can fit in leading utf8 byte, so fill holes if there are any
	
    if (back != 1) {
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
            return RANK_EMPTY;
        }

        // compares 1.2 in the example above
		if (prev->_data.size() > 1) {
			if (prev->_data.compare(0, (prev->_data.size()-1), it->_data.substr(0,(it->_data.size()-1))) != 0) {
				std::cerr << "mismatched branches in rank set" << std::endl;
				return RANK_ERR;
			}
		}

        int diff = (unsigned char)it->_data[sz-1] - (unsigned char)prev->_data[sz-1];
        if (diff <= 0) {
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

