#pragma once

#include "pipeline/unit/unit.hpp"
#include "utils/regex.hpp"

namespace choose {
namespace pipeline {

struct RmOrFilterUnit : public PipelineUnit {
  enum Type { REMOVE, FILTER };
  Type type;
  regex::code arg;
  regex::match_data match_data;

  RmOrFilterUnit(NextUnit&& next, Type type, regex::code&& arg)
      : PipelineUnit(std::move(next)), //
        type(type),
        arg(std::move(arg)),
        match_data(regex::create_match_data(this->arg)) {}

  template <typename PacketT>
  bool denied(const PacketT& p) {
    const char* id = this->type == RmOrFilterUnit::REMOVE ? "remove" : "filter";
    ViewPacket v = ViewPacket(p);
    int rc = regex::match(this->arg, v.begin, v.end - v.begin, this->match_data, id);

    if (rc > 0) {
      // there was a match
      if (this->type == RmOrFilterUnit::FILTER) {
        return false;
      }
    } else {
      // there was no match
      if (this->type == RmOrFilterUnit::REMOVE) {
        return false;
      }
    }
    return true;
  }
  
  template <typename PacketT>
  void internal_process(PacketT&& p) {
    if (!this->denied(p)) {
      if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
        ViewPacket v = ViewPacket(p);
        os->write_output(v.begin, v.end);
      } else {
        std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
        next_unit->process(std::move(p));
      }
    }
  }

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }
};

struct UncompiledRmOrFilterUnit : public UncompiledPipelineUnit {
  RmOrFilterUnit::Type type;
  const char* arg;

  UncompiledRmOrFilterUnit(RmOrFilterUnit::Type type, const char* arg) : type(type), arg(arg) {}

  PipelineUnit compile(NextUnit&& next, uint32_t regex_options) override {
    const char* id = this->type == RmOrFilterUnit::FILTER ? "filter" : "remove";
    regex::code code = regex::compile(this->arg, regex_options, id);
    return RmOrFilterUnit(std::move(next), type, std::move(code));
  }
};

} // namespace pipeline
} // namespace choose
