/* C++ code produced by gperf version 3.1 */
/* Command-line: gperf media-types.gperf  */
/* Computed positions: -k'1,6-7,10,13-14,16-18,23,36,$' */

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
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 7 "media-types.gperf"

#include <cstring>
#line 10 "media-types.gperf"
struct media_type_entry { const char* name; const char* extension; };

#define TOTAL_KEYWORDS 507
#define MIN_WORD_LENGTH 7
#define MAX_WORD_LENGTH 73
#define MIN_HASH_VALUE 93
#define MAX_HASH_VALUE 2659
/* maximum key range = 2567, duplicates = 0 */

class media_type_lookup
{
private:
  static inline unsigned int hash (const char *str, size_t len);
public:
  static const struct media_type_entry *find (const char *str, size_t len);
};

inline unsigned int
media_type_lookup::hash (const char *str, size_t len)
{
  static const unsigned short asso_values[] =
    {
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660,  375, 2660,   30,   60,   63,    0,    0,
        55,  195,    5,    0,  100,   20,   20,    5, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660,   20,  892,   45,
       365,    0,  205,  277,  501,  125,  185,  456,   35,  135,
       135,   25,    0,  330,   80,   15,   10,  400,  135,  992,
       672,  502,  220, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660, 2660,
      2660, 2660, 2660, 2660, 2660, 2660
    };
  unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[static_cast<unsigned char>(str[35])];
      /*FALLTHROUGH*/
      case 35:
      case 34:
      case 33:
      case 32:
      case 31:
      case 30:
      case 29:
      case 28:
      case 27:
      case 26:
      case 25:
      case 24:
      case 23:
        hval += asso_values[static_cast<unsigned char>(str[22])];
      /*FALLTHROUGH*/
      case 22:
      case 21:
      case 20:
      case 19:
      case 18:
        hval += asso_values[static_cast<unsigned char>(str[17])];
      /*FALLTHROUGH*/
      case 17:
        hval += asso_values[static_cast<unsigned char>(str[16])];
      /*FALLTHROUGH*/
      case 16:
        hval += asso_values[static_cast<unsigned char>(str[15])];
      /*FALLTHROUGH*/
      case 15:
      case 14:
        hval += asso_values[static_cast<unsigned char>(str[13])];
      /*FALLTHROUGH*/
      case 13:
        hval += asso_values[static_cast<unsigned char>(str[12])];
      /*FALLTHROUGH*/
      case 12:
      case 11:
      case 10:
        hval += asso_values[static_cast<unsigned char>(str[9])];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
        hval += asso_values[static_cast<unsigned char>(str[6])];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[static_cast<unsigned char>(str[5])];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[static_cast<unsigned char>(str[0])];
        break;
    }
  return hval + asso_values[static_cast<unsigned char>(str[len - 1])];
}

const struct media_type_entry *
media_type_lookup::find (const char *str, size_t len)
{
  static const struct media_type_entry wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""},
#line 478 "media-types.gperf"
      {"text/css", "css"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 479 "media-types.gperf"
      {"text/csv", "csv"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 414 "media-types.gperf"
      {"audio/mp4", "mp4a"},
      {""}, {""}, {""}, {""},
#line 70 "media-types.gperf"
      {"application/postscript", "ps"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 95 "media-types.gperf"
      {"application/srgs", "gram"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 485 "media-types.gperf"
      {"text/richtext", "rtx"},
      {""}, {""},
#line 48 "media-types.gperf"
      {"application/mp21", "mp21"},
      {""}, {""},
#line 53 "media-types.gperf"
      {"application/oebps-package+xml", "opf"},
#line 49 "media-types.gperf"
      {"application/mp4", "mp4s"},
      {""},
#line 25 "media-types.gperf"
      {"application/ecmascript", "ecma"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 85 "media-types.gperf"
      {"application/scvp-cv-response", "scs"},
      {""}, {""},
#line 427 "media-types.gperf"
      {"audio/vnd.rip", "rip"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 420 "media-types.gperf"
      {"audio/vnd.dts", "dts"},
      {""}, {""}, {""},
#line 483 "media-types.gperf"
      {"text/plain", "txt"},
      {""}, {""}, {""}, {""}, {""},
#line 419 "media-types.gperf"
      {"audio/vnd.dra", "dra"},
      {""},
#line 78 "media-types.gperf"
      {"application/rls-services+xml", "rs"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 504 "media-types.gperf"
      {"video/mp4", "mpg4"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 89 "media-types.gperf"
      {"application/set-payment-initiation", "setpay"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 467 "media-types.gperf"
      {"model/iges", "iges"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 39 "media-types.gperf"
      {"application/marc", "mrc"},
      {""}, {""}, {""}, {""}, {""},
#line 435 "media-types.gperf"
      {"image/cgm", "cgm"},
#line 87 "media-types.gperf"
      {"application/scvp-vp-response", "spp"},
      {""},
#line 481 "media-types.gperf"
      {"text/javascript", "mjs"},
      {""}, {""},
#line 477 "media-types.gperf"
      {"text/calendar", "ifb"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 416 "media-types.gperf"
      {"audio/ogg", "opus"},
      {""}, {""},
#line 503 "media-types.gperf"
      {"video/mj2", "mjp2"},
      {""},
#line 52 "media-types.gperf"
      {"application/octet-stream", "bin"},
      {""}, {""}, {""},
#line 501 "media-types.gperf"
      {"video/3gpp", "3gp"},
      {""}, {""}, {""}, {""},
#line 491 "media-types.gperf"
      {"text/vnd.curl", "curl"},
      {""}, {""}, {""}, {""},
#line 474 "media-types.gperf"
      {"model/vrml", "vrml"},
      {""}, {""},
#line 458 "media-types.gperf"
      {"image/vnd.fst", "fst"},
#line 444 "media-types.gperf"
      {"image/jxl", "jxl"},
      {""}, {""},
#line 82 "media-types.gperf"
      {"application/rtf", "rtf"},
      {""}, {""}, {""}, {""}, {""},
#line 514 "media-types.gperf"
      {"video/vnd.fvt", "fvt"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 14 "media-types.gperf"
      {"application/atomcat+xml", "atomcat"},
      {""}, {""}, {""}, {""},
#line 430 "media-types.gperf"
      {"font/ttf", "ttf"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""},
#line 429 "media-types.gperf"
      {"font/otf", "otf"},
      {""}, {""}, {""}, {""}, {""},
#line 502 "media-types.gperf"
      {"video/3gpp2", "3g2"},
      {""}, {""}, {""}, {""}, {""},
#line 81 "media-types.gperf"
      {"application/rpki-roa", "roa"},
#line 488 "media-types.gperf"
      {"text/turtle", "ttl"},
      {""}, {""}, {""},
#line 412 "media-types.gperf"
      {"application/zip", "zip"},
      {""}, {""}, {""},
#line 446 "media-types.gperf"
      {"image/png", "png"},
      {""},
#line 471 "media-types.gperf"
      {"model/vnd.gdl", "gdl"},
      {""}, {""},
#line 476 "media-types.gperf"
      {"text/cache-manifest", "appcache"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 165 "media-types.gperf"
      {"application/vnd.epson.salt", "slt"},
#line 29 "media-types.gperf"
      {"application/font-tdpfr", "pfr"},
#line 425 "media-types.gperf"
      {"audio/vnd.nuera.ecelp7470", "ecelp7470"},
      {""},
#line 341 "media-types.gperf"
      {"application/vnd.sema", "sema"},
      {""}, {""}, {""},
#line 90 "media-types.gperf"
      {"application/set-registration-initiation", "setreg"},
      {""}, {""}, {""},
#line 320 "media-types.gperf"
      {"application/vnd.osgi.dp", "dp"},
      {""},
#line 88 "media-types.gperf"
      {"application/sdp", "sdp"},
      {""}, {""},
#line 424 "media-types.gperf"
      {"audio/vnd.nuera.ecelp4800", "ecelp4800"},
#line 506 "media-types.gperf"
      {"video/ogg", "ogv"},
      {""}, {""}, {""}, {""}, {""},
#line 80 "media-types.gperf"
      {"application/rpki-manifest", "mft"},
      {""}, {""},
#line 15 "media-types.gperf"
      {"application/atomsvc+xml", "atomsvc"},
      {""},
#line 487 "media-types.gperf"
      {"text/troff", "ms"},
      {""}, {""},
#line 362 "media-types.gperf"
      {"application/vnd.tcpdump.pcap", "dmp"},
      {""}, {""}, {""},
#line 442 "media-types.gperf"
      {"image/ief", "ief"},
#line 109 "media-types.gperf"
      {"application/vnd.acucorp", "acutc"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 482 "media-types.gperf"
      {"text/n3", "n3"},
#line 167 "media-types.gperf"
      {"application/vnd.eszigno3+xml", "et3"},
#line 34 "media-types.gperf"
      {"application/java-archive", "jar"},
      {""}, {""}, {""},
#line 340 "media-types.gperf"
      {"application/vnd.seemail", "see"},
      {""},
#line 422 "media-types.gperf"
      {"audio/vnd.lucent.voice", "lvp"},
      {""},
#line 136 "media-types.gperf"
      {"application/vnd.commonspace", "csp"},
#line 366 "media-types.gperf"
      {"application/vnd.trueapp", "tra"},
#line 44 "media-types.gperf"
      {"application/mediaservercontrol+xml", "mscml"},
      {""}, {""}, {""},
#line 489 "media-types.gperf"
      {"text/uri-list", "urls"},
      {""}, {""}, {""},
#line 123 "media-types.gperf"
      {"application/vnd.astraea-software.iota", "iota"},
#line 351 "media-types.gperf"
      {"application/vnd.spotfire.dxp", "dxp"},
#line 364 "media-types.gperf"
      {"application/vnd.trid.tpt", "tpt"},
#line 480 "media-types.gperf"
      {"text/html", "htm"},
#line 302 "media-types.gperf"
      {"application/vnd.oasis.opendocument.presentation-template", "otp"},
#line 331 "media-types.gperf"
      {"application/vnd.proteus.magazine", "mgz"},
#line 307 "media-types.gperf"
      {"application/vnd.oasis.opendocument.text-template", "ott"},
#line 305 "media-types.gperf"
      {"application/vnd.oasis.opendocument.text", "odt"},
#line 145 "media-types.gperf"
      {"application/vnd.ctc-posml", "pml"},
      {""},
#line 245 "media-types.gperf"
      {"application/vnd.las.las+xml", "lasxml"},
#line 365 "media-types.gperf"
      {"application/vnd.triscape.mxs", "mxs"},
      {""},
#line 428 "media-types.gperf"
      {"font/collection", "ttc"},
      {""},
#line 132 "media-types.gperf"
      {"application/vnd.cloanto.rp9", "rp9"},
#line 352 "media-types.gperf"
      {"application/vnd.spotfire.sfs", "sfs"},
      {""},
#line 304 "media-types.gperf"
      {"application/vnd.oasis.opendocument.spreadsheet-template", "ots"},
#line 303 "media-types.gperf"
      {"application/vnd.oasis.opendocument.spreadsheet", "ods"},
      {""},
#line 160 "media-types.gperf"
      {"application/vnd.ecowin.chart", "mag"},
      {""}, {""}, {""}, {""},
#line 426 "media-types.gperf"
      {"audio/vnd.nuera.ecelp9600", "ecelp9600"},
#line 108 "media-types.gperf"
      {"application/vnd.acucobol", "acu"},
      {""}, {""},
#line 518 "media-types.gperf"
      {"video/vnd.vivo", "viv"},
      {""},
#line 133 "media-types.gperf"
      {"application/vnd.clonk.c4group", "c4u"},
      {""},
#line 35 "media-types.gperf"
      {"application/json", "json"},
      {""},
#line 107 "media-types.gperf"
      {"application/vnd.accpac.simply.imp", "imp"},
#line 60 "media-types.gperf"
      {"application/pkcs10", "p10"},
      {""},
#line 361 "media-types.gperf"
      {"application/vnd.tao.intent-module-archive", "tao"},
      {""}, {""},
#line 293 "media-types.gperf"
      {"application/vnd.oasis.opendocument.chart-template", "otc"},
#line 292 "media-types.gperf"
      {"application/vnd.oasis.opendocument.chart", "odc"},
#line 350 "media-types.gperf"
      {"application/vnd.solent.sdkm+xml", "sdkd"},
      {""}, {""},
#line 335 "media-types.gperf"
      {"application/vnd.recordare.musicxml", "mxl"},
#line 323 "media-types.gperf"
      {"application/vnd.pawaafile", "paw"},
#line 312 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.presentationml.slide", "sldx"},
#line 84 "media-types.gperf"
      {"application/scvp-cv-request", "scq"},
#line 316 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.spreadsheetml.template", "xltx"},
#line 314 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.presentationml.template", "potx"},
#line 322 "media-types.gperf"
      {"application/vnd.palm", "oprc"},
#line 318 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.wordprocessingml.template", "dotx"},
      {""},
#line 449 "media-types.gperf"
      {"image/tiff", "tif"},
      {""},
#line 315 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx"},
      {""},
#line 326 "media-types.gperf"
      {"application/vnd.picsel", "efif"},
#line 106 "media-types.gperf"
      {"application/vnd.accpac.simply.aso", "aso"},
#line 131 "media-types.gperf"
      {"application/vnd.claymore", "cla"},
      {""},
#line 317 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx"},
#line 271 "media-types.gperf"
      {"application/vnd.ms-ims", "ims"},
#line 433 "media-types.gperf"
      {"image/avif", "avif"},
#line 274 "media-types.gperf"
      {"application/vnd.ms-powerpoint", "pot"},
      {""}, {""}, {""},
#line 354 "media-types.gperf"
      {"application/vnd.stepmania.package", "smzip"},
      {""}, {""},
#line 252 "media-types.gperf"
      {"application/vnd.lotus-organizer", "org"},
#line 138 "media-types.gperf"
      {"application/vnd.cosmocaller", "cmc"},
#line 63 "media-types.gperf"
      {"application/pkcs8", "p8"},
#line 269 "media-types.gperf"
      {"application/vnd.ms-fontobject", "eot"},
      {""}, {""}, {""}, {""}, {""},
#line 355 "media-types.gperf"
      {"application/vnd.stepmania.stepchart", "sm"},
#line 306 "media-types.gperf"
      {"application/vnd.oasis.opendocument.text-master", "odm"},
#line 277 "media-types.gperf"
      {"application/vnd.ms-wpl", "wpl"},
      {""},
#line 268 "media-types.gperf"
      {"application/vnd.ms-excel", "xlw"},
      {""}, {""},
#line 270 "media-types.gperf"
      {"application/vnd.ms-htmlhelp", "chm"},
#line 61 "media-types.gperf"
      {"application/pkcs7-mime", "p7c"},
      {""},
#line 120 "media-types.gperf"
      {"application/vnd.apple.installer+xml", "mpkg"},
#line 130 "media-types.gperf"
      {"application/vnd.cinderella", "cdy"},
#line 141 "media-types.gperf"
      {"application/vnd.crick.clicker.palette", "clkp"},
      {""},
#line 121 "media-types.gperf"
      {"application/vnd.apple.mpegurl", "m3u8"},
      {""},
#line 448 "media-types.gperf"
      {"image/svg+xml", "svg"},
#line 328 "media-types.gperf"
      {"application/vnd.pocketlearn", "plf"},
      {""},
#line 227 "media-types.gperf"
      {"application/vnd.isac.fcs", "fcs"},
      {""}, {""},
#line 257 "media-types.gperf"
      {"application/vnd.medcalcdata", "mc1"},
#line 142 "media-types.gperf"
      {"application/vnd.crick.clicker.template", "clkt"},
      {""},
#line 162 "media-types.gperf"
      {"application/vnd.epson.esf", "esf"},
      {""}, {""},
#line 319 "media-types.gperf"
      {"application/vnd.osgeo.mapguide.package", "mgp"},
      {""},
#line 299 "media-types.gperf"
      {"application/vnd.oasis.opendocument.image", "odi"},
      {""},
#line 251 "media-types.gperf"
      {"application/vnd.lotus-notes", "nsf"},
#line 62 "media-types.gperf"
      {"application/pkcs7-signature", "p7s"},
#line 437 "media-types.gperf"
      {"image/gif", "gif"},
#line 343 "media-types.gperf"
      {"application/vnd.semf", "semf"},
#line 253 "media-types.gperf"
      {"application/vnd.lotus-screencam", "scm"},
#line 226 "media-types.gperf"
      {"application/vnd.is-xpr", "xpr"},
#line 101 "media-types.gperf"
      {"application/timestamped-data", "tsd"},
#line 300 "media-types.gperf"
      {"application/vnd.oasis.opendocument.image-template", "oti"},
#line 166 "media-types.gperf"
      {"application/vnd.epson.ssf", "ssf"},
#line 265 "media-types.gperf"
      {"application/vnd.mozilla.xul+xml", "xul"},
      {""}, {""},
#line 169 "media-types.gperf"
      {"application/vnd.ezpix-package", "ez3"},
      {""}, {""},
#line 301 "media-types.gperf"
      {"application/vnd.oasis.opendocument.presentation", "odp"},
      {""},
#line 264 "media-types.gperf"
      {"application/vnd.mophun.certificate", "mpc"},
#line 57 "media-types.gperf"
      {"application/pdf", "pdf"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 86 "media-types.gperf"
      {"application/scvp-vp-request", "spq"},
      {""},
#line 54 "media-types.gperf"
      {"application/ogg", "ogx"},
      {""}, {""}, {""}, {""},
#line 349 "media-types.gperf"
      {"application/vnd.smart.teacher", "teacher"},
      {""},
#line 286 "media-types.gperf"
      {"application/vnd.noblenet-sealer", "nns"},
      {""}, {""},
#line 338 "media-types.gperf"
      {"application/vnd.route66.link66+xml", "link66"},
      {""}, {""},
#line 310 "media-types.gperf"
      {"application/vnd.oma.dd2+xml", "dd2"},
      {""},
#line 139 "media-types.gperf"
      {"application/vnd.crick.clicker", "clkx"},
      {""}, {""},
#line 248 "media-types.gperf"
      {"application/vnd.lotus-1-2-3", "123"},
      {""},
#line 290 "media-types.gperf"
      {"application/vnd.nokia.radio-preset", "rpst"},
#line 333 "media-types.gperf"
      {"application/vnd.pvi.ptid1", "ptid"},
      {""}, {""}, {""}, {""},
#line 291 "media-types.gperf"
      {"application/vnd.nokia.radio-presets", "rpss"},
      {""}, {""}, {""}, {""}, {""},
#line 250 "media-types.gperf"
      {"application/vnd.lotus-freelance", "pre"},
      {""}, {""}, {""}, {""},
#line 116 "media-types.gperf"
      {"application/vnd.americandynamics.acc", "acc"},
#line 484 "media-types.gperf"
      {"text/prs.lines.tag", "dsc"},
#line 20 "media-types.gperf"
      {"application/cdmi-object", "cdmio"},
      {""},
#line 273 "media-types.gperf"
      {"application/vnd.ms-officetheme", "thmx"},
#line 373 "media-types.gperf"
      {"application/vnd.visio", "vsw"},
#line 272 "media-types.gperf"
      {"application/vnd.ms-lrm", "lrm"},
#line 74 "media-types.gperf"
      {"application/reginfo+xml", "rif"},
#line 221 "media-types.gperf"
      {"application/vnd.intergeo", "i2g"},
#line 363 "media-types.gperf"
      {"application/vnd.tmobile-livetv", "tmo"},
      {""}, {""},
#line 311 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.presentationml.presentation", "pptx"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 191 "media-types.gperf"
      {"application/vnd.geonext", "gxt"},
      {""}, {""}, {""}, {""},
#line 117 "media-types.gperf"
      {"application/vnd.amiga.ami", "ami"},
#line 296 "media-types.gperf"
      {"application/vnd.oasis.opendocument.formula-template", "odft"},
#line 220 "media-types.gperf"
      {"application/vnd.intercon.formnet", "xpx"},
      {""}, {""},
#line 230 "media-types.gperf"
      {"application/vnd.jisp", "jisp"},
      {""},
#line 56 "media-types.gperf"
      {"application/patch-ops-error+xml", "xer"},
      {""}, {""}, {""}, {""},
#line 295 "media-types.gperf"
      {"application/vnd.oasis.opendocument.formula", "odf"},
      {""}, {""}, {""}, {""},
#line 415 "media-types.gperf"
      {"audio/mpeg", "m3a"},
      {""},
#line 186 "media-types.gperf"
      {"application/vnd.genomatix.tuxedo", "txd"},
      {""},
#line 193 "media-types.gperf"
      {"application/vnd.geospace", "g3w"},
      {""},
#line 288 "media-types.gperf"
      {"application/vnd.nokia.n-gage.data", "ngdat"},
#line 438 "media-types.gperf"
      {"image/heic", "heic"},
      {""}, {""},
#line 144 "media-types.gperf"
      {"application/vnd.criticaltools.wbs+xml", "wbs"},
      {""}, {""}, {""},
#line 466 "media-types.gperf"
      {"message/rfc822", "mime"},
#line 447 "media-types.gperf"
      {"image/prs.btif", "btif"},
      {""},
#line 228 "media-types.gperf"
      {"application/vnd.jam", "jam"},
#line 259 "media-types.gperf"
      {"application/vnd.mfmp", "mfm"},
#line 16 "media-types.gperf"
      {"application/ccxml+xml", "ccxml"},
#line 164 "media-types.gperf"
      {"application/vnd.epson.quickanime", "qam"},
      {""}, {""},
#line 163 "media-types.gperf"
      {"application/vnd.epson.msf", "msf"},
#line 455 "media-types.gperf"
      {"image/vnd.dxf", "dxf"},
      {""}, {""}, {""}, {""},
#line 275 "media-types.gperf"
      {"application/vnd.ms-project", "mpt"},
#line 174 "media-types.gperf"
      {"application/vnd.frogans.fnc", "fnc"},
      {""},
#line 375 "media-types.gperf"
      {"application/vnd.vsf", "vsf"},
#line 348 "media-types.gperf"
      {"application/vnd.smaf", "mmf"},
#line 470 "media-types.gperf"
      {"model/vnd.dwf", "dwf"},
      {""},
#line 418 "media-types.gperf"
      {"audio/vnd.digital-winds", "eol"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 137 "media-types.gperf"
      {"application/vnd.contact.cmsg", "cdbcmsg"},
#line 187 "media-types.gperf"
      {"application/vnd.geogebra.file", "ggb"},
      {""}, {""},
#line 263 "media-types.gperf"
      {"application/vnd.mophun.application", "mpn"},
      {""}, {""}, {""}, {""},
#line 122 "media-types.gperf"
      {"application/vnd.aristanetworks.swi", "swi"},
#line 260 "media-types.gperf"
      {"application/vnd.micrografx.flo", "flo"},
      {""},
#line 515 "media-types.gperf"
      {"video/vnd.mpegurl", "m4u"},
      {""},
#line 298 "media-types.gperf"
      {"application/vnd.oasis.opendocument.graphics-template", "otg"},
#line 342 "media-types.gperf"
      {"application/vnd.semd", "semd"},
#line 215 "media-types.gperf"
      {"application/vnd.iccprofile", "icm"},
#line 168 "media-types.gperf"
      {"application/vnd.ezpix-album", "ez2"},
#line 188 "media-types.gperf"
      {"application/vnd.geogebra.slides", "ggs"},
#line 172 "media-types.gperf"
      {"application/vnd.fluxtime.clip", "ftc"},
#line 297 "media-types.gperf"
      {"application/vnd.oasis.opendocument.graphics", "odg"},
      {""}, {""},
#line 507 "media-types.gperf"
      {"video/quicktime", "mov"},
      {""}, {""},
#line 173 "media-types.gperf"
      {"application/vnd.framemaker", "book"},
      {""},
#line 55 "media-types.gperf"
      {"application/oxps", "oxps"},
#line 199 "media-types.gperf"
      {"application/vnd.groove-help", "ghf"},
#line 147 "media-types.gperf"
      {"application/vnd.dart", "dart"},
#line 217 "media-types.gperf"
      {"application/vnd.immervision-ivp", "ivp"},
      {""}, {""}, {""}, {""},
#line 189 "media-types.gperf"
      {"application/vnd.geogebra.tool", "ggt"},
#line 202 "media-types.gperf"
      {"application/vnd.groove-tool-message", "gtm"},
#line 203 "media-types.gperf"
      {"application/vnd.groove-tool-template", "tpl"},
      {""}, {""}, {""},
#line 198 "media-types.gperf"
      {"application/vnd.groove-account", "gac"},
#line 161 "media-types.gperf"
      {"application/vnd.enliven", "nml"},
#line 231 "media-types.gperf"
      {"application/vnd.joost.joda-archive", "joda"},
#line 149 "media-types.gperf"
      {"application/vnd.dece.data", "uvvd"},
      {""},
#line 451 "media-types.gperf"
      {"image/vnd.dece.graphic", "uvvg"},
      {""}, {""}, {""}, {""}, {""},
#line 195 "media-types.gperf"
      {"application/vnd.google-earth.kml+xml", "kml"},
#line 150 "media-types.gperf"
      {"application/vnd.dece.ttml+xml", "uvvt"},
      {""},
#line 200 "media-types.gperf"
      {"application/vnd.groove-identity-message", "gim"},
#line 325 "media-types.gperf"
      {"application/vnd.pg.osasli", "ei6"},
      {""}, {""},
#line 76 "media-types.gperf"
      {"application/resource-lists+xml", "rl"},
      {""},
#line 324 "media-types.gperf"
      {"application/vnd.pg.format", "str"},
      {""}, {""},
#line 77 "media-types.gperf"
      {"application/resource-lists-diff+xml", "rld"},
      {""}, {""},
#line 497 "media-types.gperf"
      {"text/vnd.in3d.spot", "spot"},
      {""}, {""}, {""},
#line 505 "media-types.gperf"
      {"video/mpeg", "m2v"},
#line 401 "media-types.gperf"
      {"application/x-x509-ca-cert", "crt"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 190 "media-types.gperf"
      {"application/vnd.geometry-explorer", "gre"},
      {""}, {""}, {""},
#line 255 "media-types.gperf"
      {"application/vnd.macports.portpkg", "portpkg"},
      {""}, {""},
#line 115 "media-types.gperf"
      {"application/vnd.airzip.filesecure.azs", "azs"},
#line 294 "media-types.gperf"
      {"application/vnd.oasis.opendocument.database", "odb"},
      {""}, {""},
#line 219 "media-types.gperf"
      {"application/vnd.insors.igm", "igm"},
#line 28 "media-types.gperf"
      {"application/exi", "exi"},
      {""},
#line 262 "media-types.gperf"
      {"application/vnd.mif", "mif"},
#line 284 "media-types.gperf"
      {"application/vnd.nitf", "nitf"},
      {""}, {""},
#line 119 "media-types.gperf"
      {"application/vnd.antix.game-component", "atx"},
      {""},
#line 490 "media-types.gperf"
      {"text/vcard", "vcard"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 443 "media-types.gperf"
      {"image/jpeg", "jpg"},
#line 201 "media-types.gperf"
      {"application/vnd.groove-injector", "grv"},
      {""},
#line 279 "media-types.gperf"
      {"application/vnd.mseq", "mseq"},
      {""},
#line 241 "media-types.gperf"
      {"application/vnd.kenameaapp", "htke"},
      {""}, {""},
#line 456 "media-types.gperf"
      {"image/vnd.fastbidsheet", "fbs"},
#line 216 "media-types.gperf"
      {"application/vnd.igloader", "igl"},
      {""},
#line 105 "media-types.gperf"
      {"application/vnd.3gpp2.tcap", "tcap"},
#line 289 "media-types.gperf"
      {"application/vnd.nokia.n-gage.symbian.install", "n-gage"},
#line 454 "media-types.gperf"
      {"image/vnd.dwg", "dwg"},
#line 18 "media-types.gperf"
      {"application/cdmi-container", "cdmic"},
#line 58 "media-types.gperf"
      {"application/pgp-encrypted", "pgp"},
      {""},
#line 152 "media-types.gperf"
      {"application/vnd.dece.zip", "uvvz"},
      {""}, {""},
#line 334 "media-types.gperf"
      {"application/vnd.realvnc.bed", "bed"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 41 "media-types.gperf"
      {"application/mathematica", "mb"},
      {""}, {""},
#line 405 "media-types.gperf"
      {"application/xml", "xsl"},
      {""}, {""},
#line 486 "media-types.gperf"
      {"text/tab-separated-values", "tsv"},
#line 32 "media-types.gperf"
      {"application/inkml+xml", "inkml"},
#line 175 "media-types.gperf"
      {"application/vnd.frogans.ltf", "ltf"},
      {""},
#line 357 "media-types.gperf"
      {"application/vnd.svd", "svd"},
      {""},
#line 327 "media-types.gperf"
      {"application/vnd.pmi.widget", "wg"},
      {""},
#line 209 "media-types.gperf"
      {"application/vnd.hp-hps", "hps"},
#line 154 "media-types.gperf"
      {"application/vnd.dna", "dna"},
      {""}, {""},
#line 59 "media-types.gperf"
      {"application/pgp-signature", "sig"},
#line 157 "media-types.gperf"
      {"application/vnd.dvb.ait", "ait"},
#line 210 "media-types.gperf"
      {"application/vnd.hp-jlyt", "jlt"},
      {""}, {""}, {""}, {""}, {""},
#line 368 "media-types.gperf"
      {"application/vnd.uiq.theme", "utz"},
      {""},
#line 229 "media-types.gperf"
      {"application/vnd.jcp.javame.midlet-rms", "rms"},
#line 42 "media-types.gperf"
      {"application/mathml+xml", "mathml"},
#line 329 "media-types.gperf"
      {"application/vnd.powerbuilder6", "pbd"},
#line 278 "media-types.gperf"
      {"application/vnd.ms-xpsdocument", "xps"},
      {""}, {""},
#line 356 "media-types.gperf"
      {"application/vnd.sus-calendar", "susp"},
#line 493 "media-types.gperf"
      {"text/vnd.fly", "fly"},
      {""}, {""}, {""},
#line 113 "media-types.gperf"
      {"application/vnd.ahead.space", "ahead"},
#line 256 "media-types.gperf"
      {"application/vnd.mcd", "mcd"},
      {""},
#line 309 "media-types.gperf"
      {"application/vnd.olpc-sugar", "xo"},
      {""},
#line 460 "media-types.gperf"
      {"image/vnd.fujixerox.edmics-rlc", "rlc"},
      {""},
#line 192 "media-types.gperf"
      {"application/vnd.geoplan", "g2w"},
      {""},
#line 464 "media-types.gperf"
      {"image/vnd.xiff", "xif"},
      {""}, {""}, {""}, {""},
#line 281 "media-types.gperf"
      {"application/vnd.muvee.style", "msty"},
#line 267 "media-types.gperf"
      {"application/vnd.ms-cab-compressed", "cab"},
      {""}, {""}, {""},
#line 423 "media-types.gperf"
      {"audio/vnd.ms-playready.media.pya", "pya"},
      {""},
#line 196 "media-types.gperf"
      {"application/vnd.google-earth.kmz", "kmz"},
      {""}, {""},
#line 21 "media-types.gperf"
      {"application/cdmi-queue", "cdmiq"},
#line 283 "media-types.gperf"
      {"application/vnd.neurolanguage.nlu", "nlu"},
      {""}, {""}, {""}, {""},
#line 140 "media-types.gperf"
      {"application/vnd.crick.clicker.keyboard", "clkk"},
      {""},
#line 94 "media-types.gperf"
      {"application/sparql-results+xml", "srx"},
      {""},
#line 224 "media-types.gperf"
      {"application/vnd.ipunplugged.rcprofile", "rcprofile"},
      {""}, {""}, {""}, {""}, {""},
#line 459 "media-types.gperf"
      {"image/vnd.fujixerox.edmics-mmr", "mmr"},
      {""},
#line 321 "media-types.gperf"
      {"application/vnd.osgi.subsystem", "esa"},
      {""},
#line 158 "media-types.gperf"
      {"application/vnd.dvb.service", "svc"},
      {""}, {""}, {""},
#line 249 "media-types.gperf"
      {"application/vnd.lotus-approach", "apr"},
      {""},
#line 388 "media-types.gperf"
      {"application/vnd.yamaha.hv-voice", "hvp"},
#line 205 "media-types.gperf"
      {"application/vnd.hal+xml", "hal"},
#line 111 "media-types.gperf"
      {"application/vnd.adobe.fxp", "fxpl"},
#line 392 "media-types.gperf"
      {"application/vnd.yamaha.smaf-phrase", "spf"},
      {""}, {""},
#line 371 "media-types.gperf"
      {"application/vnd.uoml+xml", "uoml"},
      {""}, {""}, {""}, {""},
#line 387 "media-types.gperf"
      {"application/vnd.yamaha.hv-script", "hvs"},
      {""}, {""}, {""}, {""},
#line 128 "media-types.gperf"
      {"application/vnd.chemdraw+xml", "cdxml"},
#line 102 "media-types.gperf"
      {"application/vnd.3gpp.pic-bw-large", "plb"},
      {""}, {""}, {""}, {""}, {""},
#line 243 "media-types.gperf"
      {"application/vnd.koan", "skm"},
      {""}, {""}, {""},
#line 391 "media-types.gperf"
      {"application/vnd.yamaha.smaf-audio", "saf"},
#line 360 "media-types.gperf"
      {"application/vnd.syncml.dm+xml", "xdm"},
      {""},
#line 359 "media-types.gperf"
      {"application/vnd.syncml.dm+wbxml", "bdm"},
      {""},
#line 276 "media-types.gperf"
      {"application/vnd.ms-works", "wdb"},
      {""}, {""}, {""},
#line 434 "media-types.gperf"
      {"image/bmp", "bmp"},
      {""},
#line 517 "media-types.gperf"
      {"video/vnd.uvvu.mp4", "uvvu"},
      {""}, {""}, {""}, {""},
#line 386 "media-types.gperf"
      {"application/vnd.yamaha.hv-dic", "hvd"},
      {""}, {""},
#line 421 "media-types.gperf"
      {"audio/vnd.dts.hd", "dtshd"},
#line 367 "media-types.gperf"
      {"application/vnd.ufdl", "ufdl"},
      {""}, {""}, {""}, {""},
#line 103 "media-types.gperf"
      {"application/vnd.3gpp.pic-bw-small", "psb"},
      {""},
#line 114 "media-types.gperf"
      {"application/vnd.airzip.filesecure.azf", "azf"},
#line 512 "media-types.gperf"
      {"video/vnd.dece.video", "uvvv"},
#line 440 "media-types.gperf"
      {"image/heif", "heif"},
      {""}, {""}, {""},
#line 50 "media-types.gperf"
      {"application/msword", "dot"},
#line 394 "media-types.gperf"
      {"application/vnd.zul", "zirz"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 258 "media-types.gperf"
      {"application/vnd.mediastation.cdkey", "cdkey"},
      {""}, {""},
#line 143 "media-types.gperf"
      {"application/vnd.crick.clicker.wordbank", "clkw"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 369 "media-types.gperf"
      {"application/vnd.umajin", "umj"},
      {""},
#line 345 "media-types.gperf"
      {"application/vnd.shana.informed.formtemplate", "itp"},
      {""},
#line 285 "media-types.gperf"
      {"application/vnd.noblenet-directory", "nnd"},
#line 51 "media-types.gperf"
      {"application/mxf", "mxf"},
      {""},
#line 347 "media-types.gperf"
      {"application/vnd.shana.informed.package", "ipk"},
      {""}, {""}, {""},
#line 282 "media-types.gperf"
      {"application/vnd.mynfc", "taglet"},
      {""}, {""}, {""}, {""},
#line 104 "media-types.gperf"
      {"application/vnd.3gpp.pic-bw-var", "pvb"},
#line 197 "media-types.gperf"
      {"application/vnd.grafeq", "gqs"},
      {""}, {""}, {""},
#line 417 "media-types.gperf"
      {"audio/vnd.dece.audio", "uvva"},
#line 280 "media-types.gperf"
      {"application/vnd.musician", "mus"},
      {""},
#line 413 "media-types.gperf"
      {"audio/basic", "snd"},
#line 45 "media-types.gperf"
      {"application/metalink4+xml", "meta4"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 436 "media-types.gperf"
      {"image/g3fax", "g3"},
      {""}, {""}, {""},
#line 384 "media-types.gperf"
      {"application/vnd.xara", "xar"},
      {""}, {""},
#line 183 "media-types.gperf"
      {"application/vnd.fujixerox.docuworks", "xdw"},
#line 93 "media-types.gperf"
      {"application/sparql-query", "rq"},
#line 337 "media-types.gperf"
      {"application/vnd.rig.cryptonote", "cryptonote"},
      {""}, {""},
#line 461 "media-types.gperf"
      {"image/vnd.ms-modi", "mdi"},
#line 396 "media-types.gperf"
      {"application/voicexml+xml", "vxml"},
      {""}, {""}, {""},
#line 510 "media-types.gperf"
      {"video/vnd.dece.pd", "uvvp"},
      {""}, {""}, {""}, {""},
#line 465 "media-types.gperf"
      {"image/webp", "webp"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 353 "media-types.gperf"
      {"application/vnd.sqlite3", "sqlite3"},
      {""},
#line 511 "media-types.gperf"
      {"video/vnd.dece.sd", "uvvs"},
#line 148 "media-types.gperf"
      {"application/vnd.data-vision.rdz", "rdz"},
#line 410 "media-types.gperf"
      {"application/yang", "yang"},
#line 346 "media-types.gperf"
      {"application/vnd.shana.informed.interchange", "iif"},
      {""},
#line 389 "media-types.gperf"
      {"application/vnd.yamaha.openscoreformat", "osf"},
#line 473 "media-types.gperf"
      {"model/vnd.vtu", "vtu"},
      {""}, {""}, {""},
#line 40 "media-types.gperf"
      {"application/marcxml+xml", "mrcx"},
      {""},
#line 498 "media-types.gperf"
      {"text/vnd.sun.j2me.app-descriptor", "jad"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 225 "media-types.gperf"
      {"application/vnd.irepository.package+xml", "irp"},
#line 204 "media-types.gperf"
      {"application/vnd.groove-vcard", "vcg"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 390 "media-types.gperf"
      {"application/vnd.yamaha.openscoreformat.osfpvg+xml", "osfpvg"},
      {""}, {""}, {""}, {""}, {""},
#line 374 "media-types.gperf"
      {"application/vnd.visionary", "vis"},
      {""},
#line 22 "media-types.gperf"
      {"application/davmount+xml", "davmount"},
      {""}, {""},
#line 516 "media-types.gperf"
      {"video/vnd.ms-playready.media.pyv", "pyv"},
      {""}, {""}, {""},
#line 218 "media-types.gperf"
      {"application/vnd.immervision-ivu", "ivu"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 439 "media-types.gperf"
      {"image/heic-sequence", "heics"},
#line 242 "media-types.gperf"
      {"application/vnd.kidspiration", "kia"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 72 "media-types.gperf"
      {"application/pskc+xml", "pskcxml"},
      {""},
#line 146 "media-types.gperf"
      {"application/vnd.cups-ppd", "ppd"},
      {""},
#line 134 "media-types.gperf"
      {"application/vnd.cluetrust.cartomobile-config", "c11amc"},
#line 98 "media-types.gperf"
      {"application/ssml+xml", "ssml"},
      {""},
#line 469 "media-types.gperf"
      {"model/vnd.collada+xml", "dae"},
#line 135 "media-types.gperf"
      {"application/vnd.cluetrust.cartomobile-config-pkg", "c11amz"},
#line 31 "media-types.gperf"
      {"application/hyperstudio", "stk"},
#line 36 "media-types.gperf"
      {"application/lost+xml", "lostxml"},
      {""},
#line 110 "media-types.gperf"
      {"application/vnd.adobe.formscentral.fcdt", "fcdt"},
#line 336 "media-types.gperf"
      {"application/vnd.recordare.musicxml+xml", "musicxml"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""},
#line 184 "media-types.gperf"
      {"application/vnd.fujixerox.docuworks.binder", "xbd"},
      {""}, {""},
#line 395 "media-types.gperf"
      {"application/vnd.zzazz.deck+xml", "zaz"},
#line 372 "media-types.gperf"
      {"application/vnd.vcx", "vcx"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 96 "media-types.gperf"
      {"application/srgs+xml", "grxml"},
      {""}, {""},
#line 445 "media-types.gperf"
      {"image/ktx", "ktx"},
      {""}, {""},
#line 475 "media-types.gperf"
      {"model/x3d+xml", "x3dz"},
      {""}, {""}, {""},
#line 452 "media-types.gperf"
      {"image/vnd.djvu", "djv"},
#line 19 "media-types.gperf"
      {"application/cdmi-domain", "cdmid"},
#line 235 "media-types.gperf"
      {"application/vnd.kde.kformula", "kfo"},
      {""}, {""},
#line 151 "media-types.gperf"
      {"application/vnd.dece.unspecified", "uvvx"},
      {""}, {""}, {""},
#line 37 "media-types.gperf"
      {"application/mac-binhex40", "hqx"},
#line 432 "media-types.gperf"
      {"font/woff2", "woff2"},
      {""},
#line 65 "media-types.gperf"
      {"application/pkix-cert", "cer"},
#line 468 "media-types.gperf"
      {"model/mesh", "silo"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 99 "media-types.gperf"
      {"application/tei+xml", "teicorpus"},
      {""}, {""}, {""}, {""}, {""},
#line 46 "media-types.gperf"
      {"application/mets+xml", "mets"},
      {""}, {""}, {""}, {""},
#line 26 "media-types.gperf"
      {"application/emma+xml", "emma"},
#line 66 "media-types.gperf"
      {"application/pkix-crl", "crl"},
#line 64 "media-types.gperf"
      {"application/pkix-attr-cert", "ac"},
      {""},
#line 12 "media-types.gperf"
      {"application/andrew-inset", "ez"},
#line 385 "media-types.gperf"
      {"application/vnd.xfdl", "xfdl"},
      {""},
#line 17 "media-types.gperf"
      {"application/cdmi-capability", "cdmia"},
#line 23 "media-types.gperf"
      {"application/dssc+der", "dssc"},
      {""},
#line 13 "media-types.gperf"
      {"application/atom+xml", "atom"},
#line 332 "media-types.gperf"
      {"application/vnd.publishare-delta-tree", "qps"},
#line 232 "media-types.gperf"
      {"application/vnd.kahootz", "ktr"},
#line 159 "media-types.gperf"
      {"application/vnd.dynageo", "geo"},
#line 69 "media-types.gperf"
      {"application/pls+xml", "pls"},
#line 38 "media-types.gperf"
      {"application/mads+xml", "mads"},
      {""}, {""}, {""}, {""},
#line 47 "media-types.gperf"
      {"application/mods+xml", "mods"},
      {""}, {""}, {""}, {""}, {""},
#line 358 "media-types.gperf"
      {"application/vnd.syncml+xml", "xsm"},
      {""}, {""},
#line 266 "media-types.gperf"
      {"application/vnd.ms-artgalry", "cil"},
#line 92 "media-types.gperf"
      {"application/smil+xml", "smil"},
#line 244 "media-types.gperf"
      {"application/vnd.kodak-descriptor", "sse"},
      {""},
#line 171 "media-types.gperf"
      {"application/vnd.fdsn.seed", "dataless"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 397 "media-types.gperf"
      {"application/wasm", "wasm"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 125 "media-types.gperf"
      {"application/vnd.blueice.multipass", "mpm"},
#line 170 "media-types.gperf"
      {"application/vnd.fdsn.mseed", "mseed"},
      {""},
#line 496 "media-types.gperf"
      {"text/vnd.in3d.3dml", "3dml"},
      {""}, {""}, {""}, {""},
#line 441 "media-types.gperf"
      {"image/heif-sequence", "heifs"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 156 "media-types.gperf"
      {"application/vnd.dreamfactory", "dfac"},
#line 431 "media-types.gperf"
      {"font/woff", "woff"},
#line 68 "media-types.gperf"
      {"application/pkixcmp", "pki"},
      {""}, {""}, {""},
#line 97 "media-types.gperf"
      {"application/sru+xml", "sru"},
      {""}, {""}, {""}, {""},
#line 236 "media-types.gperf"
      {"application/vnd.kde.kivio", "flw"},
      {""}, {""}, {""}, {""},
#line 238 "media-types.gperf"
      {"application/vnd.kde.kpresenter", "kpt"},
      {""}, {""}, {""},
#line 308 "media-types.gperf"
      {"application/vnd.oasis.opendocument.text-web", "oth"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 100 "media-types.gperf"
      {"application/thraud+xml", "tfi"},
      {""}, {""}, {""},
#line 124 "media-types.gperf"
      {"application/vnd.audiograph", "aep"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 180 "media-types.gperf"
      {"application/vnd.fujitsu.oasysgp", "fg5"},
      {""}, {""}, {""}, {""}, {""},
#line 261 "media-types.gperf"
      {"application/vnd.micrografx.igx", "igx"},
      {""},
#line 153 "media-types.gperf"
      {"application/vnd.denovo.fcselayout-link", "fe_launch"},
      {""}, {""}, {""},
#line 463 "media-types.gperf"
      {"image/vnd.wap.wbmp", "wbmp"},
#line 177 "media-types.gperf"
      {"application/vnd.fujitsu.oasys", "oas"},
      {""}, {""},
#line 181 "media-types.gperf"
      {"application/vnd.fujitsu.oasysprs", "bh2"},
      {""}, {""}, {""},
#line 400 "media-types.gperf"
      {"application/wspolicy+xml", "wspolicy"},
      {""},
#line 370 "media-types.gperf"
      {"application/vnd.unity", "unityweb"},
      {""}, {""}, {""},
#line 233 "media-types.gperf"
      {"application/vnd.kde.karbon", "karbon"},
#line 237 "media-types.gperf"
      {"application/vnd.kde.kontour", "kon"},
      {""},
#line 344 "media-types.gperf"
      {"application/vnd.shana.informed.formdata", "ifm"},
      {""}, {""}, {""}, {""}, {""},
#line 509 "media-types.gperf"
      {"video/vnd.dece.mobile", "uvvm"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 182 "media-types.gperf"
      {"application/vnd.fujixerox.ddd", "ddd"},
#line 287 "media-types.gperf"
      {"application/vnd.noblenet-web", "nnw"},
      {""}, {""},
#line 513 "media-types.gperf"
      {"video/vnd.dvb.file", "dvb"},
      {""}, {""}, {""}, {""}, {""},
#line 211 "media-types.gperf"
      {"application/vnd.hydrostatix.sof-data", "sfd-hdstx"},
#line 178 "media-types.gperf"
      {"application/vnd.fujitsu.oasys2", "oa2"},
      {""},
#line 246 "media-types.gperf"
      {"application/vnd.llamagraphics.life-balance.desktop", "lbd"},
      {""}, {""}, {""}, {""}, {""},
#line 494 "media-types.gperf"
      {"text/vnd.fmi.flexstor", "flx"},
      {""}, {""}, {""},
#line 398 "media-types.gperf"
      {"application/widget", "wgt"},
#line 194 "media-types.gperf"
      {"application/vnd.gmx", "gmx"},
      {""}, {""},
#line 254 "media-types.gperf"
      {"application/vnd.lotus-wordpro", "lwp"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 377 "media-types.gperf"
      {"application/vnd.wap.wmlc", "wmlc"},
      {""},
#line 79 "media-types.gperf"
      {"application/rpki-ghostbusters", "gbr"},
      {""}, {""},
#line 112 "media-types.gperf"
      {"application/vnd.adobe.xdp+xml", "xdp"},
#line 378 "media-types.gperf"
      {"application/vnd.wap.wmlscriptc", "wmlsc"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 207 "media-types.gperf"
      {"application/vnd.hhe.lesson-player", "les"},
      {""},
#line 247 "media-types.gperf"
      {"application/vnd.llamagraphics.life-balance.exchange+xml", "lbe"},
      {""},
#line 381 "media-types.gperf"
      {"application/vnd.wordperfect", "wpd"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 185 "media-types.gperf"
      {"application/vnd.fuzzysheet", "fzs"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 393 "media-types.gperf"
      {"application/vnd.yellowriver-custom-menu", "cmp"},
      {""}, {""}, {""}, {""},
#line 223 "media-types.gperf"
      {"application/vnd.intu.qfx", "qfx"},
      {""}, {""}, {""}, {""}, {""},
#line 313 "media-types.gperf"
      {"application/vnd.openxmlformats-officedocument.presentationml.slideshow", "ppsx"},
      {""}, {""}, {""},
#line 126 "media-types.gperf"
      {"application/vnd.bmi", "bmi"},
      {""}, {""}, {""}, {""}, {""},
#line 27 "media-types.gperf"
      {"application/epub+zip", "epub"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 24 "media-types.gperf"
      {"application/dssc+xml", "xdssc"},
      {""}, {""},
#line 495 "media-types.gperf"
      {"text/vnd.graphviz", "gv"},
#line 222 "media-types.gperf"
      {"application/vnd.intu.qbo", "qbo"},
      {""}, {""},
#line 75 "media-types.gperf"
      {"application/relax-ng-compact-syntax", "rnc"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 239 "media-types.gperf"
      {"application/vnd.kde.kspread", "ksp"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 383 "media-types.gperf"
      {"application/vnd.wt.stf", "stf"},
#line 179 "media-types.gperf"
      {"application/vnd.fujitsu.oasys3", "oa3"},
      {""}, {""}, {""}, {""}, {""},
#line 240 "media-types.gperf"
      {"application/vnd.kde.kword", "kwt"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 508 "media-types.gperf"
      {"video/vnd.dece.hd", "uvvh"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 208 "media-types.gperf"
      {"application/vnd.hp-hpid", "hpid"},
      {""}, {""}, {""},
#line 214 "media-types.gperf"
      {"application/vnd.ibm.secure-container", "sc"},
      {""},
#line 339 "media-types.gperf"
      {"application/vnd.sailingtracker.track", "st"},
      {""},
#line 380 "media-types.gperf"
      {"application/vnd.wolfram.player", "nbp"},
      {""}, {""}, {""},
#line 406 "media-types.gperf"
      {"application/xml-dtd", "dtd"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""},
#line 33 "media-types.gperf"
      {"application/ipfix", "ipfix"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 457 "media-types.gperf"
      {"image/vnd.fpx", "fpx"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 30 "media-types.gperf"
      {"application/gml+xml", "gml"},
      {""}, {""}, {""},
#line 129 "media-types.gperf"
      {"application/vnd.chipnuts.karaoke-mmd", "mmd"},
      {""}, {""}, {""}, {""}, {""},
#line 492 "media-types.gperf"
      {"text/vnd.dvb.subtitle", "sub"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 462 "media-types.gperf"
      {"image/vnd.net-fpx", "npx"},
      {""},
#line 118 "media-types.gperf"
      {"application/vnd.anser-web-certificate-issue-initiation", "cii"},
      {""}, {""}, {""}, {""},
#line 73 "media-types.gperf"
      {"application/rdf+xml", "rdf"},
      {""},
#line 127 "media-types.gperf"
      {"application/vnd.businessobjects", "rep"},
      {""}, {""}, {""}, {""},
#line 234 "media-types.gperf"
      {"application/vnd.kde.kchart", "chrt"},
      {""}, {""}, {""}, {""},
#line 67 "media-types.gperf"
      {"application/pkix-pkipath", "pkipath"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 409 "media-types.gperf"
      {"application/xv+xml", "xvm"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 155 "media-types.gperf"
      {"application/vnd.dpgraph", "dpg"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 212 "media-types.gperf"
      {"application/vnd.ibm.modcap", "list3820"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 91 "media-types.gperf"
      {"application/shf+xml", "shf"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""},
#line 404 "media-types.gperf"
      {"application/xhtml+xml", "xht"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 408 "media-types.gperf"
      {"application/xslt+xml", "xslt"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 213 "media-types.gperf"
      {"application/vnd.ibm.rights-management", "irm"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""},
#line 403 "media-types.gperf"
      {"application/xenc+xml", "xenc"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 402 "media-types.gperf"
      {"application/xcap-diff+xml", "xdf"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 411 "media-types.gperf"
      {"application/yin+xml", "yin"},
      {""}, {""}, {""}, {""},
#line 206 "media-types.gperf"
      {"application/vnd.hbci", "hbci"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 407 "media-types.gperf"
      {"application/xop+xml", "xop"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 176 "media-types.gperf"
      {"application/vnd.fsc.weblaunch", "fsc"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 382 "media-types.gperf"
      {"application/vnd.wqd", "wqd"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 83 "media-types.gperf"
      {"application/sbml+xml", "sbml"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 376 "media-types.gperf"
      {"application/vnd.wap.wbxml", "wbxml"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 330 "media-types.gperf"
      {"application/vnd.previewsystems.box", "box"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 399 "media-types.gperf"
      {"application/wsdl+xml", "wsdl"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 71 "media-types.gperf"
      {"application/prs.cww", "cww"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 379 "media-types.gperf"
      {"application/vnd.webturbo", "wtb"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 450 "media-types.gperf"
      {"image/vnd.adobe.photoshop", "psd"},
      {""}, {""},
#line 472 "media-types.gperf"
      {"model/vnd.gtw", "gtw"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 499 "media-types.gperf"
      {"text/vnd.wap.wml", "wml"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 500 "media-types.gperf"
      {"text/vnd.wap.wmlscript", "wmls"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 43 "media-types.gperf"
      {"application/mbox", "mbox"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 453 "media-types.gperf"
      {"image/vnd.dvb.subtitle", "sub"}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
#line 519 "media-types.gperf"

