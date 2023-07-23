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
  PipelineUnit(NextUnit&& next) : next(std::move(next)) {}
  virtual void process(Packet&& p) = 0;

  bool passthrough_end_of_stream(Packet& p) {
    if (std::holds_alternative<EndOfStream>(p)) {
      if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
        (*next_unit)->process(std::move(p));
        return true;
      } else {
        TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
        os.finish_output();
        throw termination_request();
      }
    }
    return false;
  }
};


} // namespace pipeline
} // namespace choose
