#pragma once

#include <set>
#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct Sort : public PipelineUnit {
    std::vector<
//   std::vector<Packet> packets;

  Sort(NextUnit&& next) : PipelineUnit(std::move(next)) {}

  void process(EndOfStream&& p) override {
    auto comp = [](const Packet& lhs_arg, const Packet& rhs_arg) -> bool {
      auto lhs = ViewPacket(lhs_arg);
      auto rhs = ViewPacket(rhs_arg);
      return std::lexicographical_compare(lhs.begin, lhs.end, rhs.begin, rhs.end);
    };
    std::sort(this->packets.begin(), this->packets.end(), comp);
  }

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

struct UncompiledSort : public UncompiledPipelineUnit {
  const char* comp;
  PipelineUnit compile(NextUnit&& next, uint32_t regex_options) override {
    regex::code code = regex::compile(this->comp, regex_options, "user defined comparison");
    return Sort(std::move(next), std::move(code));
  }
};

} // namespace pipeline
} // namespace choose
