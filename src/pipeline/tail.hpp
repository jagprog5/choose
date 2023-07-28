#pragma once

#include <deque>
#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

struct TailUnit : public PipelineUnit {
  std::deque<SimplePacket> packets;
  size_t n;

  TailUnit(NextUnit&& next, size_t n) : PipelineUnit(std::move(next)), n(n) {
    if (this->n == 0) {
      PipelineUnit::process(EndOfStream());
    }
  }

  void process(EndOfStream&& p) override {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      for (const SimplePacket& sp : this->packets) {
        os->write_output(&*sp.buffer.cbegin(), &*sp.buffer.cend());
      }
      os->finish_output();
      throw output_finished();
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      for (SimplePacket& sp : this->packets) {
        next_unit->process(std::move(sp));
      }
      this->packets.clear();
      next_unit->process(std::move(p));
    }
  }

  void process(SimplePacket&& p) override {
    if (this->packets.size() == n) {
      this->packets.erase(this->packets.begin());
    }
    this->packets.push_back(std::move(p));
  }

  void process(ViewPacket&& p) override { this->process(SimplePacket(std::move(p))); }
  void process(ReplacePacket&& p) override { this->process(SimplePacket(std::move(p))); }
};

struct UncompiledTailUnit : public UncompiledPipelineUnit {
  const size_t n;
  UncompiledTailUnit(size_t n) : n(n) {}
  std::unique_ptr<PipelineUnit> compile(NextUnit&& next, uint32_t) override {
    return std::unique_ptr<PipelineUnit>(new TailUnit(std::move(next), this->n));
  }
};

} // namespace pipeline
} // namespace choose
