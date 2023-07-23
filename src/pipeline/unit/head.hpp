#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct HeadUnit : public PipelineUnit {
  size_t n;

  HeadUnit(NextUnit&& next, size_t n) : PipelineUnit(std::move(next)), n(n) {
    if (this->n == 0) {
      PipelineUnit::process(EndOfStream());
    }
  }

  template <typename PacketT>
  void internal_process(PacketT&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      ViewPacket view = ViewPacket::fromPacket(p);
      os->write_output(view.begin, view.end);
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      next_unit.process(std::move(p));
    }

    if (--this->n == 0) {
      this->process(EndOfStream());
    }
  }

  void process(StoredPacket&& p) override { this->internal_process(std::move(p)); }
  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
};

} // namespace pipeline
} // namespace choose
