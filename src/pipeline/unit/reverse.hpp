#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct ReverseUnit : public AccumulatingUnit {
  ReverseUnit(NextUnit&& next) : AccumulatingUnit(std::move(next)) {}

  void process(EndOfStream&& p) override {
    std::reverse(this->packets.begin(), this->packets.end());
    AccumulatingUnit::process(std::move(p));
  }
};

} // namespace pipeline
} // namespace choose
