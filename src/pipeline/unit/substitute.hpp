#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct SubUnit : public PipelineUnit {
  regex::code target;
  regex::match_data data;
  const char* replacement;
  regex::SubstitutionContext ctx;

  SubUnit(NextUnit&& next, regex::code&& target, const char* replacement)
      : PipelineUnit(std::move(next)), //
        target(std::move(target)),
        data(regex::create_match_data(this->target)),
        replacement(replacement) {}

  void apply(std::vector<char>& out, const char* begin, const char* end) { //
    out = regex::substitute_global(target, begin, end - begin, replacement, this->ctx);
  }

  // same as apply, but no copies or moves. sent straight to the output
  void direct_apply(FILE* out, const char* begin, const char* end) {
    const char* offset = begin;
    while (offset < end) {
      int rc = regex::match(this->target, begin, end - begin, this->data, "match before substitution", offset - begin, PCRE2_NOTEMPTY);
      if (rc <= 0) {
        break;
      }
      regex::Match match = regex::get_match(begin, this->data, "match before substitution");
      str::write_f(out, offset, match.begin);
      offset = match.end;
      std::vector<char> replacement = regex::substitute_on_match(this->data, this->target, begin, end - begin, this->replacement, this->ctx);
      str::write_f(out, replacement);
    }
    str::write_f(out, offset, end);
  }

  void process(Packet&& p) override {
    if (this->passthrough_end_of_stream(p)) return;

    ViewPacket v = ViewPacket::fromPacket(p);
    if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
        SimplePacket next_packet;
        this->apply(next_packet.t.buffer, v.begin, v.end);
        (*next_unit)->process(std::move(next_packet));
    } else {
        TokenOutputStream& os = std::get<TokenOutputStream>(this->next);
        auto direct_apply_sub = [&](FILE* out, const char* begin, const char* end) { //
            this->direct_apply(out, begin, end);
        };
        os.write_output(v.begin, v.end, direct_apply_sub);
    }

  }
};
} // namespace pipeline
} // namespace choose
