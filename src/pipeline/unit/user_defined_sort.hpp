#pragma once

#include <set>
#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct UserDefinedSortUnit : public PipelineUnit {
  regex::code comp;
  regex::match_data data;

  bool comp(const StoredPacket& lhs, const StoredPacket& rhs) {
    int lhs_result = regex::match(this->comp, lhs->buffer.data(), lhs->buffer.size(), this->data, "user comp");
    int rhs_result = regex::match(this->comp, rhs->buffer.data(), rhs->buffer.size(), this->data, "user comp");
    if (lhs_result && !rhs_result) {
      return 1;
    } else {
      return 0;
    }
  }

  UserDefinedSortUnit(NextUnit&& next, regex::code&& comp)
      : PipelineUnit(std::move(next)), //
        comp(std::move(comp)),
        data(regex::create_match_data(this->comp)) {}
  
  template <typename PacketT>
  void internal_process(PacketT&& p) {
    StoredPacket stored; // convert the input packet into a stored packet
    if (ViewPacket* vp = std::get_if<ViewPacket>(&p)) {
      SimplePacket simple(*vp);
      stored = std::make_shared<Token>(std::move(simple.t));
    } else if (SimplePacket* sp = std::get_if<SimplePacket>(&p)) {
      stored = std::make_shared<Token>(std::move(sp->t));
    } else {
      StoredPacket& same = std::get<StoredPacket>(p);
      stored = std::move(same);
    }

    //                        V copy of shared ptr
    if (uniqueness_set.insert(stored).second) {
      // element was unique
        if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
            Packet out = std::move(stored);
            (*next_unit)->process(std::move(stored));
        } else {

        }
    }
    // TODODOOO

    if (TokenOutputStream* os = std::get_if<TokenOutputStream>(&this->next)) {
      ViewPacket v(p);
      auto direct_apply_index = [&](FILE* out, const char* begin, const char* end) { //
        this->direct_apply(out, begin, end);
      };
      os->write_output(v.begin, v.end, direct_apply_index);
    } else {
      std::unique_ptr<PipelineUnit>& next_unit = std::get<std::unique_ptr<PipelineUnit>>(this->next);
      SimplePacket next_packet(std::move(p));
      this->apply(next_packet.t.buffer);
      next_unit->process(std::move(next_packet));
    }
  }

  void process(StoredPacket&& p) override { this->internal_process(std::move(p)); }
  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
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
