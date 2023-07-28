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
  size_t in_limit;

  InLimitOp(size_t n) : in_limit(n) {}

  bool finished() { return !in_limit--; }
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

  void direct_apply(FILE* out,
                    const char* subj_begin,        //
                    PCRE2_SIZE sub_len,          //
                    const regex::match_data& data, //
                    const regex::code& re) {
    regex::substitute_on_match_direct(out, data, re, subj_begin, sub_len, this->replacement, this->ctx);
  }

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
  enum Type { INPUT, OUTPUT };
  IndexOp(Type type, Align align) : in_index(type == INPUT ? 0 : std::numeric_limits<size_t>::max()), align(align) {}
  // index is only used if this is an input op.
  // if it is set to max, this indicates that it is an output op,
  // in which case, the supplied output index is used
  size_t in_index;
  Align align;

 private:
  bool is_output_index_op() const { return in_index == std::numeric_limits<size_t>::max(); }

  // bytes needed (without null char) for ascii base 10 representation of uint
  static constexpr size_t space_required(size_t value) { return value == 0 ? 1 : (size_t(std::log10(value)) + 1); }

 public:
  // out_index is the number of tokens that have already been written to the output
  // places the ascii base 10 representation onto the beginning or end of a vector
  void apply(std::vector<char>& v, size_t out_index) {
    size_t index = this->is_output_index_op() ? out_index : this->in_index;
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

  // same as apply, but sent straight to the output. no copies or moves used
  void direct_apply(FILE* out, const char* begin, const char* end, size_t out_index) {
    size_t index = this->is_output_index_op() ? out_index : this->in_index;

    if (this->align == IndexOp::BEFORE) {
      fprintf(out, "%zu ", index);
    }

    str::write_f(out, begin, end);

    if (this->align != IndexOp::BEFORE) {
      fprintf(out, " %zu", index);
    }

    if (!this->is_output_index_op()) {
      ++this->in_index;
    }
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
