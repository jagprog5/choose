#pragma once
#include <algorithm>
#include <cassert>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "args.hpp"
#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

// Token is a thin wrapper around vector<char>. provides type clarity
struct Token {
  std::vector<char> buffer;

  // for testing
  Token(const char* in) : buffer(in, in + strlen(in)) {}
  bool operator==(const Token& other) const { return this->buffer == other.buffer; }

  Token(std::vector<char>&& i) : buffer(std::move(i)){};
  Token() = default;
  Token(const Token&) = default;
  Token(Token&&) = default;
  Token& operator=(const Token&) & = default;
  Token& operator=(Token&&) & = default;
  ~Token() = default;
};

using indirect = std::vector<Token>::size_type; // an index into output

} // namespace choose
