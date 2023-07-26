#pragma once

#include <variant>
#include "regex.hpp"
#include "pipeline/packet/token.hpp"

namespace choose {

namespace pipeline {

struct EndOfStream {
  char unused[0]; // make the struct zero length. cpp quirk
};

// this is a simple packet which hold ownership over a vector
struct SimplePacket {
  Token t;

  SimplePacket(std::vector<char>&& v) : t{std::move(v)} {}
  
  // for safety, delete copying. shouldn't ever happen
  SimplePacket(const SimplePacket&) = delete;
  SimplePacket& operator=(const SimplePacket&) = delete;
  SimplePacket(SimplePacket&&) = default;
  SimplePacket& operator=(SimplePacket&&) = default;

  SimplePacket(const ViewPacket& p) : t{std::vector<char>(p.begin, p.end)} {}
  SimplePacket(ViewPacket&& p) : t{std::vector<char>(p.begin, p.end)} {}

  SimplePacket(const ReplacePacket& rp) : SimplePacket(ViewPacket(rp)) {}
  SimplePacket(ReplacePacket&& rp) : SimplePacket(ViewPacket(rp)) {}
};

// like a ViewPacket but with more information. for the input of a ReplaceUnit
struct ReplacePacket {
  const char* subj_begin;
  const char* subj_end;
  const regex::match_data& data;
  const regex::code& re;
};

// non owning view of temporary memory.
// the memory could exist in the match buffer and will go away on next match iteration.
// so this can be passed down the pipeline but should not be held anywhere
struct ViewPacket {
  const char* begin;
  const char* end;
  // view packet can take a view of the other packet types
  ViewPacket(const SimplePacket& sp) : begin(&*sp.t.buffer.cbegin()), end(&*sp.t.buffer.cend()) {}

  ViewPacket(const ReplacePacket& rp) {
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(rp.data.get());
    this->begin = rp.subj_begin + ovector[0];
    this->end = rp.subj_begin + ovector[1];
  }
};

using BulkPacket = std::vector<SimplePacket>;

} // namespace pipeline
} // namespace choose
