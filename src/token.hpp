#pragma once
#include <algorithm>
#include <cassert>
#include <optional>
#include <set>

#include "args.hpp"
#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

struct Token {
  std::vector<char> buffer;
  bool operator<(const Token& other) const {  //
    return std::lexicographical_compare(buffer.cbegin(), buffer.cend(), other.buffer.cbegin(), other.buffer.cend());
  }
  bool operator>(const Token& other) const { return other < *this; }
  bool operator==(const Token& other) const { return this->buffer == other.buffer; }
  bool operator!=(const Token& other) const { return !(*this == other); }

  // for testing
  Token(const char* in) : buffer(in, in + strlen(in)) {}

  Token() = default;
  Token(const Token&) = default;
  Token(Token&&) = default;
  Token& operator=(const Token&) & = default;
  Token& operator=(Token&&) & = default;
};

// writes an output separator between each token
// and a batch output separator on finish.
class TokenOutputStream {
  bool first = true;
  const Arguments& args;

 public:
  TokenOutputStream(const Arguments& args) : args(args) {}

  void write_output(const Token& t) {
    if (!first) {
      str::write_f(args.output, args.out_separator);
    }
    first = false;
    str::write_f(args.output, t.buffer);
  }

  void write_output(const char* begin, const char* end) {
    if (!first) {
      str::write_f(args.output, args.out_separator);
    }
    first = false;
    str::write_f(args.output, begin, end);
  }

  void finish_output() {
    if (!args.bout_no_delimit && !first) {
      str::write_f(args.output, args.bout_separator);
    }
    first = true;  // optional
  }
};

// writes an output separator between tokens,
// and a batch separator between batches and at the end
class BatchOutputStream {
  bool first_within_batch = true;
  bool first_batch = true;

  const Arguments& args;
  std::optional<std::vector<char>> output;

 public:
  BatchOutputStream(const Arguments& args,  //
                    std::optional<std::vector<char>>&& output)
      : args(args), output(output) {}

  void write_output(const Token& t) {
    if (!first_within_batch) {
      str::write_optional_buffer(args.output, output, args.out_separator);
    } else if (!first_batch) {
      str::write_optional_buffer(args.output, output, args.bout_separator);
    }
    first_within_batch = false;
    str::write_optional_buffer(args.output, output, t.buffer);
  }

  void finish_batch() {
    first_batch = false;
    first_within_batch = true;
  }

  void finish_output() {
    if (!args.bout_no_delimit && !first_batch) {
      str::write_optional_buffer(args.output, output, args.bout_separator);
    }
    str::finish_optional_buffer(args.output, output);
    first_within_batch = true;  // optional
    first_batch = true;
  }

  bool uses_buffer() const { return output.has_value(); }
};

struct termination_request : public std::exception {};

namespace {

// returns number of bytes read from file
size_t get_n_bytes(FILE* f, size_t n, char* out) {
  // assertion is guaranteed in the cli arg parsing and min_match_length logic.
  // it makes sure that create_tokens always makes progress
  assert(n != 0);
  size_t read_ret = fread(out, sizeof(char), n, f);
  if (read_ret == 0) {
    if (feof(f)) {
      return read_ret;
    } else {
      // no need to check ferror here, since we never read zero elements
      const char* err_string = strerror(errno);
      throw std::runtime_error(err_string);
    }
  }
  return read_ret;
}

const char* id(bool is_match) {
  if (is_match) {
    return "match pattern";
  } else {
    return "input separator";
  }
}

using indirect = std::vector<Token>::size_type;  // an index into output

struct ProcessTokenContext {
  choose::Arguments& args;
  std::vector<Token>& output;
  std::function<bool(indirect)> seen_check;
  std::optional<TokenOutputStream> direct_output;
  // out_count is the number of tokens that have been written to the output.
  // if args.direct_output() isn't set, then this value will mirror output.size()
  decltype(args.out) out_count;
};

// this function applies the operations specified in the args to a candidate token.
// returns true iff this should be the last token added to the output
bool process_token(Token&& t, ProcessTokenContext& context) {
  for (OrderedOp& op : context.args.ordered_ops) {
    if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
      std::vector<char> sub_buf = regex::substitute_global(sub_op->target, t.buffer.data(), t.buffer.size(), sub_op->replacement);
      t.buffer = std::move(sub_buf);
    } else if (RmOrFilterOp* rf_op = std::get_if<RmOrFilterOp>(&op)) {
      const char* id = rf_op->type == RmOrFilterOp::REMOVE ? "remove" : "filter";
      int rc = regex::match(rf_op->arg, t.buffer.data(), t.buffer.size(), rf_op->match_data, id);

      if (rc > 0) {
        // there was a match
        if (rf_op->type == RmOrFilterOp::REMOVE) {
          return false;
        }
      } else {
        // there was no match
        if (rf_op->type == RmOrFilterOp::FILTER) {
          return false;
        }
      }
    } else {
      IndexOp& in_op = std::get<IndexOp>(op);

      if (in_op.is_output_index_op()) {
        str::apply_index_op(t.buffer, context.out_count, in_op.align == IndexOp::BEFORE);
      } else {
        str::apply_index_op(t.buffer, in_op.index, in_op.align == IndexOp::BEFORE);
        ++in_op.index;
      }
    }
  }

  if (context.direct_output) {
    context.direct_output->write_output(t);
    ++context.out_count;
    if (context.out_count == context.args.out || context.out_count == context.args.in) {
      context.direct_output->finish_output();
      throw termination_request();
    }
    return false;
  }

  if (!context.args.sort || context.args.defined_sort_comp) {
    context.output.push_back(std::move(t));
    if (context.args.unique) {
      // if unique, duplicate removed
      if (!context.seen_check(context.output.size() - 1)) {
        context.output.pop_back();
        return false;
      }
    }
    ++context.out_count;
  } else {
    // non user defined sorting and uniqueness. user defined sorting is handled below
    auto comp = [&context](const Token& lhs, const Token& rhs) -> bool {  //
      return context.args.sort_reverse ? lhs > rhs : lhs < rhs;
    };
    auto pos = std::lower_bound(context.output.cbegin(), context.output.cend(), t, comp);
    if (!context.args.unique || pos == context.output.cend() || *pos != t) {
      context.output.insert(pos, std::move(t));
      ++context.out_count;
    }
  }

  return context.out_count == context.args.in;
}

// equivalent to process_token when args.is_basic(). it's faster and avoids a copy
void basic_process_token(const char* begin, const char* end, ProcessTokenContext& context) {
  for (const OrderedOp& op : context.args.ordered_ops) {
    const RmOrFilterOp& rf_op = std::get<RmOrFilterOp>(op);
    const char* id = rf_op.type == RmOrFilterOp::REMOVE ? "remove" : "filter";
    int rc = regex::match(rf_op.arg, begin, end - begin, rf_op.match_data, id);

    if (rc > 0) {
      // there was a match
      if (rf_op.type == RmOrFilterOp::REMOVE) {
        return;
      }
    } else {
      // there was no match
      if (rf_op.type == RmOrFilterOp::FILTER) {
        return;
      }
    }
  }

  context.direct_output->write_output(begin, end);
  ++context.out_count;
  if (context.out_count == context.args.out || context.out_count == context.args.in) {
    context.direct_output->finish_output();
    throw termination_request();
  }
}

}  // namespace

// reads from args.input, and returns the tokens if not args.out_set().
// if args.out is set, then this function writes the output to args.output
// and then throws a termination_request exception, which the callee should handle.
std::vector<Token> create_tokens(choose::Arguments& args) {
  std::vector<Token> output;
  // edge case on process_token logic. it processes the token, increments, then checks if the limit is hit.
  if (args.out == 0) {
    throw termination_request();
  }

  // same reason as above, but for basic_process_token
  if (args.in == 0) {
    return output;
  }

  bool is_uft = regex::options(args.primary) & PCRE2_UTF;
  bool is_invalid_uft = regex::options(args.primary) & PCRE2_MATCH_INVALID_UTF;

  uint32_t max_lookbehind_size;  // bytes
  if (args.max_lookbehind_set()) {
    max_lookbehind_size = args.max_lookbehind;
  } else {
    max_lookbehind_size = regex::max_lookbehind_size(args.primary);
  }
  if (is_uft) {
    max_lookbehind_size *= str::utf8::MAX_BYTES_PER_CHARACTER;
  }

  uint32_t min_bytes_to_read;
  if (args.min_read_set()) {
    min_bytes_to_read = args.min_read;
  } else {
    // * 2 is arbitrary. some amount more than the match
    min_bytes_to_read = regex::min_match_length(args.primary) * 2;
    // reasonable lower bound based on cursory profiling
    static constexpr size_t LOWER_BOUND = RETAIN_LIMIT_DEFAULT / 8;
    if (min_bytes_to_read < LOWER_BOUND) {
      min_bytes_to_read = LOWER_BOUND;
    }
  }

  std::vector<char> subject;    // match subject
  PCRE2_SIZE start_offset = 0;  // match offset in the subject
  regex::match_data match_data = regex::create_match_data(args.primary);
  uint32_t match_options = PCRE2_PARTIAL_HARD | PCRE2_NOTEMPTY;
  const bool is_match = args.match;
  const bool is_basic = args.is_basic();

  // seen is only used if the output is unique and unsorted.
  // it is entirely used in process_token
  auto indirect_comparison = [&output](indirect lhs, indirect rhs) -> bool { return output[lhs] < output[rhs]; };
  std::set<indirect, decltype(indirect_comparison)> seen(indirect_comparison);

  // returns true if the output[elem] is unique
  std::function<bool(indirect)> seen_check = [&seen](indirect elem) -> bool { return seen.insert(elem).second; };

  ProcessTokenContext ptc{args,                      //
                          output,                    //
                          seen_check,                //
                          args.is_direct_output() ?  //
                              std::optional(TokenOutputStream(args))
                                                  : std::nullopt,
                          0};

  // stores the token so far.
  // only used when is_match == false
  Token token_being_built;

  bool input_done;
  // helper lambda. reads stdin into the subject.
  // sets input_done appropriately
  auto get_more_input = [&](size_t n) {
    subject.resize(subject.size() + n);
    char* write_pos = &*subject.end() - n;
    size_t bytes_read = get_n_bytes(args.input, n, write_pos);
    input_done = bytes_read != n;
    if (input_done) {  // shrink excess
      subject.resize(bytes_read + write_pos - &*subject.begin());
    }
  };

  while (1) {
    get_more_input(min_bytes_to_read);
    if (is_uft && !input_done) {
      int to_complete = str::utf8::bytes_required(&*subject.cbegin(), &*subject.cend());
      if (to_complete > 0) {
        get_more_input(to_complete);
      } else if (to_complete < 0 && !is_invalid_uft) {
        throw std::runtime_error("utf8 decoding error");
      }
    }

    if (input_done) {
      // required to make end anchors like \Z match at the end of the input
      match_options &= ~PCRE2_PARTIAL_HARD;
    }

  skip_read:  // do another iteration but don't read in any more bytes

    // match for the pattern in the subject
    int match_result = regex::match(args.primary, subject.data(), subject.size(), match_data, id(is_match), start_offset, match_options);

    if (match_result != 0 && match_result != -1) {
      // a complete match
      regex::Match match = regex::get_match(subject.data(), match_data, id(is_match));

      if (is_match) {
        auto match_handler = [&](const regex::Match& m) -> bool {
          if (is_basic) {
            basic_process_token(m.begin, m.end, ptc);
            return false;
          } else {
            std::vector<char> token_buf;
            str::append_to_buffer(token_buf, m.begin, m.end);
            Token t;
            t.buffer = std::move(token_buf);
            return process_token(std::move(t), ptc);
          }
        };
        if (regex::get_match_and_groups(subject.data(), match_result, match_data, match_handler, "match pattern")) {
          break;
        }
      } else {
        if (is_basic && token_being_built.buffer.empty()) {
          basic_process_token(&*subject.begin() + start_offset, match.begin, ptc);
        } else {
          // copy the bytes prior to the separator into token_being_built
          str::append_to_buffer(token_being_built.buffer, &*subject.begin() + start_offset, match.begin);
          if (process_token(std::move(token_being_built), ptc)) {
            break;
          }
          token_being_built = Token();
        }
      }

      // set the start offset to just after the match
      start_offset = match.end - &*subject.begin();
      goto skip_read;
    } else {
      if (!input_done) {
        const char* new_subject_begin;
        if (match_result == 0) {
          // there was no match but there is more input
          new_subject_begin = &*subject.cend();
        } else {
          // there was a partial match and there is more input
          regex::Match match = regex::get_match(subject.data(), match_data, "input separator");
          new_subject_begin = match.begin;
        }

        if (!is_match) {
          str::append_to_buffer(token_being_built.buffer, &*subject.begin() + start_offset, new_subject_begin);
          if ((int)token_being_built.buffer.size() > args.retain_limit) {
            // the line of text is really really long.
            // grep's handling of this case is implementation defined.
            token_being_built.buffer.clear();
          }
        }

        // account for lookbehind bytes to retain prior to the match
        const char* new_subject_begin_cp = new_subject_begin;
        new_subject_begin -= max_lookbehind_size;
        if (new_subject_begin < &*subject.begin()) {
          new_subject_begin = &*subject.begin();
        }
        if (is_uft) {
          new_subject_begin = str::utf8::decrement_until_not_separating_multibyte(new_subject_begin, &*subject.cbegin(), &*subject.cend());
        }

        start_offset = new_subject_begin_cp - new_subject_begin;
        subject.erase(subject.begin(), typename std::vector<char>::const_iterator(new_subject_begin));

        if ((int)subject.size() > args.retain_limit) {
          // the partial match length has exceeded the limit. count as a no match
          if (!is_match) {
            // typical behaviour on match failure is to append the subject to the token being built.
            // however, since the token_being_built would also reach this limit, it is discarded as well
            token_being_built.buffer.clear();
          }
          subject.clear();
          start_offset = 0;
        }
      } else {
        // there was no match and there is no more input
        if (!is_match) {
          str::append_to_buffer(token_being_built.buffer, &*subject.begin() + start_offset, &*subject.end());
          if (token_being_built.buffer.size() != 0 || args.use_input_delimiter) {
            process_token(std::move(token_being_built), ptc);
          }
        }
        break;
      }
    }
  }

  // skips the interface, and didn't stored the tokens anywhere
  if (ptc.direct_output) {
    ptc.direct_output->finish_output();
    throw termination_request();
  }

  if (args.defined_sort_comp) {
    // user defined sort requires different sorting and uniqueness handling
    // uniqueness was handled already in process_token. now just sorting
    auto comp = [&args](const Token& lhs, const Token& rhs) -> bool {  //
      Token combined;
      str::append_to_buffer(combined.buffer, lhs.buffer);
      str::append_to_buffer(combined.buffer, args.defined_sort_sep);
      str::append_to_buffer(combined.buffer, rhs.buffer);
      int comp_result = regex::match(args.defined_sort_comp, combined.buffer.data(), combined.buffer.size(), args.defined_sort_comp_data, "user comp");
      bool ret = comp_result > 0;
      if (args.sort_reverse) {
        ret = !ret;  // subtle nuance here. this is ok because the sort is stable
      }
      return ret;
    };
    std::stable_sort(output.begin(), output.end(), comp);
  }

  if (args.flip) {
    std::reverse(output.begin(), output.end());
  }

  // argument that skips the interface, in its simplest form
  if (args.out_set()) {
    choose::TokenOutputStream tos(args);
    auto pos = output.begin();
    while (pos != output.end() && args.out-- > 0) {
      tos.write_output(*pos++);
    }
    tos.finish_output();
    throw termination_request();
  }

  return output;
}

}  // namespace choose
