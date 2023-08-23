#pragma once

#include <algorithm>
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

// leveraged under the following assumptions:
//   - end of string has not been reached
//   - character frequency. e.g. a obtaining any non-zero digit is less likely than zero digit (1/9 vs 8/9)
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

// increments pos until it doesn't point to a space or it points at the end.
// returns the character pos points to on return, or STR_END if it's at the end.
char trim_leading_spaces(const char*& pos, const char* end) {
  while (likely(pos < end)) {
    char ch = *pos;
    if (ch != ' ' && ch != '\t') {
      return ch;
    }
    ++pos;
  }
  return STR_END;
}

// pos_ch is the character pointed to by pos.
// if pos points to a sign, increment pos and updates pos_ch appropriately.
// return true if a sign was present and it was negative
bool trim_leading_sign(char& pos_ch, const char*& pos, const char* end) {
  auto do_increment = [&]() {
    ++pos;
    if (likely(pos < end)) {
      pos_ch = *pos;
    } else {
      pos_ch = STR_END;
    }
  };

  if (pos_ch == '-') {
    do_increment();
    return true;
  }

  if (pos_ch == '+') {
    do_increment();
  }
  return false;
}

// pos_ch is the character pointed to by pos
// increments pos until it doesn't point at a '0' (ignoring thousands seps),
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
      return true;
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

// compare two numbers, like          -123,456,789.99912134000
// numeric strings match this: ^[ \t]*[-+]?[0-9,]*(?:\.[0-9]*)?$
bool numeric_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  char lhs_ch = trim_leading_spaces(lhs_begin, lhs_end);
  char rhs_ch = trim_leading_spaces(rhs_begin, rhs_end);
  bool lhs_negative = trim_leading_sign(lhs_ch, lhs_begin, lhs_end);
  bool rhs_negative = trim_leading_sign(rhs_ch, rhs_begin, rhs_end);

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
        ++rhs_begin; // point after decimal
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
  char lhs_ch = trim_leading_spaces(lhs_begin, lhs_end);
  char rhs_ch = trim_leading_spaces(rhs_begin, rhs_end);
  bool lhs_negative = trim_leading_sign(lhs_ch, lhs_begin, lhs_end);
  bool rhs_negative = trim_leading_sign(rhs_ch, rhs_begin, rhs_end);

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
      } else if (rhs_ch == ',') {
        // both end of string
        return true;
      } else { // '.'
        // rhs reached decimal point and lhs reached end of string
        // check if rhs fraction is zero
        ++rhs_begin; // point after decimal
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
        ++lhs_begin; // point after decimal
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

  // trim leading spaces
  char ch = trim_leading_spaces(begin, end);

  // negative sign is added to hash later if overall string is non-zero
  bool is_negative = trim_leading_sign(ch, begin, end);

  // trim leading zeros
  trim_leading_zeros(ch, begin, end);

  if (unlikely(ch == STR_END)) {
    return ret;
  }

  // precondition begin < end
  // precondition ch == *begin

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
    if (ch == '.') {
      // begin points on the decimal.
      // decimal is applied from within do_fractional_hash
      do_fractional_hash(begin, end);
      break;
    }

    if (ch != ',') { // ignore thousands sep
      apply(ch);
    }
    ++begin;
    if (begin >= end) {
      break;
    }
    ch = *begin;
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
