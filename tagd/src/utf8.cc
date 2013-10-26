#include <cassert>
#include <limits>

#include "tagd/utf8.h"

namespace tagd {

/*
 * Parts of the following code was borrowed from utf.c
 * of the sqlite v3.7.17 distribution.
 *
 * Thank you Dr. Hipp!
 */

/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char sqlite3Utf8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

// modified from WRITE_UTF8 macro to append to string
size_t utf8_append(std::string &s, uint32_t c) {
	if (c == 0)
		return 0;

	char utf8[5] = {'\0','\0','\0','\0','\0'};
	size_t sz = 0;

	if ( c<0x00080 ) {
		utf8[sz++] = (c&0xFF);
	}
	else if ( c<0x00800 ) {
		utf8[sz++] = 0xC0 + ((c>>6)&0x1F);
		utf8[sz++] = 0x80 + (c & 0x3F);
	}
	else if ( c<0x10000 ) {
		utf8[sz++] = 0xE0 + ((c>>12)&0x0F);
		utf8[sz++] = 0x80 + ((c>>6) & 0x3F);
		utf8[sz++] = 0x80 + (c & 0x3F);
	} else {
		utf8[sz++] = 0xF0 + ((c>>18) & 0x07);
		utf8[sz++] = 0x80 + ((c>>12) & 0x3F);
		utf8[sz++] = 0x80 + ((c>>6) & 0x3F);
		utf8[sz++] = 0x80 + (c & 0x3F);
	}

	s.append(utf8);
	return sz;
}

// modified from sqlite3Utf8Read to deal with std::string
uint32_t utf8_read(const std::string &s, size_t *pos) {
	if (s.empty()) {
		*pos = 0;
		return 0;
	}

	unsigned int c = (unsigned char) s[(*pos)++];

	if ( c>=0xc0 ) {                           // 0xc0 == 11000000 - leading byte of multibyte sequence
		c = sqlite3Utf8Trans1[c-0xc0];         // value of leading byte
		while( (s[*pos] & 0xc0)==0x80 ) {      // 0x80 == 10000000 - continuation byte
			c = (c<<6) + (0x3f & s[(*pos)++]); // 0x3f == 00111111 - shift in value of each continuation byte
		}
		if( c<0x80                             // leading byte w/o continuation bytes
			|| (c&0xFFFFF800)==0xD800          // UTF16 surrogate
			|| (c&0xFFFFFFFE)==0xFFFE )        // 11111110 - leading byte w/ no space for value
		{  c = 0xFFFD; }                       // replacement character �
	}

	return c;
}

size_t utf8_pos_back(const std::string &s, size_t pos) {
	if (pos == std::string::npos)
		pos = s.size() - 1;

	for (; pos>=0; --pos) {
		if ( (unsigned char)s[pos] < 0x80     // regular ASCII
		  || (unsigned char)s[pos] >= 0xc0 )  // leading multibyte sequence
			return pos;
	}

	return std::string::npos;
}

uint32_t utf8_increment(uint32_t cp) {
	if (cp >= UTF8_MAX_CODE_POINT)
		return 0xFFFD;

	cp++;

	if ( cp == 0xFFFD )
		return (cp + 1);  // advance past replacement sequence

	if ( (cp>=0xD800 && cp<=0xDFFF) )
		return (0xDFFF + 1);  // advance past UTF16 surrogate

	if ( (cp&0xFFFFFFFE)==0xFFFE )
		return 0xFFFD;   // no room for value

	return cp;
}

bool utf8_is_valid(uint32_t cp) {
	return (
		cp <= UTF8_MAX_CODE_POINT &&
		cp != 0xFFFD &&             // replacement sequence
		!(cp >= 0xD800 && cp <= 0xDFFF) && // UTF16 surrogate
		(cp & 0xFFFFFFFE) != 0xFFFE // no room for value
	);
}

} // namespace tagd
