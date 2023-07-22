#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct Head : public PipelineUnit {
  size_t n;

  Head(NextUnit next, size_t n) : PipelineUnit(std::move(next)), n(n) {
    if (this->n == 0) {
      this->process(EndOfStream());
    }
  }

  void process(Packet&& s) override {
    if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
      (*next_unit)->process(std::move(s));
    } else {
      TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
      // TODO LIST
      // first this
      // then convert the other ordered ops
      // then do the uncompiled ops in arg folder
      // then ...
    }

    if (--this->n == 0) {
      this->process(EndOfStream());
    }

  }
}

} // namespace pipeline
} // namespace choose