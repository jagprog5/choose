#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct SubUnit : public BulkUnit {
  regex::code target;
  regex::match_data data;
  const char* replacement;
  regex::SubstitutionContext ctx;

  SubUnit(NextUnit&& next, regex::code&& target, const char* replacement)
      : BulkUnit(std::move(next)), //
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

  template <typename PacketT>
  void internal_process(PacketT&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      ViewPacket v(p);
      auto direct_apply_sub = [&](FILE* out, const char* begin, const char* end) { //
          this->direct_apply(out, begin, end);
      };
      os->write_output(v.begin, v.end, direct_apply_sub);
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      SimplePacket next_packet;
      this->apply(next_packet.t.buffer, v.begin, v.end);
      next_unit->process(std::move(next_packet));
    }
  }

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }

  void process(BulkPacket&& p) {
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      for (SimplePacket& sp : p) {
        auto direct_apply_index = [&](FILE* out, const char* begin, const char* end) { //
          this->direct_apply(out, begin, end);
        };
        os->write_output(&*sp.t.buffer.cbegin(), &*sp.t.buffer.cend(), direct_apply_index);
      }
      os->finish_output();
      throw termination_request();
    } else {
      for (SimplePacket& sp : p) {
        apply(sp.t.buffer, &*sp.t.buffer.cbegin(), &*sp.t.buffer.cend());
      }
      this->process_bulk_packet_for_next_unit(std::move(p));
    }
  }
};

struct UncompiledSubUnit : public UncompiledPipelineUnit {
  const char* target;
  const char* replacement;
  PipelineUnit compile(NextUnit&& next, uint32_t regex_options) override {
    return SubUnit(std::move(next), regex::compile(this->target, regex_options, "substitute"), this->replacement);
  }
};

} // namespace pipeline
} // namespace choose
