#pragma once

#include <algorithm>
#include <charconv>
#include <execution>
#include <vector>

namespace choose {

template <typename ExecutionPolicy, typename it, typename Comp>
void stable_partial_sort(ExecutionPolicy&& policy, it begin, it middle, it end, Comp comp) {
  static_assert(std::is_same_v<typename std::iterator_traits<it>::iterator_category, //
                               std::random_access_iterator_tag>);
  // adapted from https://stackoverflow.com/a/27248519/15534181
  std::vector<it> sorted;
  sorted.resize(end - begin);
  auto to = sorted.begin();
  for (auto p = begin; p != end; ++p) {
    *to++ = p;
  }

  auto n = middle - begin;
  std::partial_sort(std::forward<ExecutionPolicy>(policy), sorted.begin(), sorted.begin() + n, sorted.end(), [&](const it& lhs, const it& rhs) {
    // First see if the underlying elements differ.
    if (comp(*lhs, *rhs))
      return true;
    if (comp(*rhs, *lhs))
      return false;
    // Underlying elements are the same, so compare iterators; these represent
    // position in original vector.
    return lhs < rhs;
  });

  std::vector<typename it::value_type> replacement;
  replacement.resize(n);
  for (decltype(n) i = 0; i < n; ++i) {
    replacement[i] = *sorted[i];
  }

  auto from = replacement.cbegin();
  for (auto pos = begin; pos != middle; ++end) {
    *pos++ = *from++;
  }
}

namespace {
// attempt to get a floating point type that is the same size as size_t. defaults to float
using floating_hash_t = std::conditional<sizeof(size_t) == sizeof(long double),
                                         long double, //
                                         std::conditional<sizeof(size_t) == sizeof(double), double, float>::type>::type;
} // namespace

bool general_numeric_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) { //
  float lhs, rhs;
  std::from_chars_result lhs_ret = std::from_chars(lhs_begin, lhs_end, lhs, std::chars_format::general);
  std::from_chars_result rhs_ret = std::from_chars(rhs_begin, rhs_end, rhs, std::chars_format::general);

  // entire string conversion not required for parse success.
  if (rhs_ret.ec != std::errc()) {
    return false; // rhs parse failure. even if lhs also had parse failure, false returned here
  }

  if (lhs_ret.ec != std::errc()) {
    return true; // lhs parse failure and rhs parse success
  }

  return lhs < rhs;
}

bool general_numeric_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  float lhs, rhs;
  std::from_chars_result lhs_ret = std::from_chars(lhs_begin, lhs_end, lhs, std::chars_format::general);
  std::from_chars_result rhs_ret = std::from_chars(rhs_begin, rhs_end, rhs, std::chars_format::general);

  bool lhs_err = lhs_ret.ec != std::errc();
  bool rhs_err = rhs_ret.ec != std::errc();
  if (lhs_err || rhs_err) {
    return lhs_err && rhs_err;
  }

  return lhs == rhs;
}

size_t general_numeric_hash(const char* begin, const char* end) {
  floating_hash_t val;
  std::from_chars_result ret = std::from_chars(begin, end, val, std::chars_format::general);

  if (ret.ec != std::errc()) {
    return 0; // parse failure gives 0 hash
  }

  return (size_t)val; // identity
};

// locale is not used for numeric functions. keeping consistent with
// std::from_chars for general numeric

// it was necessary to create numeric comparison functions for a few reasons: as
// pointed out by GNU sort, it avoids overflow issues and is faster since nearly
// always the entire string doesn't have to be read. additionally, GNU sort's
// implementation is based on c strings, versus here ranges were used. so this
// didn't exist yet.

// leveraged under the following assumptions:
//   - end of string has not been reached
//   - character frequency. e.g. obtaining any non-zero digit is more likely than zero digit (8/9 vs 1/9)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

namespace {

// a numeric string's fraction part is the section that comes after the decimal.
// so for 123.456, the fraction part is 456. beginning at 4, and ending just after 6
// for 789 the fraction part is an empty string.
// given two fractional parts, this is a less than comparison between them
bool fraction_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    if (likely(rhs_begin < rhs_end)) {
      if (likely(lhs_begin < lhs_end)) {
        // precondition lhs and rhs not empty
        char lhs_ch = *lhs_begin++;
        char rhs_ch = *rhs_begin++;
        if (likely(lhs_ch != rhs_ch)) {
          return lhs_ch < rhs_ch;
        }
      } else {
        // lhs is empty. precondition rhs is not empty
        do {
          if (likely(*rhs_begin++ != '0')) {
            // rhs had non zero fraction remaining.
            return true;
          }
        } while (likely(rhs_begin < rhs_end));
        return false; // exhausted rhs trying to find non-zero digit. equal
      }
    } else {
      // rhs is empty. every case here leads to rhs less than or equal to lhs
      return false;
    }
  }
}

// begins must point one after the decimal place
bool fraction_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    if (likely(rhs_begin < rhs_end)) {
      if (likely(lhs_begin < lhs_end)) {
        // precondition lhs and rhs not empty
        char lhs_ch = *lhs_begin++;
        char rhs_ch = *rhs_begin++;
        if (likely(lhs_ch != rhs_ch)) {
          return false;
        }
      } else {
        // lhs has nothing left. if rhs only has zeros left they are equal.
        // precondition rhs not empty
        do {
          if (likely(*rhs_begin++ != '0')) {
            return false;
          }
        } while (likely(rhs_begin < rhs_end));
        return true;
      }
    } else {
      // rhs has nothing left. if lhs only has zeros left they are equal
      while (likely(lhs_begin < lhs_end)) {
        if (likely(*lhs_begin++ != '0')) {
          return false;
        }
      }
      return true;
    }
  }
}

// similar bit pattern to decimal point, for convenience
static constexpr char STR_END = '.' | (char)0b10000000;
static constexpr char END_MASK = 0b01111111;

// is_negative is set to true or false, depending on if the string begins with a negative sign
// pos points to the first position that is not the negative sign, or to the end.
// pos_ch is the value at pos, or STR_END if at the end
void trim_leading_sign(bool& is_negative, char& pos_ch, const char*& pos, const char* end) {
  if (likely(pos < end)) {
    if (*pos == '-') {
      is_negative = true;
      ++pos;
      if (likely(pos < end)) {
        pos_ch = *pos;
      } else {
        pos_ch = STR_END;
      }
      return;
    }

    is_negative = false;
    pos_ch = *pos;
  } else {
    is_negative = false;
    pos_ch = STR_END;
  }
}

// pos_ch is the character pointed to by pos
// increments pos while it doesn't point at a '0' (ignoring thousands seps),
// and updates pos_ch appropriately,
void trim_leading_zeros(char& pos_ch, const char*& pos, const char* end) {
  while (1) {
    if (pos_ch != '0' && pos_ch != ',') {
      return;
    }
    ++pos;
    if (likely(pos < end)) {
      pos_ch = *pos;
    } else {
      pos_ch = STR_END;
      return;
    }
  }
}

// checks if remaining string (fractional and non fractional) is zero or not
bool non_zero(const char* begin, const char* end) {
  while (likely(begin < end)) {
    char ch = *begin++;
    if (likely(ch != '0' && ch != ',' && ch != '.')) {
      return ch != STR_END;
    }
  }
  return false;
}

// returns the first non thousands sep char at or after pos, and increments pos appropriately.
// if the end of string is reached, then STR_END is returned instead
char get_next(const char*& pos, const char* end) {
  while (likely(pos < end)) {
    char ch = *pos++;
    if (likely(ch != ',')) {
      return ch;
    }
  }
  return STR_END;
}

} // namespace

// compare two numbers, like -123,456,789.99912134000
// numeric strings match this: ^-?[0-9,]*(?:\.[0-9]*)?$
bool numeric_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  char lhs_ch, rhs_ch;
  bool lhs_negative, rhs_negative;
  trim_leading_sign(lhs_negative, lhs_ch, lhs_begin, lhs_end);
  trim_leading_sign(rhs_negative, rhs_ch, rhs_begin, rhs_end);

  if (!lhs_negative && rhs_negative) {
    return false;
  }

  if (lhs_negative && !rhs_negative) {
    bool lhs_non_zero = non_zero(lhs_begin, lhs_end);
    bool rhs_non_zero = non_zero(rhs_begin, rhs_end);
    bool both_zero = !lhs_non_zero && !rhs_non_zero;
    if (unlikely(both_zero)) {
      return false;
    } else {
      return true;
    }
  }

  // precondition lhs_negative == rhs_negative
  bool both_negative = lhs_negative;
  if (both_negative) {
    std::swap(lhs_ch, rhs_ch);
    std::swap(lhs_begin, rhs_begin);
    std::swap(lhs_end, rhs_end);
  }

  trim_leading_zeros(lhs_ch, lhs_begin, lhs_end);
  trim_leading_zeros(rhs_ch, rhs_begin, rhs_end);
  ++lhs_begin; // possibly points one after end (when lhs_ch == STR_END), but that's ok
  ++rhs_begin;

  while (1) {
    if (likely((lhs_ch & END_MASK) != '.')) {
      if (likely((rhs_ch & END_MASK) != '.')) {
        // neither lhs or rhs have reached end of string or decimal
        // this is the most likely branch
        // precondition lhs and rhs have char to compare

        if (likely(lhs_ch != rhs_ch)) {
          bool left_leaning = lhs_ch > rhs_ch;
          while (1) {
            lhs_ch = get_next(lhs_begin, lhs_end);
            rhs_ch = get_next(rhs_begin, rhs_end);
            if (unlikely((rhs_ch & END_MASK) == '.')) {
              // even if lhs also runs out of characters at the same time,
              // lhs is still greater
              if (left_leaning) {
                return false;
              } else {
                return (lhs_ch & END_MASK) == '.';
              }
            }

            if (unlikely((lhs_ch & END_MASK) == '.')) {
              return true;
            }
          }
        }
      } else {
        // rhs reached decimal or end and lhs still has non fractional digits
        return false;
      }
    } else if (lhs_ch == STR_END) {
      if (rhs_ch == STR_END) {
        // both end of string at same time
        return false;
      } else if (rhs_ch == '.') {
        // rhs reached decimal place and lhs reached end of string
        // check if rhs fraction is zero
        while (likely(rhs_begin < rhs_end)) {
          if (likely(*rhs_begin++ != '0')) {
            return true;
          }
        }
        return false;
      } else {
        // lhs reached end of string and rhs still has non fractional digits
        return true;
      }
    } else { // '.'
      if (rhs_ch == STR_END) {
        // lhs reached decimal point and rhs reached end of string
        return false;
      } else if (rhs_ch != '.') {
        // lhs reached decimal and rhs still has non fractional digits
        return true;
      } else {
        // both reached decimal at same time
        return fraction_compare(lhs_begin, lhs_end, rhs_begin, rhs_end);
      }
    }

    lhs_ch = get_next(lhs_begin, lhs_end);
    rhs_ch = get_next(rhs_begin, rhs_end);
  }
}

bool numeric_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  char lhs_ch, rhs_ch;
  bool lhs_negative, rhs_negative;
  trim_leading_sign(lhs_negative, lhs_ch, lhs_begin, lhs_end);
  trim_leading_sign(rhs_negative, rhs_ch, rhs_begin, rhs_end);

  if (lhs_negative != rhs_negative) {
    bool lhs_non_zero = non_zero(lhs_begin, lhs_end);
    bool rhs_non_zero = non_zero(rhs_begin, rhs_end);
    bool both_zero = !lhs_non_zero && !rhs_non_zero;
    if (unlikely(both_zero)) {
      return true;
    } else {
      return false;
    }
  }

  trim_leading_zeros(lhs_ch, lhs_begin, lhs_end);
  trim_leading_zeros(rhs_ch, rhs_begin, rhs_end);
  ++lhs_begin;
  ++rhs_begin;

  while (1) {
    // for either side, a character, the decimal point, or end of string can be
    // reached. handle each case appropriately

    if (likely((lhs_ch & END_MASK) != '.')) {
      if (likely((rhs_ch & END_MASK) != '.')) {
        // neither lhs or rhs have reached end of string or decimal
        // this is the most likely branch
        // precondition lhs and rhs have char to compare
        if (likely(lhs_ch != rhs_ch)) {
          return false;
        }
      } else {
        // rhs reached decimal or end and lhs still has non fractional digits
        return false;
      }
    } else if (lhs_ch == STR_END) {
      if ((rhs_ch & END_MASK) != '.') {
        // lhs reached end of string and rhs still has non fractional digits
        return false;
      } else if (rhs_ch == STR_END) {
        // both end of string
        return true;
      } else { // '.'
        // rhs reached decimal point and lhs reached end of string
        // check if rhs fraction is zero
        while (likely(rhs_begin < rhs_end)) {
          if (likely(*rhs_begin++ != '0')) {
            return false; // not equal
          }
        }
        return true;
      }
    } else { // '.'
      if ((rhs_ch & END_MASK) != '.') {
        // lhs reached decimal rhs still has non fractional digits
        return false;
      } else if (rhs_ch == STR_END) {
        // lhs reached decimal point and rhs reached end of string
        // check if lhs fraction is zero
        while (likely(lhs_begin < lhs_end)) {
          if (likely(*lhs_begin++ != '0')) {
            return false; // not equal
          }
        }
        return true;
      } else { // '.'
        // both reach decimal at same time
        return fraction_equal(lhs_begin, lhs_end, rhs_begin, rhs_end);
      }
    }
    lhs_ch = get_next(lhs_begin, lhs_end);
    rhs_ch = get_next(rhs_begin, rhs_end);
  }
}

// follows same format mentioned in numeric_compare
size_t numeric_hash(const char* begin, const char* end) {
  // https://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_range_id420926.html
  // initial seed of 0 is reasonable
  static constexpr size_t INITIAL_SEED = 0;
  size_t ret = INITIAL_SEED;

  auto apply = [&](char ch) {
    // https://github.com/HowardHinnant/hash_append/issues/7#issuecomment-371758166
    // extend boost implementation from just 32 bit to 64 bit as well
    // this probably isn't a good idea though, since better hashes exist
    static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8);
    if constexpr (sizeof(size_t) == 4) {
      ret ^= ch + 0x9e3779b9U + (ret << 6) + (ret >> 2);
    } else {
      ret ^= ch + 0x9e3779b97f4a7c15LLU + (ret << 12) + (ret >> 4);
    }
  };

  char ch;
  bool is_negative;
  trim_leading_sign(is_negative, ch, begin, end);
  trim_leading_zeros(ch, begin, end);

  // begin points to the decimal point
  auto do_fractional_hash = [&](const char*& begin, const char*& end) {
    const char*& pos = end; // alias
    --pos;                  // point to last character, and iter backwards

    // pos is lower bounded by decimal point
    while (unlikely(*pos == '0')) {
      --pos;
    }

    if (likely(begin != pos)) {
      while (likely(pos >= begin)) {
        apply(*pos--);
      }
    } else {
      // fractional part is zero length. do not hash decimal place
    }
  };

  while (1) {
    if (likely((ch & END_MASK) != '.')) {
      // ch isn't end of string or decimal
      if (ch != ',') { // ignore thousands sep
        apply(ch);
      }
      ++begin;
      if (begin >= end) {
        break;
      }
      ch = *begin;
    } else if (ch == '.') {
      // begin points on the decimal.
      // decimal is applied from within do_fractional_hash
      do_fractional_hash(begin, end);
      break;
    } else { // STR_END
      break; // for consistency with other functions, must break here
    }
  }

  if (is_negative && ret != INITIAL_SEED) {
    // ret != INITIAL_SEED provides the weak guarantee that the overall string is non-zero. this
    // won't happen always since the hash so far could happen to be zero for other
    // strings.

    // if there was a leading sign and the string is non-zero then apply the negative
    // sign. if the negative sign isn't applied (because ret == 0 yet string non-zero),
    // that's ok. slight increase in collision rate.
    apply('-');
  }
  return ret;
}

} // namespace choose

/*
// a fuzzy checker for the numeric functions
#include <stdlib.h>
#include <time.h>
#include <cassert>

int main() {
  srand(10);

  auto get_rand_vec = []() {
    std::vector<char> random_chars;
    random_chars.resize(rand() % 8);
    // bool has_decimal = false;
    for (int i = 0; i < random_chars.size(); ++i) {
      random_chars[i] = rand();
    }
    return random_chars;
  };

  int length = rand() % 8;

  for (int i = 0; i < 100000000; ++i) {
    if (i % 100000 == 0) {
      printf("%d\n", i);
    }
    auto lhs = get_rand_vec();
    auto rhs = get_rand_vec();

    auto on_failure = [&](const char* msg) {
      puts(msg);
      for (char ch : lhs) {
        printf("%u", (unsigned char)ch);
        putchar('|');
      }
      putchar('\n');
      for (char ch : rhs) {
        printf("%u", (unsigned char)ch);
        putchar('|');
      }
      putchar('\n');
      assert(false);
    };

    bool lhs_lt_rhs = choose::numeric_compare(&*lhs.cbegin(), &*lhs.cend(), &*rhs.cbegin(), &*rhs.cend());
    bool rhs_lt_lhs = choose::numeric_compare(&*rhs.cbegin(), &*rhs.cend(), &*lhs.cbegin(), &*lhs.cend());
    if (lhs_lt_rhs && rhs_lt_lhs) {
      on_failure("comparison ill formed");
    }
    bool equal = choose::numeric_equal(&*lhs.cbegin(), &*lhs.cend(), &*rhs.cbegin(), &*rhs.cend());
    if (equal != (!lhs_lt_rhs && !rhs_lt_lhs)) {
      on_failure("equality and comparison disagree");
    }

    auto lhs_hash = choose::numeric_hash(&*lhs.cbegin(), &*lhs.cend());
    auto rhs_hash = choose::numeric_hash(&*rhs.cbegin(), &*rhs.cend());

    bool hash_equal = lhs_hash == rhs_hash;
    if (equal != hash_equal) {
      printf("equal %d, hash equal %d\n", equal, hash_equal);
      // will eventually happen due to hash collisions
      on_failure("hash equality and equality disagree");
    }

    auto middle = get_rand_vec();
    bool lhs_lt_middle = choose::numeric_compare(&*lhs.cbegin(), &*lhs.cend(), &*middle.cbegin(), &*middle.cend());
    bool middle_lt_rhs = choose::numeric_compare(&*middle.cbegin(), &*middle.cend(), &*rhs.cbegin(), &*rhs.cend());
    if (lhs_lt_middle && middle_lt_rhs) {
      if (!lhs_lt_rhs) {
        on_failure("transitive fail");
      }
    }
  }
}
*/