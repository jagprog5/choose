#pragma once

#include "pipeline/unit.hpp"

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
    PipelineUnit::process(std::move(p));
    if (--this->n == 0) {
      PipelineUnit::process(EndOfStream());
    }
  }

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }
};

struct UncompiledHeadUnit : public UncompiledPipelineUnit {
  const size_t n;
  UncompiledHeadUnit(size_t n) : n(n) {}
  PipelineUnit compile(NextUnit&& next, uint32_t) override {
    return HeadUnit(std::move(next), this->n);
  }
};

} // namespace pipeline
} // namespace choose
