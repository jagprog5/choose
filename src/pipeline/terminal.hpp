#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

struct ready_for_ui : public std::exception {};

// this is a last unit in the pipeline. either this or a TokenOutputStream is used at the end
struct TerminalUnit : public AccumulatingUnit {
  using output_t = decltype(AccumulatingUnit::packets);
  output_t& output;
  TerminalUnit(output_t& output) : AccumulatingUnit(NextUnit()), output(output) {}

  void process(EndOfStream&&) override {
    output = std::move(this->packets);
    throw ready_for_ui();
  }
};

} // namespace pipeline
} // namespace choose
