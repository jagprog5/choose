#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

// this is a last unit in the pipeline. either this or a TokenOutputStream is used at the end
template<typename CompletionHandler> // e.g. decltype(functionNameHere)
struct TerminalUnit : public AccumulatingUnit {
  TerminalUnit() : AccumulatingUnit(NextUnit(NULL)) {}

  void process(EndOfStream&& p) override {
    CompletionHandler()(std::move(this->packets));
    // completion handler should handle the packets then exit the program.
    // this should never be reached
    assert(false);
  }
};

} // namespace pipeline
} // namespace choose
