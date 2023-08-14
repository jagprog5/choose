#pragma once

#include <algorithm>
#include <vector>

#include "numeric_utils.hpp"

namespace choose {

template <typename it, typename Comp>
void stable_partial_sort(it begin, it middle, it end, Comp comp) {
  // adapted from https://stackoverflow.com/a/27248519/15534181
  std::vector<it> sorted;
  sorted.resize(end - begin);
  auto to = sorted.begin();
  for (auto p = begin; p != end; ++p) {
    *to++ = p;
  }
  auto n = middle - begin;

  std::partial_sort(sorted.begin(), sorted.begin() + n, sorted.end(), [&](const it& lhs, const it& rhs) {
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
// so for 123.456, the fraction part is 456.
// for 123 the fraction part is an empty string.
// given two fractional parts, this is a less than comparison between them
bool fraction_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  while (1) {
    // bound checks

    if (lhs_begin >= lhs_end) {
      // lhs is empty
      while (rhs_begin < rhs_end) {
        char rhs_ch = *rhs_begin++;
        if (rhs_ch == '0') {
          // keep going
        } else if (num::in(rhs_ch, '1', '9')) {
          // rhs had non-zero fractional part left, so the lhs to less than
          return true;
        } else {
          // some weird non digit. like 123.aaa
          return false;
        }
      }
      return false; // exhausted rhs trying to find non-zero digit. equal
    }

    if (rhs_begin >= rhs_end) {
      // rhs is empty. precondition lhs is not empty
      do {
        char lhs_ch = *lhs_begin++;
        if (lhs_ch == '0') {
          // keep going
        } else if (num::in(lhs_ch, '1', '9')) {
          // lhs had non-zero fractional part left, so the rhs is less than
          return false;
        } else {
          // some weird non digit. like 123.aaa
          return false;
        }
      } while (lhs_begin < lhs_end);
      return false; // exhausted lhs trying to find non-zero digit. equal
    }

    // precondition lhs and rhs not empty
    char lhs_ch = *lhs_begin++;
    char rhs_ch = *rhs_begin++;
    if (!num::in(lhs_ch, '1', '9') || !num::in(rhs_ch, '1', '9')) {
      return false;
    }
    // TODO
  }
}

}

// compare two numbers, like +000123,456,789.99912134000
bool numeric_compare(const char* lhs_begin, const char* lhs_end, const char* rhs_begin, const char* rhs_end) {
  // TODO
}

} // namespace choose
