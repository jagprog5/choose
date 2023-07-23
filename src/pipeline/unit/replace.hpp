#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct ReplaceUnit : public PipelineUnit {
  const char* replacement;
  regex::SubstitutionContext ctx;
  ReplaceUnit(NextUnit&& next, const char* replacement) //
      : PipelineUnit(std::move(next)), replacement(replacement) {}
  
  void process(Packet&& p) override {
    if (this->passthrough_end_of_stream(p)) return;
    ReplacePacket& rp = std::get<ReplacePacket>(p);
    std::vector<char> out = regex::substitute_on_match(rp.data, rp.re, rp.subj_begin, rp.subj_end - rp.subj_begin, this->replacement, this->ctx);
    if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
        SimplePacket next_packet;
        next_packet.t.buffer = std::move(out);
        (*next_unit)->process(std::move(next_packet));
    } else {
        TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
        os.write_output(&*out.cbegin(), &*out.cend());
    }
  }
};

} // namespace pipeline
} // namespace choose
