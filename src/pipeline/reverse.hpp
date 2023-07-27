#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

struct ReverseUnit : public AccumulatingUnit {
  ReverseUnit(NextUnit&& next) : AccumulatingUnit(std::move(next)) {}

  void process(EndOfStream&& p) override {
    std::reverse(this->packets.begin(), this->packets.end());
    AccumulatingUnit::process(std::move(p));
  }
};

struct UncompiledReverseUnit : public UncompiledPipelineUnit {
  PipelineUnit compile(NextUnit&& next, uint32_t) override {
    return ReverseUnit(std::move(next));
  }
};

} // namespace pipeline
} // namespace choose
