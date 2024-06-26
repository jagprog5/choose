#pragma once

#include <variant>
#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

struct TuiSelectOp {
  regex::code target;
  regex::match_data match_data;

  TuiSelectOp(regex::code&& target) : target(std::move(target)), match_data(regex::create_match_data(this->target)) {}

  bool matches(const char* begin, const char* end) const {
    int rc = regex::match(this->target, begin, end - begin, this->match_data, "tui selection target");
    return rc > 0;
  }
};

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
  using T = size_t; // type for arg parsing
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

  InLimitOp(T low, T high) : low(low), high(high) {}

  InLimitOp(T high) : low(std::nullopt), high(high) {}

  Result apply() {
    Result ret = DONE;
    if (low && in_count < *low) {
      ret = REMOVE;
    } else if (in_count < high) {
      ret = ALLOW;
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
  size_t index = 0;
  Align align;

 private:
  // just in case floating point math accumulates errors.
  // this is being paranoid, since it's used to allocate a size on the stack.
  // also this give 0 output for 0 input
  static size_t log10_manual(size_t n) {
    size_t log = 0;
    while (n >= 10) {
      n /= 10;
      log++;
    }
    return log;
  }

 public:
  // out_index is the number of tokens that have already been written to the output
  // places the ascii base 10 representation onto the beginning or end of a vector
  void apply(std::vector<char>& v) {
    size_t extension = log10_manual(index) + 1;
    extension += 1; // +1 for space
    if (this->align == IndexOp::BEFORE) {
      char temp[extension];
      // populate temp
      snprintf(temp, extension, "%zu", this->index);
      // overwrite the null written by snprintf
      *(temp + (ptrdiff_t)(extension - 1)) = ' ';

      v.insert(v.cbegin(), temp, temp + extension);
    } else {
      v.resize(v.size() + extension);
      char* ptr = &*v.end() - extension;
      *ptr++ = ' ';
      size_t without_last = this->index / 10;
      // index when divided by 10 must take one fewer byte
      if (without_last != 0) { // aka greater than 9
        snprintf(ptr, extension - 1, "%zu", without_last);
      }
      // overwrite the null written by snprintf
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

using OrderedOp = std::variant<RmOrFilterOp, SubOp, ReplaceOp, InLimitOp, IndexOp, TuiSelectOp>;

namespace uncompiled {

struct UncompiledTuiSelectOp {
  const char* target;
};

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
using UncompiledOrderedOp = std::variant<UncompiledRmOrFilterOp, //
                                         UncompiledSubOp,
                                         UncompiledReplaceOp,
                                         UncompiledInLimitOp,
                                         UncompiledIndexOp,
                                         UncompiledTuiSelectOp>;

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
  } else if (UncompiledTuiSelectOp* o = std::get_if<UncompiledTuiSelectOp>(&op)) {
    return TuiSelectOp(regex::compile(o->target, options, "tui select"));
  } else {
    return std::get<UncompiledIndexOp>(op);
  }
}

} // namespace uncompiled

} // namespace choose
