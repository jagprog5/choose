#pragma once

#include "pipeline/unit/unit.hpp"

#include <limits>
#include <unordered_set>

namespace choose {
namespace pipeline {

struct Unique : public BulkUnit {
  // indirect points to elements in this->packets unless it is the max value, which is reserved
  // for a candidate packet which has not yet been added to this->packets
  using indirect = BulkPacket::size_type;
  BulkPacket packets;

  // a candidate packet that might be added to this->packets
  const char* candidate_begin = NULL;
  size_t candidate_size = 0;

  std::string_view from_val(indirect val) const {
    const char* begin = val != std::numeric_limits<indirect>::max() //
                            ? &*this->packets[val].t.buffer.cbegin()
                            : this->candidate_begin;
    size_t size = val != std::numeric_limits<indirect>::max() //
                      ? this->packets[val].t.buffer.size()
                      : this->candidate_size;
    return {begin, size};
  }

  size_t unordered_set_hash(indirect val) const { //
    return std::hash<std::string_view>{}(this->from_val(val));
  }

  bool unordered_set_equals(indirect lhs, indirect rhs) const {
    std::string_view lv = this->from_val(lhs);
    std::string_view rv = this->from_val(rhs);
    return std::lexicographical_compare(lv.begin(), lv.end(), rv.begin(), rv.end());
  }

  using unordered_set_T = std::unordered_set<indirect, decltype(unordered_set_hash), decltype(unordered_set_equals)>;

  unordered_set_T unique_checker = []() -> unordered_set_T {
    auto ret = unordered_set_T(8, unordered_set_hash, unordered_set_equals);
    ret.max_load_factor(0.125); // determined from perf.md
  }();

  bool is_input_sorted = false;

  // unique is special.
  // if the next unit is an accumulating unit (effectively blocking the overall output) then
  //    each element is accumulated and sent as a bulk packet at the end
  // else
  //    each element is sent individually to the next unit
  bool next_unit_is_accumulating = false;

  Unique(NextUnit&& next) : BulkUnit(std::move(next)) {
    if (std::unique_ptr<PipelineUnit>* next_unit = std::get_if<std::unique_ptr<PipelineUnit>>(&this->next)) {
      if (AccumulatingUnit* b = dynamic_cast<AccumulatingUnit*>(next_unit->get())) {
        this->next_unit_is_accumulating = true;
      }
    }
  }

  void process(BulkPacket&& p) override {
    // TODO apply unique
    BulkUnit::process(std::move(p));
  }

  void process(EndOfStream&& p) override {
    this->unique_checker.clear();
    if (this->next_unit_is_accumulating) {
      BulkUnit::process(std::move(this->packets));
    } else {
      PipelineUnit::process(std::move(p));
    }
  }

  template <typename PacketT>
  void internal_process(PacketT&& p) {
    // not guarding for this->is_input_sorted out of per token overhead worry
    indirect val;
    if constexpr (std::is_same_v<SimplePacket, PacketT>) {
      this->packets.push_back(std::move(p));
      val = this->packets.size() - 1;
    } else {
      ViewPacket view(p);
      this->candidate_begin = view.begin;
      this->candidate_size = view.end - view.begin;
      val = std::numeric_limits<indirect>::max();
    }

    std::pair<unordered_set_T::iterator, bool> result = this->unique_checker.insert(val);
    if (!result.second) {
      // the insertion did not take place, meaning the element already existed
      if constexpr (std::is_same_v<SimplePacket, PacketT>) {
        this->packets.pop_back();
      }
    } else {
      // the insertion did take place
      if constexpr (std::is_same_v<SimplePacket, PacketT>) {
        ViewPacket vp(this->packets[val]);
        if (!next_unit_is_accumulating) {
          BulkUnit::process(std::move(vp));
        }
      } else {
        // promote the candidate token to a token
        SimplePacket sp(p);
        this->packets.push_back(std::move(sp));
        const_cast<indirect*>(&*result.first) = this->packets.size() - 1;
        if (!next_unit_is_accumulating) {
          // pass along a view
          BulkUnit::process(std::move(p));
        }
      }
    }
  }

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }
};

} // namespace pipeline
} // namespace choose
