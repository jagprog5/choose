#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

template<typename OnCompletion> // e.g. decltype(functionNameHere)
struct Accumulator : public PipelineUnit {
  std::vector<Packet> packets;

  void process(Packet&& p) override {
    if (std::holds_alternative<EndOfStream>(p)) {
      OnCompletion()();
      this->packets.clear();
      return;
    }
    // todo make not View
    this->packets.push_back(p);
  }

};

} // namespace pipeline
} // namespace choose