#pragma once

// #include <cstdint>  // uint32_t defined C++11 only
#include <stdint.h>
#include <string>

namespace tagd {

// I looped from 1 to std::numeric_limits<uint32_t>::max()
// using utf8_increment(), testing each code point with utf8_append(),
// discarding return values of 0xFFFD and came up with the following:
//
//   uint32_t max: 4294967295
//   code_points available: 4290504830
//   not ok: 4462464
//   max code_point: 2097151
const uint32_t UTF8_MAX_CODE_POINT = 2097151;

// append code point as utf8 to string
// and return number of bytes appended
size_t utf8_append(std::string&, uint32_t);

// reads one code point from utf8 from input string,
// advances to the position past the last byte byte sequence,
// and returns the code point
uint32_t utf8_read(const std::string&, size_t*);

// returns the position of the start of the last byte sequence
// moving backwards from pos (or the end of the string if pos not given)
// returns std::string::npos if not found
size_t utf8_pos_back(const std::string&, size_t pos=std::string::npos);

// increments and return code point 
// returns 0xFFFD if cannot be incremented
uint32_t utf8_increment(uint32_t);

// returns whether a code point is valid for utf8 encoding
bool utf8_is_valid(uint32_t);

} // namespace tagd

