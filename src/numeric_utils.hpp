#pragma once
#include <limits>
#include <optional>

namespace choose {

namespace num {

template <typename T>
bool in(T var, T min_inclusive, T max_inclusive) { //
  return var >= min_inclusive && var <= max_inclusive;
};

template <typename T>
std::optional<T> mul_overflow(const T& a, const T& b) {
  static_assert(std::is_unsigned<T>::value);
  T out = a * b;
  if (a != 0 && out / a != b) {
    return std::nullopt;
  } else {
    return out;
  }
}

template <typename T>
std::optional<T> add_overflow(const T& a, const T& b) {
  static_assert(std::is_unsigned<T>::value);
  T out = a + b;
  if (out < a) {
    return std::nullopt;
  } else {
    return out;
  }
}

// parse a non-negative T from null terminating base 10 string.
template <typename T, typename OnErr>
T parse_unsigned(OnErr onErr, const char* str, bool zero_allowed = true, bool max_allowed = true) {
  // this function was surprisingly difficult to make.
  //  - atol gives UB for value out of range
  //  - strtoul is trash and doesn't handle negative values in a sane way
  //  - strtol doesn't give a full range
  // reverting to a dumb and slow way to make sure this is correct.
  // its only used in parsing args, so performance isn't a concern.
  if (str == 0) {
    onErr();
    return 0;
  }
  T out = 0;
  while (1) {
    char ch = *str++;
    if (ch == '\0') {
      break;
    }

    if (!in(ch, '0', '9')) {
      onErr();
      return 0;
    }

    if (auto mul_result = mul_overflow(out, (T)10)) {
      out = *mul_result;
    } else {
      onErr();
      return 0;
    }

    if (auto add_result = add_overflow(out, (T)(ch - '0'))) {
      out = *add_result;
    } else {
      onErr();
      return 0;
    }
  }

  if (!max_allowed && out == std::numeric_limits<T>::max()) {
    onErr();
    return 0;
  }

  if (!zero_allowed && out == 0) {
    onErr();
    return 0;
  }

  return out;
}

} // namespace num
} // namespace choose