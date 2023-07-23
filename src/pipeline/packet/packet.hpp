#pragma once

#include <variant>
#include "pipeline/packet/token.hpp"

namespace choose {

namespace pipeline {

struct EndOfStream {
  char unused[0]; // make the struct zero length. cpp quirk
};

// todo!
// non-owning view to persistent memory
struct StoredPacket {
  const char* begin;
  const char* end;  
};

// this is a simple packet which hold ownership over a vector
struct SimplePacket {
  Token t;

  SimplePacket(std::vector<char>&& v) : t{std::move(v)} {}
  
  // for safety, delete copying. shouldn't ever happen
  SimplePacket(const SimplePacket& o) = delete;
  SimplePacket& operator=(const SimplePacket&) = delete;

  // create copy from other types of packets before modifying
  SimplePacket(ViewPacket&& p) : t{std::vector<char>(p.begin, p.end)} {}
  SimplePacket(StoredPacket&& p) : t{std::vector<char>(p.begin, p.end)} {}
};

// non owning view of temporary memory.
// the memory could exist in the match buffer and will go away on next match iteration.
// so this can be passed down the pipeline but should not be held anywhere
struct ViewPacket {
  const char* begin;
  const char* end;
  // view packet can take a view of the other packet types
  ViewPacket(const SimplePacket& sp) : begin(&*sp.t.buffer.cbegin()), end(&*sp.t.buffer.cend()) {}
  ViewPacket(const StoredPacket& sp) : begin(sp.begin), end(sp.end) {}
};

// like a ViewPacket but with more information.
// it must exclusively be used on the input of ReplaceUnit.
struct ReplacePacket {
  const char* subj_begin;
  const char* subj_end;
  const regex::match_data& data;
  const regex::code& re;
};

} // namespace pipeline
} // namespace choose
