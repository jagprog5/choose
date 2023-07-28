#pragma once

#include "pipeline/unit.hpp"

#include <limits>
#include <unordered_set>
#include <functional>

namespace choose {
namespace pipeline {

struct UniqueUnit : public PipelineUnit {
  // indirect points to elements in this->packets unless it is the max value, which is reserved
  // for a candidate packet which has not yet been added to this->packets
  using indirect = std::vector<SimplePacket>::size_type;
  std::vector<SimplePacket> packets;

  // a candidate packet that might be added to this->packets
  const char* candidate_begin = NULL;
  size_t candidate_size = 0;

  std::string_view from_val(indirect val) const {
    const char* begin = val != std::numeric_limits<indirect>::max() //
                            ? &*this->packets[val].buffer.cbegin()
                            : this->candidate_begin;
    size_t size = val != std::numeric_limits<indirect>::max() //
                      ? this->packets[val].buffer.size()
                      : this->candidate_size;
    return {begin, size};
  }

  size_t unordered_set_hash_member(indirect val) const { //
    return std::hash<std::string_view>{}(this->from_val(val));
  }

  static std::function<size_t(indirect)> unordered_set_hash(const UniqueUnit* me) {
    return std::bind(&UniqueUnit::unordered_set_hash_member, me, std::placeholders::_1);
  }

  bool unordered_set_equals_member(indirect lhs, indirect rhs) const {
    std::string_view lv = this->from_val(lhs);
    std::string_view rv = this->from_val(rhs);
    return std::lexicographical_compare(lv.begin(), lv.end(), rv.begin(), rv.end());
  }

  static std::function<size_t(indirect, indirect)> unordered_set_equals(const UniqueUnit* me) {
    return std::bind(&UniqueUnit::unordered_set_equals_member, me, std::placeholders::_1, std::placeholders::_2);
  }

  // no copies or moves. as a safety measure, since std::bind is used above and take a copy of "this" on construction
  // pointer would get invalidated
  UniqueUnit(const UniqueUnit& o) = delete;
  UniqueUnit& operator=(const UniqueUnit&) = delete;
  UniqueUnit(UniqueUnit&& o) = delete;
  UniqueUnit& operator=(UniqueUnit&&) = delete;

  using unordered_set_T = std::unordered_set<indirect, decltype(unordered_set_hash(NULL)), decltype(unordered_set_equals(NULL))>;

  unordered_set_T create_unique_checker() {
    auto ret = unordered_set_T(8, unordered_set_hash(this), unordered_set_equals(this));
    ret.max_load_factor(0.125); // determined from perf.md
    return ret;
  }

  unordered_set_T unique_checker = create_unique_checker();

  UniqueUnit(NextUnit&& next) : PipelineUnit(std::move(next)) {}

  template <typename PacketT>
  void internal_process(PacketT&& p) {
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
        PipelineUnit::process(std::move(vp));
      } else {
        // promote the candidate token to a token
        SimplePacket sp(p);
        this->packets.push_back(std::move(sp));
        const_cast<unordered_set_T::value_type&>(*result.first) = this->packets.size() - 1;
        // pass along a view
        PipelineUnit::process(std::move(p));
      }
    }
  }

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }
};

struct UncompiledUniqueUnit : public UncompiledPipelineUnit {
  PipelineUnit compile(NextUnit&& next, uint32_t) override {
    return UniqueUnit(std::move(next));
  }
};

} // namespace pipeline
} // namespace choose
