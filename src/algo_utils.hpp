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

namespace {

// a numeric string's fraction part is the section that comes after the decimal.
// so for 123.456, the fraction part is 456. beginning at 4, and ending just after 6
// for 789 the fraction part is an empty string.
// given two fractional parts, this is a less than comparison between them
bool fraction_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    // bound checks

    if (rhs_begin >= rhs_end) {
      // rhs is empty. every case here leads to rhs less than or equal to lhs
      return false;
    }

    if (lhs_begin >= lhs_end) {
      // lhs is empty. precondition rhs is not empty
      do {
        char rhs_ch = *rhs_begin++;
        if (rhs_ch == '0') {
          // keep going
        } else {
          // rhs had non zero fraction remaining.
          // this is also triggered by non-digits, which is fine
          return true;
        }
      } while (rhs_begin < rhs_end);
      return false; // exhausted rhs trying to find non-zero digit. equal
    }

    // precondition lhs and rhs not empty
    char lhs_ch = *lhs_begin++;
    char rhs_ch = *rhs_begin++;
    if (lhs_ch < rhs_ch) {
      return true;
    } else if (lhs_ch > rhs_ch) {
      return false;
    }
  }
}

// begins must point one after the decimal place
bool fraction_equal(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    if (rhs_begin >= rhs_end) {
      // rhs has nothing left. if lhs only has zeros left they are equal
      while (lhs_begin < lhs_end) {
        char ch = *lhs_begin++;
        if (ch == '0') {
          // keep going
        } else {
          return false;
        }
      }
      return true;
    }

    if (lhs_begin >= lhs_end) {
      // lhs has nothing left. if rhs only has zeros left they are equal.
      // precondition rhs not empty
      do {
        char ch = *rhs_begin++;
        if (ch == '0') {
          // keep going.
        } else {
          return false;
        }
      } while (rhs_begin < rhs_end);
      return true;
    }

    // precondition lhs and rhs not empty
    char lhs_ch = *lhs_begin++;
    char rhs_ch = *rhs_begin++;
    if (lhs_ch != rhs_ch) {
      return false;
    }
  }
}

void trim_leading_spaces(const char*& pos, const char* end) {
  while (pos < end && std::isspace(*pos)) {
    ++pos;
  }
}

// returns true if negative
bool trim_leading_sign(const char*& pos, const char* end) {
  if (pos < end) {
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
  while (pos < end && (*pos == '0' || *pos == ',')) {
    ++pos;
  }
}

// checks if remaining string (fractional and non fractional) is zero or not
bool non_zero(const char* begin, const char* end) {
  while (begin < end) {
    char ch = *begin++;
    if (ch == '0' || ch == ',' || ch == '.') {
      // keep going.
    } else {
      return true;
    }
  }
  return false;
}

// if the return value is a thousands sep, then end of the string was reached
// else, this is the character that is returned from this part of the string at
// the current position. pos is set to the position of the returned character. it
// must be incremented by the callee to get the next character for the next call
char get_next(const char*& pos, const char* end) {
  while (pos < end) {
    char ch = *pos++;
    if (ch == ',') {
      // ignore thousands sep
    } else {
      return ch;
    }
  }
  return ',';
}

} // namespace

// compare two numbers, like          -123,456,789.99912134000
// the numbers have these format properties:
//  - any number of leading whitespace is allowed
//  - a leading positive or negative sign is allowed following the whitespace
//  - leading zeros are allowed but must follow the sign
//  - thousand separators can be used anywhere after the leading sign and before the decimal
//  - trailing zeros are allowed at the end of the fractional part
//  - the fractional length can be zero. like 123.
//  - the non fractional length can be zero. like +.,,,123
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
    if (lhs_non_zero || rhs_non_zero) {
      return true;
    }
    // both are zero
    return false;
  }

  // precondition lhs_negative == rhs_negative
  bool both_negative = lhs_negative;

  if (both_negative) {
    std::swap(lhs_begin, rhs_begin);
    std::swap(lhs_end, rhs_end);
  }

  trim_leading_zeros(lhs_begin, lhs_end);
  trim_leading_zeros(rhs_begin, rhs_end);

  // first non-zero diff between non-fractional digits
  int lead_dif = 0;

  while (1) {
    char lhs_ch = get_next(lhs_begin, lhs_end);
    char rhs_ch = get_next(rhs_begin, rhs_end);

    // for either side, a character, the decimal point, or end of string can be
    // reached. handle each case appropriately

    if (lhs_ch == ',' && rhs_ch == ',') {
      // both end of string
      return lead_dif < 0;
    }

    if (lhs_ch == '.' && rhs_ch == '.') {
      // both reached decimal point
      if (lead_dif == 0) {
        // non fraction section is identical
        return fraction_compare(lhs_begin, lhs_end, rhs_begin, rhs_end);
      } else {
        return lead_dif < 0;
      }
    }

    if (lhs_ch == '.' && rhs_ch == ',') {
      // lhs reached decimal point and rhs reached end of string
      if (lead_dif == 0) {
        // non fraction section is identical
        return false;
      } else {
        return lead_dif < 0;
      }
    }

    if (lhs_ch == ',' && rhs_ch == '.') {
      // rhs reached decimal place and lhs reached end of string
      if (lead_dif == 0) {
        // check if rhs fraction is zero
        while (rhs_begin < rhs_end) {
          if (*rhs_begin++ != '0') {
            return true;
          }
        }
        return false;
      } else {
        return lead_dif < 0;
      }
    }

    if (lhs_ch == '.' || lhs_ch == ',') {
      // lhs reached decimal or end and rhs still has non fractional digits
      return true;
    }

    if (rhs_ch == '.' || rhs_ch == ',') {
      // rhs reached decimal or end and lhs still has non fractional digits
      return false;
    }

    // precondition lhs and rhs have char to compare
    if (lead_dif == 0 && lhs_ch != rhs_ch) {
      lead_dif = lhs_ch - rhs_ch;
      if (lead_dif != 0) {
        // consider going to second loop. the exact same as this one, but
        // without this lead_dif setter logic
      }
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
    if (both_zero) {
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

    if (lhs_ch == ',' && rhs_ch == ',') {
      // both end of string
      return true;
    }

    if (lhs_ch == '.' && rhs_ch == '.') {
      // both reach decimal at same time
      return fraction_equal(lhs_begin, lhs_end, rhs_begin, rhs_end);
    }

    if (lhs_ch == '.' && rhs_ch == ',') {
      // lhs reached decimal point and rhs reached end of string
      // check if lhs fraction is zero
      while (lhs_begin < lhs_end) {
        if (*lhs_begin++ != '0') {
          return false; // not equal
        }
      }
      return true;
    }

    // above with lhs and rhs flipped
    if (rhs_ch == '.' && lhs_ch == ',') {
      while (rhs_begin < rhs_end) {
        if (*rhs_begin++ != '0') {
          return false; // not equal
        }
      }
      return true;
    }

    if (lhs_ch == '.' || lhs_ch == ',') {
      // lhs reached decimal or end and rhs still has non fractional digits
      return false;
    }

    if (rhs_ch == '.' || rhs_ch == ',') {
      // rhs reached decimal or end and lhs still has non fractional digits
      return false;
    }

    if (lhs_ch != rhs_ch) {
      return false;
    }
  }
}

// follows same format mentioned in numeric_compare
size_t numeric_hash(const char* begin, const char* end) {
  // https://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_range_id420926.html
  // initial seed of 0 is reasonable
  size_t ret = 0;
  bool is_non_zero = false; // disambiguate since ret could return to 0

  auto apply = [&](char ch) {
    is_non_zero = true;
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

  if (is_negative && is_non_zero) {
    apply('-');
  }
  return ret;
}

} // namespace choose
