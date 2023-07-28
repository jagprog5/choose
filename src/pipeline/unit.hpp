#pragma once

#include "pipeline/packet.hpp"
#include "pipeline/token_output_stream.hpp"

struct TokenOutputStream;

namespace choose {
namespace pipeline {

// this is a unit of transformation that processes Packets
struct PipelineUnit;

// the next unit of the pipeline points to another unit.
// the last element of the pipeline can be a TokenOutputStream
using NextUnit = std::variant<std::unique_ptr<PipelineUnit>, TokenOutputStream>;

// this is caught and treated like exit() unless this is a unit test
struct output_finished : public std::exception {};

// base class. the default behaviour is to pass everything to the next unit of the pipeline
struct PipelineUnit {
  NextUnit next;
  PipelineUnit() = default;
  PipelineUnit(NextUnit&& next) : next(std::move(next)) {}

  PipelineUnit(PipelineUnit&& o) = default;
  PipelineUnit& operator=(PipelineUnit&&) = default;

  virtual void process(EndOfStream&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      os->finish_output();
      throw output_finished();
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      next_unit->process(std::move(p));
    }
  };

  template <typename PacketT>
  void internal_process(PacketT&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      ViewPacket view(p);
      os->write_output(view.begin, view.end);
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      next_unit->process(std::move(p));
    }
  }

  virtual void process(SimplePacket&& p) { this->internal_process(std::move(p)); }
  virtual void process(ViewPacket&& p) { this->internal_process(std::move(p)); }
  virtual void process(ReplacePacket&& p) { this->internal_process(std::move(p)); }

  virtual ~PipelineUnit() {}
};

// a PipelineUnit that accumulates all of the tokens it receives.
struct AccumulatingUnit : public PipelineUnit {
  std::vector<SimplePacket> packets;

  AccumulatingUnit(NextUnit&& next) : PipelineUnit(std::move(next)) {}

  void process_stream_for_next_unit() {
    std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
    for (SimplePacket& sp : this->packets) {
      next_unit->process(std::move(sp));
    }
    this->packets.clear();
    next_unit->process(EndOfStream());
  }

  void process(EndOfStream&&) override {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      for (const SimplePacket& sp : this->packets) {
        os->write_output(&*sp.buffer.cbegin(), &*sp.buffer.cend());
      }
      os->finish_output();
      throw output_finished();
    } else {
      this->process_stream_for_next_unit();
    }
  }

  void process(SimplePacket&& p) override { this->packets.push_back(std::move(p)); }
  void process(ViewPacket&& p) override { this->process(SimplePacket(std::move(p))); }
  void process(ReplacePacket&& p) override { this->process(SimplePacket(std::move(p))); }
};

// needed for some of the Units. holding spot while args are parsed
struct UncompiledPipelineUnit {
  virtual std::unique_ptr<PipelineUnit> compile(NextUnit&& next, [[maybe_unused]] uint32_t regex_options) = 0;
  virtual ~UncompiledPipelineUnit() {}
};

} // namespace pipeline
} // namespace choose
