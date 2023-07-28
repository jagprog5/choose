#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

struct pipeline_complete : public std::exception {
  std::vector<SimplePacket> packets;
  pipeline_complete(std::vector<SimplePacket>&& packets) : packets(std::move(packets)) {}
};

// this is a last unit in the pipeline. either this or a TokenOutputStream is used at the end
struct TerminalUnit : public AccumulatingUnit {
  using output_t = decltype(AccumulatingUnit::packets);
  TerminalUnit() : AccumulatingUnit(NextUnit()){}

  void process(EndOfStream&&) override {
    throw pipeline_complete(std::move(this->packets));
  }
};

} // namespace pipeline
} // namespace choose
