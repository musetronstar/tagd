/* C++ code produced by gperf version 3.0.3 */
/* Command-line: gperf hard-tags.gperf  */
/* Computed positions: -k'2,4' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "hard-tags.gperf"

#include "tagd.h"
#line 12 "hard-tags.gperf"
struct hard_tag_hash_value {
    const char *key;
    const char *super;
    tagd::part_of_speech pos;
	uint32_t rank;
};

#define TOTAL_KEYWORDS 33
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 18
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 50
/* maximum key range = 47, duplicates = 0 */

class hard_tag_hash
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static struct hard_tag_hash_value *lookup (const char *str, unsigned int len);
};

inline unsigned int
hard_tag_hash::hash (register const char *str, register unsigned int len)
{
  static unsigned char asso_values[] =
    {
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 40, 51, 25, 25, 25,
       5, 25, 15, 51,  5,  5, 51,  0,  0,  5,
      15, 51,  0,  5,  0,  5, 15,  0, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51
    };
  return len + asso_values[(unsigned char)str[3]] + asso_values[(unsigned char)str[1]];
}

struct hard_tag_hash_value *
hard_tag_hash::lookup (register const char *str, register unsigned int len)
{
  static struct hard_tag_hash_value wordlist[] =
    {
      {""}, {""}, {""}, {""},
#line 39 "hard-tags.gperf"
      {"_url",  HARD_TAG_ENTITY, tagd::POS_TAG, 20},
#line 48 "hard-tags.gperf"
      {"_port",  HARD_TAG_URL_PART, tagd::POS_TAG, 29},
      {""}, {""},
#line 23 "hard-tags.gperf"
      {"_relator",  HARD_TAG_ENTITY, tagd::POS_RELATOR, 4},
#line 40 "hard-tags.gperf"
      {"_url_part",  HARD_TAG_ENTITY, tagd::POS_TAG, 21},
#line 50 "hard-tags.gperf"
      {"_pass",  HARD_TAG_URL_PART, tagd::POS_TAG, 31},
#line 20 "hard-tags.gperf"
      {"_super",  HARD_TAG_ENTITY, tagd::POS_SUPER_RELATOR, 1},
#line 36 "hard-tags.gperf"
      {"_unknown_tag",  HARD_TAG_ENTITY, tagd::POS_TAG, 17},
#line 42 "hard-tags.gperf"
      {"_private",  HARD_TAG_URL_PART, tagd::POS_TAG, 23},
#line 24 "hard-tags.gperf"
      {"_has",  HARD_TAG_RELATOR, tagd::POS_RELATOR, 5},
#line 41 "hard-tags.gperf"
      {"_host",  HARD_TAG_URL_PART, tagd::POS_TAG, 22},
      {""},
#line 51 "hard-tags.gperf"
      {"_scheme",  HARD_TAG_URL_PART, tagd::POS_TAG, 32},
#line 34 "hard-tags.gperf"
      {"_message",  HARD_TAG_ENTITY, tagd::POS_TAG, 15},
      {""},
#line 45 "hard-tags.gperf"
      {"_path",  HARD_TAG_URL_PART, tagd::POS_TAG, 26},
      {""},
#line 28 "hard-tags.gperf"
      {"_refers",  HARD_TAG_RELATOR, tagd::POS_REFERS, 9},
#line 22 "hard-tags.gperf"
      {"_type_of",  HARD_TAG_SUPER, tagd::POS_SUPER_RELATOR, 3},
#line 27 "hard-tags.gperf"
      {"_referent",  HARD_TAG_SUPER, tagd::POS_REFERENT, 8},
#line 29 "hard-tags.gperf"
      {"_refers_to",  HARD_TAG_RELATOR, tagd::POS_REFERS_TO, 10},
      {""},
#line 38 "hard-tags.gperf"
      {"_line_number",  HARD_TAG_ENTITY, tagd::POS_TAG, 19},
      {""},
#line 43 "hard-tags.gperf"
      {"_pub",  HARD_TAG_URL_PART, tagd::POS_TAG, 24},
#line 49 "hard-tags.gperf"
      {"_user",  HARD_TAG_URL_PART, tagd::POS_TAG, 30},
#line 33 "hard-tags.gperf"
      {"_error",  HARD_TAG_ENTITY, tagd::POS_TAG, 14},
      {""},
#line 26 "hard-tags.gperf"
      {"_interrogator",  HARD_TAG_ENTITY, tagd::POS_INTERROGATOR, 7},
#line 44 "hard-tags.gperf"
      {"_sub",  HARD_TAG_URL_PART, tagd::POS_TAG, 25},
#line 35 "hard-tags.gperf"
      {"_caused_by",  HARD_TAG_RELATOR, tagd::POS_RELATOR, 16},
#line 46 "hard-tags.gperf"
      {"_query",  HARD_TAG_URL_PART, tagd::POS_TAG, 27},
      {""},
#line 32 "hard-tags.gperf"
      {"_ignore_duplicates",  HARD_TAG_FLAG, tagd::POS_FLAG, 13},
      {""},
#line 37 "hard-tags.gperf"
      {"_bad_token",  HARD_TAG_ENTITY, tagd::POS_TAG, 18},
      {""}, {""}, {""},
#line 25 "hard-tags.gperf"
      {"_can",  HARD_TAG_RELATOR, tagd::POS_RELATOR, 6},
#line 31 "hard-tags.gperf"
      {"_flag",  HARD_TAG_ENTITY, tagd::POS_FLAG, 12},
      {""},
#line 19 "hard-tags.gperf"
      {"_entity",  HARD_TAG_ENTITY, tagd::POS_TAG, 0},
#line 30 "hard-tags.gperf"
      {"_context",  HARD_TAG_RELATOR, tagd::POS_CONTEXT, 11},
#line 47 "hard-tags.gperf"
      {"_fragment",  HARD_TAG_URL_PART, tagd::POS_TAG, 28},
#line 21 "hard-tags.gperf"
      {"_is_a",  HARD_TAG_SUPER, tagd::POS_SUPER_RELATOR, 2}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].key;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
