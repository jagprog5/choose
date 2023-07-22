#pragma once

#include <memory>
#include <variant>
#include "cassert"
#include "pipeline/packet/token.hpp"

namespace choose {

namespace pipeline {

using EndOfStream = std::monostate;
struct ViewPacket;
struct SimplePacket;
struct StoredPacket;
// the data flows through the pipeline in these packets
using Packet = std::variant<EndOfStream, ViewPacket, SimplePacket, StoredPacket>;

// non owning temporary view of memory
struct ViewPacket {
  const char* begin;
  const char* end;

  static ViewPacket fromPacket(const Packet& p);
};

// owned copy from a ViewPacket
struct SimplePacket {
  Token t;
  SimplePacket(ViewPacket ve) : t{std::vector<char>(ve.begin, ve.end)} {}
};

// this points to an element that is currently in use by one of the ordered ops.
// for example, after applying uniqueness, this element is still being held.
using StoredPacket = std::shared_ptr<Token>;

ViewPacket ViewPacket::fromPacket(const Packet& p) {
  assert(!std::holds_alternative<EndOfStream>(p));
  if (const ViewPacket* vp = std::get_if<ViewPacket>(&p)) {
    return *vp;
  } else if (const SimplePacket* sp = std::get_if<SimplePacket>(&p)) {
    return {&*sp->t.buffer.cbegin(), &*sp->t.buffer.cend()};
  } else {
    const Token* tp = std::get<StoredPacket>(p).get();
    return {&*tp->buffer.cbegin(), &*tp->buffer.cend()};
  }
}

} // namespace pipeline
} // namespace choose
