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

struct PipelineUnit {
  NextUnit next;
  PipelineUnit(NextUnit&& next) : next(std::move(next)) {}

  void throw_not_implemented() { throw std::runtime_error("processing this packet type is not implemented"); }

  // default behaviour is to pass EndOfStream to the next unit
  virtual void process(EndOfStream&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      os->finish_output();
      throw termination_request();
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      next_unit->process(std::move(p));
    }
  };

  virtual void process(StoredPacket&& p) { throw_not_implemented(); };
  virtual void process(SimplePacket&& p) { throw_not_implemented(); };
  virtual void process(ViewPacket&& p) { throw_not_implemented(); };
  virtual void process(ReplacePacket&& p) { throw_not_implemented(); };
};

// needed for some of the Units. holding spot while args are parsed
struct UncompiledPipelineUnit {
  virtual PipelineUnit compile(NextUnit&& next, uint32_t regex_options) = 0;
};

} // namespace pipeline
} // namespace choose
