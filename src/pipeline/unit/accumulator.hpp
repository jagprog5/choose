#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

template<typename CompletionHandler> // e.g. decltype(functionNameHere)
struct Accumulator : public PipelineUnit {
  std::vector<Packet> packets;

  void process(Packet&& p) override {
    if (std::holds_alternative<EndOfStream>(p)) {
      CompletionHandler()(std::move(this->packets));
      // completion handler should handle the packets then exit the program.
      // this should never be reached
      assert(false);
    }
    // todo make not View
    this->packets.push_back(p);
  }

};

} // namespace pipeline
} // namespace choose