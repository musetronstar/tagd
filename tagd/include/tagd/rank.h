#pragma once

#include <string>
#include <set>
#include <algorithm>  // reverse_copy
#include "codes.h"

namespace tagd {

class rank;
typedef std::set<rank> rank_set;

void print_rank_set(const tagd::rank_set&);

// encapsulates bytes used to order the tag heirachy
class rank {
    private:
		/*\
		|*|  A rank string is '\0' terminated utf8 bytes, where each character (code point)
		|*|  represents a level in the rank heirarchy.
		|*|  A rank string is the ordinal value of each node in the tagspace tree
		|*|  A code point at a particular level is known as the rank value
		|*|   ex: "\x01\x01\x02"
		|*|   the value at level 3 is 0x02
		|*|  Level 0 (rank == NULL) is reserved for _entity
		|*|  utf8 was chosen because:
		|*|   - each level can store more values vs one raw byte (63485 vs 255)
		|*|   - it collates lexicographically
		|*|   - all children of a particular node in the rank heirarchy will
		|*|     be prefixed by the parent node - that is,
		|*|     rank substrings define subtrees in the heirarchy
		|*|   - as a means to encode numbers as multibyte strings,
		|*|     it was already figured out for us (thanks Dave Prosser and Ken Thompson)
		|*|
		|*|  Caveat: The 0xFFFD byte sequence is used as a replacement for invalid uft8 sequences.
		|*|          Our utf8 functions use it to indicate an error state, so it must not be
		|*|          used in valid ranks - it will fail to validate.
		\*/
		std::string _data;

        // validates bytes
		tagd::code validate(const std::string&);

    public:
        rank() : _data() {}
        rank(const rank& cp) : _data(cp._data) {}

		// unpacks the 8 bytes of a 64 bit number into the string passed in
		// returns the number of non-zero bytes unpacked
		static size_t unpack(std::string& s, uint64_t packed) {
			if (packed == 0) return 0;

			char unpacked[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			size_t i = 7, sz = 0;
			do {
				char b = (packed & 0xff);
				if (b) {
					unpacked[i] = b;
					sz++;
				}
				packed >>= 8;
			} while (i-- > 0);

			if (sz != 0)
				s = std::string(unpacked, sz);

			return sz;
		}

		rank(uint64_t packed) {  // not validated - make sure your bytes are valid!
			unpack(_data, packed);
		}
		// otherwise, don't allow iniatialization by constructors,
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

        // validates bytes, and copies to _data
		tagd::code init(const char*);
		tagd::code init(uint64_t);  // packed bytes
 
        const char* c_str() const { return _data.c_str(); }
        std::string dotted_str() const;

        // copy of last code point (0 if empty)
        uint32_t back() const;

        // pop the last code point (0 if empty)
        uint32_t pop_back();

        // push a code point
		tagd::code push_back(uint32_t);

        // increment the last byte, returns TAGD_OK or RANK_MAX_VALUE
		tagd::code increment();

        size_t size() const { return _data.size(); }

        bool empty() const { return _data.empty(); }

        // next rank in a rank set - fills holes
        static tagd::code next(rank&, const rank_set&);
        static std::string dotted_str(const char*);
};

} // namespace tagd
