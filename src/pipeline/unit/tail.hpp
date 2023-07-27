#pragma once

#include <deque>
#include "pipeline/unit/unit.hpp"

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
        os->write_output(&*sp.t.buffer.cbegin(), &*sp.t.buffer.cend());
      }
      os->finish_output();
      throw termination_request();
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      for (SimplePacket& sp : this->packets) {
        next_unit->process(std::move(sp));
      }
      this->packets.clear();
      next_unit->process(EndOfStream());
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

} // namespace pipeline
} // namespace choose
