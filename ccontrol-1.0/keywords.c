/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf keywords.gperf  */
/* Computed positions: -k'1' */

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

#line 1 "keywords.gperf"

/* Keep -Wmissing-declarations happy: */
const struct ccontrol_command *
find_keyword (register const char *str, register size_t len);
#line 7 "keywords.gperf"
struct ccontrol_command {
	const char *name;
	enum token_type type;
};
/* maximum key range = 26, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash_keyword (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 10, 28,  0,
       5,  5, 28, 28, 28, 20, 28, 28,  5, 15,
       5, 28, 28, 28, 28, 28, 28, 28, 15, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28, 28, 28, 28, 28, 28
    };
  return len + asso_values[(unsigned char)str[0]];
}

const struct ccontrol_command *
find_keyword (register const char *str, register size_t len)
{
  enum
    {
      TOTAL_KEYWORDS = 17,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 13,
      MIN_HASH_VALUE = 2,
      MAX_HASH_VALUE = 27
    };

  static const struct ccontrol_command wordlist[] =
    {
      {""}, {""},
#line 19 "keywords.gperf"
      {"cc", CC},
#line 20 "keywords.gperf"
      {"c++", CPLUSPLUS},
#line 30 "keywords.gperf"
      {"cpus", CPUS},
      {""},
#line 23 "keywords.gperf"
      {"ccache", CCACHE},
#line 21 "keywords.gperf"
      {"ld", LD},
#line 33 "keywords.gperf"
      {"env", ENV},
#line 28 "keywords.gperf"
      {"nice", NICE},
      {""},
#line 24 "keywords.gperf"
      {"distcc", DISTCC},
#line 31 "keywords.gperf"
      {"disable", DISABLE},
#line 32 "keywords.gperf"
      {"add", ADD},
#line 34 "keywords.gperf"
      {"lock-file", LOCK_FILE},
      {""},
#line 18 "keywords.gperf"
      {"no-parallel", NO_PARALLEL},
#line 25 "keywords.gperf"
      {"distcc-hosts", DISTCC_HOSTS},
#line 26 "keywords.gperf"
      {"distc++-hosts", DISTCPLUSPLUS_HOSTS},
#line 22 "keywords.gperf"
      {"make", MAKE},
      {""}, {""},
#line 27 "keywords.gperf"
      {"verbose", VERBOSE},
      {""}, {""}, {""}, {""},
#line 29 "keywords.gperf"
      {"include", INCLUDE}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash_keyword (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
