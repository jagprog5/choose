#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct IndexUnit : public PipelineUnit {
  enum Align { BEFORE, AFTER };
  const Align align;
  size_t index = 0;

  IndexUnit(NextUnit&& next, Align align) //
      : PipelineUnit(std::move(next)), align(align) {}
  
  // bytes needed (without null char) for ascii base 10 representation of uint
  static constexpr size_t space_required(size_t value) { return value == 0 ? 1 : (size_t(std::log10(value)) + 1); }

  // out_index is the number of tokens that have already been written to the output
  // places the ascii base 10 representation onto the beginning or end of a vector
  void apply(std::vector<char>& v) {
    size_t extension = IndexUnit::space_required(index);
    extension += 1; // +1 for space
    v.resize(v.size() + extension);

    if (this->align == IndexUnit::BEFORE) {
      // move the entire buffer forward
      char* to_ptr = &*v.rbegin();
      const char* from_ptr = &*v.rbegin() - extension;
      while (from_ptr >= &*v.begin()) {
        *to_ptr-- = *from_ptr--;
      }
      sprintf(&*v.begin(), "%zu", index);
      // overwrite the null written by sprintf
      *(v.begin() + (ptrdiff_t)(extension - 1)) = ' ';
    } else {
      char* ptr = &*v.end() - extension;
      *ptr++ = ' ';
      size_t without_last = index / 10;
      // index when divided by 10 must take one fewer byte
      if (without_last != 0) { // aka greater than 9
        sprintf(ptr, "%zu", without_last);
      }
      // overwrite the null written by sprintf
      // NOLINTNEXTLINE narrowing to char is ok for index in range [0-9]
      *v.rbegin() = (char)(index - without_last * 10) + '0';
    }

    ++this->index;
  }

  // same as apply, but sent straight to the output. no copies or moves used
  void direct_apply(FILE* out, const char* begin, const char* end) {
    if (this->align == IndexUnit::BEFORE) {
      fprintf(out, "%zu ", index);
    }
    str::write_f(out, begin, end);
    if (this->align != IndexUnit::BEFORE) {
      fprintf(out, " %zu", index);
    }
    ++this->index;
  }

  template <typename PacketT>
  void internal_process(PacketT&& p) {
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

  void process(SimplePacket&& p) override { this->internal_process(std::move(p)); }
  void process(ViewPacket&& p) override { this->internal_process(std::move(p)); }
  void process(ReplacePacket&& p) override { this->internal_process(std::move(p)); }
};

} // namespace pipeline
} // namespace choose
