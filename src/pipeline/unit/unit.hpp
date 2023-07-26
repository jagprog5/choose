#pragma once

#include "pipeline/packet/packet.hpp"
#include "pipeline/unit/token_output_stream.hpp"
#include "variant"

namespace choose {
namespace pipeline {

// this is a unit of transformation that processes Packets
struct PipelineUnit;

// the next unit of the pipeline points to another unit.
// if it is the last element of the pipeline, it instead points to a TokenOutputStream
using NextUnit = std::variant<std::unique_ptr<PipelineUnit>, TokenOutputStream>;

// this is caught and treated like exit() unless this is a unit test
struct termination_request : public std::exception {};

// base class. the default behaviour is to pass everything to the next unit of the pipeline
struct PipelineUnit {
  NextUnit next;
  PipelineUnit(NextUnit&& next) : next(std::move(next)) {}

  virtual void process(EndOfStream&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      os->finish_output();
      throw termination_request();
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
};

// this is a type of pipeline unit that can handle BulkPackets.
// the default behaviour is to pass through appropriately.
// if a unit should only be able to handle individual packets at a time (not a bulk packet),
// then it should be a PipelineUnit, and not a BulkUnit
struct BulkUnit : public PipelineUnit {
  BulkUnit(NextUnit&& next) : PipelineUnit(std::move(next)) {}

  void process_bulk_packet_for_next_unit(BulkPacket&& p) {
    std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
    if (BulkUnit* b = dynamic_cast<BulkUnit*>(next_unit.get())) {
      b->process(std::move(p));
    } else {
      for (SimplePacket& sp : p) {
        next_unit->process(std::move(sp));
      }
      next_unit->process(EndOfStream());
    }
  }

  virtual void process(BulkPacket&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      for (const SimplePacket& sp : p) {
        os->write_output(&*sp.t.buffer.cbegin(), &*sp.t.buffer.cend());
      }
      os->finish_output();
      throw termination_request();
    } else {
      this->process_bulk_packet_for_next_unit(std::move(p));
    }
  }
};

// a BulkUnit that accumulates all of the tokens it receives.
struct AccumulatingUnit : public BulkUnit {
  BulkPacket packets;

  AccumulatingUnit(NextUnit&& next) : BulkUnit(std::move(next)) {}

  void process(BulkPacket&& p) override {
    this->packets = std::move(p);
    // for an AccumulatingUnit, end of stream is processed when the input is done,
    // regardless of if it came from a bulk packet or from individual packets
    this->process(EndOfStream());
  }

  void process(EndOfStream&& p) override {
    BulkUnit::process(std::move(this->packets));
  }

  void process(SimplePacket&& p) override {
    this->packets.push_back(std::move(p));
  }
  void process(ViewPacket&& p) override { this->process(SimplePacket(std::move(p))); }
  void process(ReplacePacket&& p) override { this->process(SimplePacket(std::move(p))); }
};

// needed for some of the Units. holding spot while args are parsed
struct UncompiledPipelineUnit {
  virtual PipelineUnit compile(NextUnit&& next, uint32_t regex_options) = 0;
};

} // namespace pipeline
} // namespace choose
