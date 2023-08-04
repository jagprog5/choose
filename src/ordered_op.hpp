#pragma once

#include <variant>
#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

struct RmOrFilterOp {
  enum Type { REMOVE, FILTER };
  Type type;
  regex::code arg;
  regex::match_data match_data;

  RmOrFilterOp(Type type, regex::code&& arg)
      : type(type), //
        arg(std::move(arg)),
        match_data(regex::create_match_data(this->arg)) {}

  // returns true iff the token should not pass to the output
  bool removes(const char* begin, const char* end) const {
    const char* id = this->type == RmOrFilterOp::REMOVE ? "remove" : "filter";
    int rc = regex::match(this->arg, begin, end - begin, this->match_data, id);

    if (rc > 0) {
      // there was a match
      if (this->type == RmOrFilterOp::REMOVE) {
        return true;
      }
    } else {
      // there was no match
      if (this->type == RmOrFilterOp::FILTER) {
        return true;
      }
    }
    return false;
  }
};

struct InLimitOp {
  using T = size_t; // parse type put in args
  enum Result { REMOVE, ALLOW, DONE };
  T in_count = 0;
  std::optional<T> low;
  T high;

  InLimitOp(std::tuple<T, std::optional<T>> val) {
    auto first = std::get<0>(val);
    auto second = std::get<1>(val);
    if (second) {
      this->low = first;
      this->high = *second;
    } else {
      this->high = first;
    }
  }

  InLimitOp(T high) : InLimitOp(std::nullopt, high) {}
  InLimitOp(std::optional<T> low, T high) : low(low), high(high) {}

  Result apply() {
    Result ret;
    if (low && in_count < *low) {
      ret = REMOVE;
    } else if (in_count < high) {
      ret = ALLOW;
    } else {
      ret = DONE;
    }
    ++in_count;
    return ret;
  }
};

struct SubOp {
  regex::code target;
  regex::SubstitutionContext ctx;
  const char* replacement;

  SubOp(regex::code&& target, const char* replacement)
      : target(std::move(target)), //
        replacement(replacement) {}

  void apply(std::vector<char>& out, const char* begin, const char* end) { //
    out = regex::substitute_global(target, begin, end - begin, replacement, this->ctx);
  }

  // same as apply, but no copies or moves. sent straight to the output
  void direct_apply(FILE* out, const char* begin, const char* end) {
    regex::match_data data = regex::create_match_data(this->target);
    const char* offset = begin;
    while (offset < end) {
      int rc = regex::match(this->target, begin, end - begin, data, "match before substitution", offset - begin, PCRE2_NOTEMPTY);
      if (rc <= 0) {
        break;
      }
      regex::Match match = regex::get_match(begin, data, "match before substitution");
      str::write_f(out, offset, match.begin);
      offset = match.end;
      std::vector<char> replacement = regex::substitute_on_match(data, this->target, begin, end - begin, this->replacement, this->ctx);
      str::write_f(out, replacement);
    }
    str::write_f(out, offset, end);
  }
};

struct ReplaceOp {
  const char* replacement;
  regex::SubstitutionContext ctx;
  ReplaceOp(const char* replacement) : replacement(replacement) {}

  void apply(std::vector<char>& out,        //
             const char* subj_begin,        //
             const char* subj_end,          //
             const regex::match_data& data, //
             const regex::code& re) {
    out = regex::substitute_on_match(data, re, subj_begin, subj_end - subj_begin, this->replacement, this->ctx);
  }
};

struct IndexOp {
  enum Align { BEFORE, AFTER };
  IndexOp(Align align) : align(align) {}
  size_t index;
  Align align;

 private:
  // bytes needed (without null char) for ascii base 10 representation of uint
  static constexpr size_t space_required(size_t value) { return value == 0 ? 1 : (size_t(std::log10(value)) + 1); }

 public:
  // out_index is the number of tokens that have already been written to the output
  // places the ascii base 10 representation onto the beginning or end of a vector
  void apply(std::vector<char>& v) {
    size_t extension = IndexOp::space_required(index);
    extension += 1; // +1 for space
    v.resize(v.size() + extension);

    if (this->align == IndexOp::BEFORE) {
      // move the entire buffer forward
      char* to_ptr = &*v.rbegin();
      const char* from_ptr = &*v.rbegin() - extension;
      while (from_ptr >= &*v.begin()) {
        *to_ptr-- = *from_ptr--;
      }
      sprintf(&*v.begin(), "%zu", this->index);
      // overwrite the null written by s11printf
      *(v.begin() + (ptrdiff_t)(extension - 1)) = ' ';
    } else {
      char* ptr = &*v.end() - extension;
      *ptr++ = ' ';
      size_t without_last = this->index / 10;
      // index when divided by 10 must take one fewer byte
      if (without_last != 0) { // aka greater than 9
        sprintf(ptr, "%zu", without_last);
      }
      // overwrite the null written by sprintf
      // NOLINTNEXTLINE narrowing to char is ok for index in range [0-9]
      *v.rbegin() = (char)(this->index - without_last * 10) + '0';
    }

    ++this->index;
  }

  // same as apply, but sent straight to the output. no copies or moves used
  void direct_apply(FILE* out, const char* begin, const char* end) {
    if (this->align == IndexOp::BEFORE) {
      fprintf(out, "%zu ", this->index);
    }

    str::write_f(out, begin, end);

    if (this->align != IndexOp::BEFORE) {
      fprintf(out, " %zu", this->index);
    }

    ++this->index;
  }
};

using OrderedOp = std::variant<RmOrFilterOp, SubOp, ReplaceOp, InLimitOp, IndexOp>;

namespace uncompiled {

struct UncompiledRmOrFilterOp {
  RmOrFilterOp::Type type;
  const char* arg;
};

struct UncompiledSubOp {
  const char* target;
  const char* replacement;
};

using UncompiledReplaceOp = ReplaceOp;
using UncompiledInLimitOp = InLimitOp;
using UncompiledIndexOp = IndexOp;

// uncompiled ops are exclusively used in the args. They hold information as all the
// args are parsed. once the args are fully known, they are converted to
// there compiled counterparts.
using UncompiledOrderedOp = std::variant<UncompiledRmOrFilterOp, UncompiledSubOp, UncompiledReplaceOp, UncompiledInLimitOp, UncompiledIndexOp>;

OrderedOp compile(UncompiledOrderedOp op, uint32_t options) {
  if (UncompiledRmOrFilterOp* rf_op = std::get_if<UncompiledRmOrFilterOp>(&op)) {
    const char* id = rf_op->type == RmOrFilterOp::FILTER ? "filter" : "remove";
    return RmOrFilterOp(rf_op->type, regex::compile(rf_op->arg, options, id));
  } else if (UncompiledSubOp* sub_op = std::get_if<UncompiledSubOp>(&op)) {
    return SubOp(regex::compile(sub_op->target, options, "substitute"), sub_op->replacement);
  } else if (UncompiledReplaceOp* o = std::get_if<UncompiledReplaceOp>(&op)) {
    return *o;
  } else if (UncompiledInLimitOp* o = std::get_if<UncompiledInLimitOp>(&op)) {
    return *o;
  } else {
    return std::get<UncompiledIndexOp>(op);
  }
}

} // namespace uncompiled

} // namespace choose
