#pragma once

#include "pipeline/packet/packet.hpp"
#include "pipeline/unit/token_output_stream.hpp"

namespace choose {
namespace pipeline {

// this is a unit of transformation that processes PipelineElements
struct PipelineUnit;

// the next unit of the pipeline points to another unit.
// if it is the last element of the pipeline, it instead points to a TokenOutputStream
using NextUnit = std::variant<std::unique_ptr<PipelineUnit>, TokenOutputStream>;

struct PipelineUnit {
  NextUnit next;
  PipelineUnit(NextUnit next) : next(std::move(next)) {}
  virtual void process(Packet&& s) = 0;
};

} // namespace pipeline
} // namespace choose
