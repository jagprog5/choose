#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

// this is a last unit in the pipeline. either this or a TokenOutputStream is used at the end
struct TerminalUnit : public AccumulatingUnit {
  using output_t = decltype(AccumulatingUnit::packets);
  TerminalUnit() : AccumulatingUnit(NextUnit()){}

  void process(EndOfStream&& p) override {
    // out is allowed to be null in circumstances where the output would be empty
    if (p.out) {
      *p.out = std::move(this->packets);
    }
  }
};

} // namespace pipeline
} // namespace choose
