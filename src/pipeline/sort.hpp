#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

struct SortUnit : public AccumulatingUnit {
  SortUnit(NextUnit&& next) : AccumulatingUnit(std::move(next)) {
    // todo look ahead until head is found. do a partial sort here instead
  }

  // hook onto after the packets have been received but before they are sent to the next unit
  void process(EndOfStream&& p) override {
    auto comp = [](const SimplePacket& lhs, const SimplePacket& rhs) -> bool { //
      return std::lexicographical_compare(&*lhs.buffer.cbegin(), &*lhs.buffer.cend(), &*rhs.buffer.cbegin(), &*rhs.buffer.cend());
    };
    std::sort(this->packets.begin(), this->packets.end(), comp);
    AccumulatingUnit::process(std::move(p));
  }
};

struct UncompiledSortUnit : public UncompiledPipelineUnit {
  PipelineUnit compile(NextUnit&& next, uint32_t) override {
    return SortUnit(std::move(next));
  }
};

} // namespace pipeline
} // namespace choose
