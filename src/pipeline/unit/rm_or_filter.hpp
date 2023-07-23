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

  void process(Packet&& p) override {
    if (this->passthrough_end_of_stream(p)) return;

    const char* id = this->type == RmOrFilterUnit::REMOVE ? "remove" : "filter";
    ViewPacket v = ViewPacket::fromPacket(p);
    int rc = regex::match(this->arg, v.begin, v.end - v.begin, this->match_data, id);

    auto send_to_next = [&]() {
      if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
        (*next_unit)->process(std::move(p));
      } else {
        TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
        os.write_output(v.begin, v.end);
      }
    };

    if (rc > 0) {
      // there was a match
      if (this->type == RmOrFilterUnit::REMOVE) {
        send_to_next();
      }
    } else {
      // there was no match
      if (this->type == RmOrFilterUnit::FILTER) {
        send_to_next();
      }
    }
  }
};

} // namespace pipeline
} // namespace choose
