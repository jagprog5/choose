#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct Sort : public AccumulatingUnit {
  Sort(NextUnit&& next) : AccumulatingUnit(std::move(next)) {
    // todo look ahead until head is found. do a partial sort here instead
  }

  // hook onto after the packets have been received but before they are sent to the next unit
  void process(EndOfStream&& p) override {
    auto comp = [](const SimplePacket& lhs, const SimplePacket& rhs) -> bool { //
      return std::lexicographical_compare(&*lhs.t.buffer.cbegin(), &*lhs.t.buffer.cend(), &*rhs.t.buffer.cbegin(), &*rhs.t.buffer.cend());
    };
    std::sort(this->packets.begin(), this->packets.end(), comp);
    AccumulatingUnit::process(std::move(p));
  }
};

} // namespace pipeline
} // namespace choose
