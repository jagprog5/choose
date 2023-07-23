#pragma once

#include "pipeline/unit/unit.hpp"

namespace choose {
namespace pipeline {

struct IndexUnit : public PipelineUnit {
  enum Align { BEFORE, AFTER };
  Align align;
  size_t index = 0;

  IndexUnit(NextUnit&& next, Align align) //
      : PipelineUnit(std::move(next)), align(align) {}
  
  // bytes needed (without null char) for ascii base 10 representation of uint
  static constexpr size_t space_required(size_t value) { return value == 0 ? 1 : (size_t(std::log10(value)) + 1); }

  // out_index is the number of tokens that have already been written to the output
  // places the ascii base 10 representation onto the beginning or end of a vector
  void apply(std::vector<char>& v, size_t out_index) {
    size_t extension = IndexUnit::space_required(index);
    extension += 1; // +1 for space
    v.resize(v.size() + extension);

    if (this->align == IndexOp::BEFORE) {
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

    if (!this->is_output_index_op()) {
      ++this->in_index;
    }
  }
  
  void process(Packet&& p) override {
    if (this->passthrough_end_of_stream(p)) return;


  }

};

}
}
