#pragma once

#include <string.h>
#include <variant>
#include "utils/regex.hpp"

namespace choose {

namespace pipeline {

// like a ViewPacket but with more information. for the input of a ReplaceUnit
struct ReplacePacket {
  const char* subj_begin;
  const char* subj_end;
  const regex::match_data& data;
  const regex::code& re;
};

struct SimplePacket;
struct ReplacePacket;

// non owning view of temporary memory.
// the memory could exist in the match buffer and will go away on next match iteration.
// so this can be passed down the pipeline but should not be held anywhere
struct ViewPacket {
  const char* begin;
  const char* end;
  ViewPacket(const char* begin, const char* end) : begin(begin), end(end) {}
  // view packet can take a view of the other packet types
  ViewPacket(const SimplePacket& sp);
  ViewPacket(const ReplacePacket& rp);
};

// this is a simple packet which hold ownership over a vector
struct SimplePacket {
  std::vector<char> buffer;

  // for testing
  SimplePacket(const char* in) : buffer(in, in + strlen(in)) {}
  bool operator==(const SimplePacket& other) const { return this->buffer == other.buffer; }

  SimplePacket() = default;
  SimplePacket(std::vector<char>&& v) : buffer{std::move(v)} {}

  // for safety, delete copying. shouldn't ever happen
  SimplePacket(const SimplePacket&) = delete;
  SimplePacket& operator=(const SimplePacket&) = delete;
  SimplePacket(SimplePacket&&) = default;
  SimplePacket& operator=(SimplePacket&&) = default;

  SimplePacket(const ViewPacket& p) : buffer{std::vector<char>(p.begin, p.end)} {}
  SimplePacket(ViewPacket&& p) : buffer{std::vector<char>(p.begin, p.end)} {}

  SimplePacket(const ReplacePacket& rp) : SimplePacket(ViewPacket(rp)) {}
  SimplePacket(ReplacePacket&& rp) : SimplePacket(ViewPacket(rp)) {}
};

inline ViewPacket::ViewPacket(const SimplePacket& sp) : begin(&*sp.buffer.cbegin()), end(&*sp.buffer.cend()) {}

inline ViewPacket::ViewPacket(const ReplacePacket& rp) {
  PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(rp.data.get());
  this->begin = rp.subj_begin + ovector[0];
  this->end = rp.subj_begin + ovector[1];
}

struct EndOfStream {
  std::vector<pipeline::SimplePacket>* out = 0;
};

} // namespace pipeline
} // namespace choose
