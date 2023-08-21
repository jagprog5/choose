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
    // bound checks
    if (unlikely(rhs_begin >= rhs_end)) {
      // rhs is empty. every case here leads to rhs less than or equal to lhs
      return false;
    }
    if (unlikely(lhs_begin >= lhs_end)) {
      // lhs is empty. precondition rhs is not empty
      do {
        if (likely(*rhs_begin++ != '0')) {
          // rhs had non zero fraction remaining.
          return true;
        }
      } while (likely(rhs_begin < rhs_end));
      return false; // exhausted rhs trying to find non-zero digit. equal
    }

    // precondition lhs and rhs not empty
    char lhs_ch = *lhs_begin++;
    char rhs_ch = *rhs_begin++;
    if (likely(lhs_ch != rhs_ch)) {
      return lhs_ch < rhs_ch;
    }
  }
}

// begins must point one after the decimal place
bool fraction_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    if (unlikely(rhs_begin >= rhs_end)) {
      // rhs has nothing left. if lhs only has zeros left they are equal
      while (likely(lhs_begin < lhs_end)) {
        if (likely(*lhs_begin++ != '0')) {
          return false;
        }
      }
      return true;
    }

    if (unlikely(lhs_begin >= lhs_end)) {
      // lhs has nothing left. if rhs only has zeros left they are equal.
      // precondition rhs not empty
      do {
        if (likely(*rhs_begin++ != '0')) {
          return false;
        }
      } while (likely(rhs_begin < rhs_end));
      return true;
    }

    // precondition lhs and rhs not empty
    char lhs_ch = *lhs_begin++;
    char rhs_ch = *rhs_begin++;
    if (likely(lhs_ch != rhs_ch)) {
      return false;
    }
  }
}

void trim_leading_spaces(const char*& pos, const char* end) {
  while (likely(pos < end) && (*pos == ' ' || *pos == '\t')) {
    ++pos;
  }
}

// returns true if negative
bool trim_leading_sign(const char*& pos, const char* end) {
  if (likely(pos < end)) {
    if (*pos == '+') {
      ++pos;
    } else if (*pos == '-') {
      ++pos;
      return true;
    }
  }
  return false;
}

void trim_leading_zeros(const char*& pos, const char* end) {
  while (likely(pos < end) && (*pos == '0' || *pos == ',')) {
    ++pos;
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

// if the return value is a thousands sep, then end of the string was reached
// else, this is the character that is returned from this part of the string at
// the current position
char get_next(const char*& pos, const char* end) {
  while (likely(pos < end)) {
    char ch = *pos++;
    if (likely(ch != ',')) {
      return ch;
    }
  }
  return ',';
}

} // namespace

// compare two numbers, like          -123,456,789.99912134000
// numeric strings match this: ^[\x20\x9]*[-+]?[0-9,]*(?:\.[0-9]*)?$
bool numeric_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  trim_leading_spaces(lhs_begin, lhs_end);
  trim_leading_spaces(rhs_begin, rhs_end);
  bool lhs_negative = trim_leading_sign(lhs_begin, lhs_end);
  bool rhs_negative = trim_leading_sign(rhs_begin, rhs_end);

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
    std::swap(lhs_begin, rhs_begin);
    std::swap(lhs_end, rhs_end);
  }

  trim_leading_zeros(lhs_begin, lhs_end);
  trim_leading_zeros(rhs_begin, rhs_end);

  while (1) {
    char lhs_ch = get_next(lhs_begin, lhs_end);
    char rhs_ch = get_next(rhs_begin, rhs_end);

    if (likely(lhs_ch != rhs_ch)) {
      if (unlikely(rhs_ch == ',')) {
        // rhs reached end of string, lhs hasn't
        return false;
      }

      if (unlikely(rhs_ch == '.')) {
        if (unlikely(lhs_ch == ',')) {
          // check if rhs decimal is entirely zero
          while (likely(rhs_begin < rhs_end)) {
            if (unlikely(*rhs_begin++ != '0')) {
              return true;
            }
          }
          return false;
        } else {
          return false;
        }
      }

      if (unlikely(lhs_ch == ',' || lhs_ch == '.')) {
        // lhs reached end of non-fractional and rhs hasn't
        return true;
      }

      // go to appropriate loop now that it's know which side has a greater
      // leading non-fractional digit
      if (lhs_ch > rhs_ch) {
        goto left_loop;
      } else {
        goto right_loop;
      }
    } else {
      // precondition lhs_ch == rhs_ch
      if (unlikely(rhs_ch == ',')) {
        // both reached end of string
        return false;
      } else if (unlikely(rhs_ch == '.')) {
        // both reached decimal place at same time
        return fraction_compare(lhs_begin, lhs_end, rhs_begin, rhs_end);
      } else {
        // both reached same digit or same character not in numeric format
        continue;
      }
    }
  }

left_loop:
  while (1) {
    char lhs_ch = get_next(lhs_begin, lhs_end);
    char rhs_ch = get_next(rhs_begin, rhs_end);
    if (unlikely(rhs_ch == ',' || rhs_ch == '.')) {
      // even if lhs also runs out of characters at the same time,
      // lhs is still greater
      return false;
    }

    if (unlikely(lhs_ch == ',' || lhs_ch == '.')) {
      return true;
    }
  }

right_loop:
  while (1) {
    char lhs_ch = get_next(lhs_begin, lhs_end);
    char rhs_ch = get_next(rhs_begin, rhs_end);
    if (unlikely(lhs_ch == ',' || lhs_ch == '.')) {
      // even if rhs also runs out of characters at the same time,
      // rhs is still greater
      return true;
    }

    if (unlikely(rhs_ch == ',' || rhs_ch == '.')) {
      return false;
    }
  }
}

bool numeric_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  trim_leading_spaces(lhs_begin, lhs_end);
  trim_leading_spaces(rhs_begin, rhs_end);
  bool lhs_negative = trim_leading_sign(lhs_begin, lhs_end);
  bool rhs_negative = trim_leading_sign(rhs_begin, rhs_end);

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

  trim_leading_zeros(lhs_begin, lhs_end);
  trim_leading_zeros(rhs_begin, rhs_end);

  while (1) {
    char lhs_ch = get_next(lhs_begin, lhs_end);
    char rhs_ch = get_next(rhs_begin, rhs_end);

    if (likely(lhs_ch != rhs_ch)) {
      if (unlikely(rhs_ch == ',' && lhs_ch == '.')) {
        // rhs reached end of string, lhs reached decimal
        // check if lhs decimal is entirely zero
        while (likely(lhs_begin < lhs_end)) {
          if (unlikely(*lhs_begin++ != '0')) {
            return false;
          }
        }
        return true;
      }

      if (unlikely(rhs_ch == '.' && lhs_ch == ',')) {
        // rhs reached decimal and lhs reached end of string
        // check if rhs decimal is entirely zero
        while (likely(rhs_begin < rhs_end)) {
          if (unlikely(*rhs_begin++ != '0')) {
            return false;
          }
        }
        return true;
      }
      
      // all other cases for chars not equal
      return false;
    } else {
      // precondition lhs_ch == rhs_ch
      if (unlikely(lhs_ch == ',')) {
        // both reached end of string at same time
        return true;
      } else if (unlikely(lhs_ch == '.')) {
        // both reached decimal place at same time
        return fraction_equal(lhs_begin, lhs_end, rhs_begin, rhs_end);
      } else {
        // both reached same digit or same character not in numeric format
        continue;
      }
    }
  }
}

// follows same format mentioned in numeric_compare
size_t numeric_hash(const char* begin, const char* end) {
  // https://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_range_id420926.html
  // initial seed of 0 is reasonable
  size_t ret = 0;

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
  trim_leading_spaces(begin, end);

  // negative sign is added to hash later if overall string is non-zero
  bool is_negative = trim_leading_sign(begin, end);

  // trim leading zeros
  trim_leading_zeros(begin, end);

  // begin points to the decimal point
  auto do_fractional_hash = [&](const char* begin, const char* end) {
    const char* pos = end - 1; // point to last character, and iter backwards

    // trim trailing zeros
    while (pos >= begin && *pos == '0') {
      --pos;
    }

    if (begin == pos) {
      // fractional part is zero length
      return; // do not hash decimal place
    }

    while (pos >= begin) {
      char ch = *pos--;
      apply(ch);
    }
  };

  // take hash of non-fractional part
  while (begin < end) {
    char ch = *begin;
    if (ch == ',') {
      // ignore thousands sep
      ++begin;
      continue;
    } else if (ch == '.') {
      // begin points on the decimal.
      // decimal is applied from within do_fractional_hash
      do_fractional_hash(begin, end);
      break;
    }
    apply(ch);
    ++begin;
  }

  if (is_negative && ret) {
    // ret != 0 provides the weak guarantee that the overall string is non-zero. this
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
