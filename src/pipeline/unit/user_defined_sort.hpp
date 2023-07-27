#pragma once

#include <set>
#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct UserDefinedSortUnit : public AccumulatingUnit {
  regex::code comp;
  regex::match_data data;

  UserDefinedSortUnit(NextUnit&& next, regex::code&& comp)
      : AccumulatingUnit(std::move(next)), //
        comp(std::move(comp)),
        data(regex::create_match_data(this->comp)) {}
  
  // hook onto after the packets have been received but before they are sent to the next unit
  void process(EndOfStream&& p) override {
    auto comp = [this](const SimplePacket& lhs, const SimplePacket& rhs) -> bool { //
      int lhs_result = regex::match(this->comp, lhs.t.buffer.data(), lhs.t.buffer.size(), this->data, "user comp");
      int rhs_result = regex::match(this->comp, rhs.t.buffer.data(), rhs.t.buffer.size(), this->data, "user comp");
      if (lhs_result && !rhs_result) {
        return 1;
      } else {
        return 0;
      }
    };
    std::sort(this->packets.begin(), this->packets.end(), comp);
    AccumulatingUnit::process(std::move(p));
  }
};

struct UncompiledUserDefinedSortUnit : public UncompiledPipelineUnit {
  const char* comp;
  PipelineUnit compile(NextUnit&& next, uint32_t regex_options) override {
    regex::code code = regex::compile(this->comp, regex_options, "user defined comparison");
    return UserDefinedSortUnit(std::move(next), std::move(code));
  }
};

} // namespace pipeline
} // namespace choose
