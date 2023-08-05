#pragma once
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>

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

// parse T from null or comma terminating base 10 string.
template <typename T, typename OnErr>
std::enable_if_t<std::is_unsigned<T>::value, T> parse_number(OnErr onErr, const char* str, bool zero_allowed = true, bool max_allowed = true) {
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

  if (*str == '+') {
    ++str;
  }

  while (1) {
    char ch = *str++;
    if (ch == '\0' || ch == ',') {
      break;
    }

    if (!in(ch, '0', '9')) {
      onErr();
      return 0;
    }

    if (auto mul_result = mul_overflow(out, T(10))) {
      out = *mul_result;
    } else {
      onErr();
      return 0;
    }

    if (auto add_result = add_overflow(out, T(ch - '0'))) {
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

template <typename T, typename OnErr>
std::enable_if_t<std::is_signed<T>::value, T> parse_number(OnErr onErr, const char* str) {
  if (str == 0) {
    onErr();
    return 0;
  }
  bool has_leading_negative = false;
  if (*str == '-') {
    has_leading_negative = true;
    ++str;
  }
  using unsigned_T = std::make_unsigned_t<T>;
  auto out = parse_number<unsigned_T>(onErr, str);
  if (!has_leading_negative) {
    if (out > std::numeric_limits<T>::max()) {
      onErr();
      return 0;
    } else {
      return T(out);
    }
  } else {
    if (out > unsigned_T{std::numeric_limits<T>::max()} + 1) {
      onErr();
      return 0;
    } else {
      return -T(out);
    }
  }
}

template <typename T, typename OnErr>
std::tuple<T, std::optional<T>> parse_number_pair(OnErr onErr, const char* str) {
  bool erred = false;
  auto local_on_err = [&]() {
    erred = true;
    onErr();
  };

  T first = parse_number<T>(local_on_err, str);
  if (erred) {
    return {0, 0};
  }
  while (1) {
    char ch = *str;
    if (ch == '\0') {
      return {first, std::nullopt};
    } else if (ch == ',') {
      ++str;
      break;
    } else if (!in(ch, '0', '9') && ch != '+' && ch != '-') {
      onErr();
      return {0, 0};
    }
    ++str;
  }
  T second = parse_number<T>(local_on_err, str);
  if (erred) {
    return {0, 0};
  }
  return {first, second};
}

} // namespace num
} // namespace choose
