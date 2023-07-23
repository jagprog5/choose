#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

// this is a last unit in the pipeline. either this or a TokenOutputStream is used at the end
template<typename CompletionHandler> // e.g. decltype(functionNameHere)
struct AccumulatorUnit : public PipelineUnit {
  // only one of these vectors is used, depending on the upstream source
  std::vector<SimplePacket> simple_packets;
  std::vector<StoredPacket> stored_packets;

  AccumulatorUnit() : PipelineUnit(NextUnit(NULL)) {}

  void process(EndOfStream&& p) override {
    assert(this->simple_packets.empty() || this->stored_packets.empty());
    if (this->stored_packets.empty()) {
      CompletionHandler()(std::move(this->simple_packets));
    } else {

    }
    // if (this->simple_packets)
      // completion handler should handle the packets then exit the program.
      // this should never be reached
    assert(false);
  }

  void process(StoredPacket&& p) override {
    this->stored_packets.push_back(std::move(p));
  }

  void process(SimplePacket&& p) override {
    this->simple_packets.push_back(std::move(p));
  }

  void process(ViewPacket&& p) override {
    this->process(SimplePacket(std::move(p)));
  }
};

} // namespace pipeline
} // namespace choose
