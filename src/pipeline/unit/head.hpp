#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct HeadUnit : public PipelineUnit {
  size_t n;

  HeadUnit(NextUnit&& next, size_t n) : PipelineUnit(std::move(next)), n(n) {
    if (this->n == 0) {
      this->process(EndOfStream());
    }
  }

  void process(Packet&& p) override {
    if (this->passthrough_end_of_stream(p)) return;

    if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
      (*next_unit)->process(std::move(p));
    } else {
      TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
      ViewPacket view = ViewPacket::fromPacket(p);
      os.write_output(view.begin, view.end);

      // TODO LIST
      // then convert the other ordered ops
      // then do the uncompiled ops in arg folder
    }

    if (--this->n == 0) {
      this->process(EndOfStream());
    }

  }
};

} // namespace pipeline
} // namespace choose