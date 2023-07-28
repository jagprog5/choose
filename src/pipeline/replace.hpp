#pragma once

#include "pipeline/unit.hpp"

namespace choose {
namespace pipeline {

// like a substitution (target and replacement pair), but only does the replacement part.
// the target is instead done by the positional argument being matched
struct ReplaceUnit : public PipelineUnit {
  const char* replacement;
  regex::SubstitutionContext ctx;
  ReplaceUnit(NextUnit&& next, const char* replacement) //
      : PipelineUnit(std::move(next)), replacement(replacement) {}
    
  void process(ReplacePacket&& rp) override {
    std::vector<char> out = regex::substitute_on_match(rp.data, rp.re, rp.subj_begin, rp.subj_end - rp.subj_begin, this->replacement, this->ctx);
    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      os->write_output(&*out.cbegin(), &*out.cend());
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      SimplePacket next_packet{std::move(out)};
      next_unit->process(std::move(next_packet));
    }
  }

  void wrong_type_err() {
    throw std::runtime_error("A replace unit can only be proceeded by simple pipeline units.");
  }

  void process(SimplePacket&&) override {
    wrong_type_err();
  }
  void process(ViewPacket&&) override {
    wrong_type_err();
  }
};

struct UncompiledReplaceUnit : public UncompiledPipelineUnit {
  const char* replacement;
  UncompiledReplaceUnit(const char* replacement) : replacement(replacement) {}
  std::unique_ptr<PipelineUnit> compile(NextUnit&& next, uint32_t) override {
    return std::unique_ptr<PipelineUnit>(new ReplaceUnit(std::move(next), this->replacement));
  }
};


} // namespace pipeline
} // namespace choose
