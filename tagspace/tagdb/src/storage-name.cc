#include "tagdb/storage.h"
#include <cstdint>

namespace tagdb {

namespace unicode {
constexpr uint32_t space = 0x20;
constexpr uint32_t next_line = 0x85;
constexpr uint32_t no_break_space = 0xa0;
constexpr uint32_t ogham_space_mark = 0x1680;
constexpr uint32_t en_quad = 0x2000;
constexpr uint32_t hair_space = 0x200a;
constexpr uint32_t line_separator = 0x2028;
constexpr uint32_t paragraph_separator = 0x2029;
constexpr uint32_t narrow_no_break_space = 0x202f;
constexpr uint32_t medium_mathematical_space = 0x205f;
constexpr uint32_t ideographic_space = 0x3000;
constexpr uint32_t delete_control = 0x7f;
constexpr uint32_t last_c1_control = 0x9f;
constexpr uint32_t first_surrogate = 0xd800;
constexpr uint32_t last_surrogate = 0xdfff;
constexpr uint32_t max_scalar = 0x10ffff;
}

namespace utf8 {
constexpr uint32_t first_non_ascii = 0x80;
constexpr uint32_t two_byte_lead_min = 0xc2;
constexpr uint32_t two_byte_lead_max = 0xdf;
constexpr uint32_t three_byte_lead_min = 0xe0;
constexpr uint32_t three_byte_lead_max = 0xef;
constexpr uint32_t four_byte_lead_min = 0xf0;
constexpr uint32_t four_byte_lead_max = 0xf4;
constexpr uint32_t two_byte_payload_mask = 0x1f;
constexpr uint32_t three_byte_payload_mask = 0x0f;
constexpr uint32_t four_byte_payload_mask = 0x07;
constexpr uint32_t two_byte_min_scalar = 0x80;
constexpr uint32_t three_byte_min_scalar = 0x800;
constexpr uint32_t four_byte_min_scalar = 0x10000;
constexpr uint32_t continuation_marker_mask = 0xc0;
constexpr uint32_t continuation_marker = 0x80;
constexpr uint32_t continuation_payload_mask = 0x3f;
constexpr unsigned continuation_payload_bits = 6;
}

bool unicode_space(uint32_t cp) {
	return cp == unicode::space || cp == unicode::next_line ||
		   cp == unicode::no_break_space || cp == unicode::ogham_space_mark ||
		   (cp >= unicode::en_quad && cp <= unicode::hair_space) ||
		   cp == unicode::line_separator ||
		   cp == unicode::paragraph_separator ||
		   cp == unicode::narrow_no_break_space ||
		   cp == unicode::medium_mathematical_space ||
		   cp == unicode::ideographic_space;
}

bool valid_storage_name(const std::string& name) {
	/*
	 * A tagspace name becomes one directory name and one filename.
	 * Keep its original UTF-8 bytes, but reject path separators, controls,
	 * and leading or trailing whitespace before constructing either path.
	 */
	if (name.empty() || name == "." || name == "..")
		return false;

	for (size_t i = 0; i < name.size();) {
		const size_t start = i;
		uint32_t cp = static_cast<unsigned char>(name[i++]);
		unsigned extra = 0;
		uint32_t minimum = 0;

		if (cp >= utf8::two_byte_lead_min && cp <= utf8::two_byte_lead_max) {
			cp &= utf8::two_byte_payload_mask;
			extra = 1;
			minimum = utf8::two_byte_min_scalar;
		} else if (cp >= utf8::three_byte_lead_min &&
				   cp <= utf8::three_byte_lead_max) {
			cp &= utf8::three_byte_payload_mask;
			extra = 2;
			minimum = utf8::three_byte_min_scalar;
		} else if (cp >= utf8::four_byte_lead_min &&
				   cp <= utf8::four_byte_lead_max) {
			cp &= utf8::four_byte_payload_mask;
			extra = 3;
			minimum = utf8::four_byte_min_scalar;
		} else if (cp >= utf8::first_non_ascii) {
			return false;
		}

		if (extra > name.size() - i)
			return false;
		while (extra--) {
			unsigned char next = name[i++];
			if ((next & utf8::continuation_marker_mask) !=
				utf8::continuation_marker)
				return false;
			cp = (cp << utf8::continuation_payload_bits) |
				 (next & utf8::continuation_payload_mask);
		}

		/* Filenames need valid Unicode; the rank codec permits larger ordinals. */
		if (cp < minimum || cp > unicode::max_scalar ||
			(cp >= unicode::first_surrogate && cp <= unicode::last_surrogate))
			return false;
		if (cp < unicode::space ||
			(cp >= unicode::delete_control && cp <= unicode::last_c1_control) ||
			cp == '/' || cp == '\\')
			return false;
		if ((start == 0 || i == name.size()) && unicode_space(cp))
			return false;
	}
	return true;
}

} // namespace tagdb
