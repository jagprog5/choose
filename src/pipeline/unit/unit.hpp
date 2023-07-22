#pragma once

#include "pipeline/packet/packet.hpp"
#include "pipeline/unit/token_output_stream.hpp"

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
  PipelineUnit(NextUnit next) : next(std::move(next)) {}

  void completion_guard(const Packet& p) {
    if (std::holds_alternative<EndOfStream>(p)) {
      if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
        // if this is the end of the stream and this is the last unit on the pipeline
        os->finish_output();
        throw termination_request();
      }
    }
  }

  virtual void process(Packet&& p) = 0;

};


} // namespace pipeline
} // namespace choose
