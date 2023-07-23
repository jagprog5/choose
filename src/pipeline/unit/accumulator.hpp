#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

template<typename CompletionHandler> // e.g. decltype(functionNameHere)
struct AccumulatorUnit : public PipelineUnit {
  std::vector<Packet> packets;

  AccumulatorUnit() : PipelineUnit(NextUnit(NULL))

  void process(Packet&& p) override {
    if (std::holds_alternative<EndOfStream>(p)) {
      CompletionHandler()(std::move(this->packets));
      // completion handler should handle the packets then exit the program.
      // this should never be reached
      assert(false);
    }
    if (ViewPacket* vp = std::get_if<ViewPacket>(p)) {
      p = SimplePacket(*vp);
    }

    // the packets accumulated will have the memory stored
    assert(!std::holds_alternative<EndOfStream>(p));
    assert(!std::holds_alternative<ViewPacket>(p));
    this->packets.push_back(std::move(p));
  }

};

} // namespace pipeline
} // namespace choose
