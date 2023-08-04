#pragma once

#include <vector>
#include <string.h>

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

}
