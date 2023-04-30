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
      str::write_stdout(args.out_separator);
    }
    first = false;
    str::write_stdout(t.buffer);
  }

  void write_output(const char* begin, const char* end) {
    if (!first) {
      str::write_stdout(args.out_separator);
    }
    first = false;
    str::write_stdout(begin, end);
  }

  void finish_output() {
    if (!args.bout_no_delimit && !first) {
      str::write_stdout(args.bout_separator);
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
      str::write_optional_buffer(output, args.out_separator);
    } else if (!first_batch) {
      str::write_optional_buffer(output, args.bout_separator);
    }
    first_within_batch = false;
    str::write_optional_buffer(output, t.buffer);
  }

  void finish_batch() {
    first_batch = false;
    first_within_batch = true;
  }

  void finish_output() {
    if (!args.bout_no_delimit && !first_batch) {
      str::write_optional_buffer(output, args.bout_separator);
    }
    str::finish_optional_buffer(output);
    first_within_batch = true;  // optional
    first_batch = true;
  }

  bool output_is_stdout() { return !output.has_value(); }
};

namespace {

// returns number of bytes read from stdin
size_t get_n_bytes(size_t n, char* out) {
  // assertion is guaranteed in the cli arg parsing and min_match_length logic.
  // it makes sure that create_tokens always makes progress
  assert(n != 0);
  size_t read_ret = fread(out, sizeof(char), n, stdin);
  if (read_ret == 0) {
    if (feof(stdin)) {
      return read_ret;
    } else {
      const char* err_string = strerror(errno);
      throw std::runtime_error(err_string);
    }
  }
  return read_ret;
}

const char* id(const choose::Arguments& args) {
  if (args.match) {
    return "match pattern";
  } else {
    return "input separator";
  }
}

using indirect = std::vector<Token>::size_type;  // an index into output

struct ProcessTokenContext {
  const choose::Arguments& args;
  std::vector<Token>& output;
  std::function<bool(indirect)> seen_check;
  std::optional<TokenOutputStream> direct_output;
  // out_count is the number of tokens that have been written to the output.
  // if direct_output isn't set, then this value will mirror output.size()
  decltype(args.out) out_count;
};

// returns true iff this should be the last token added to the output
bool process_token(Token&& t, ProcessTokenContext& context) {
  for (const OrderedOp& op : context.args.ordered_ops) {
    if (const SubOp* sub_op = std::get_if<SubOp>(&op)) {
      std::vector<char> sub_buf = regex::substitute_global(sub_op->target, t.buffer.data(), t.buffer.size(), sub_op->replacement);
      t.buffer = std::move(sub_buf);
    } else if (const RmOrFilterOp* rf_op = std::get_if<RmOrFilterOp>(&op)) {
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
      const IndexOp& in_op = std::get<IndexOp>(op);

      if (in_op.is_output_index_op()) {
        str::apply_index_op(t.buffer, context.out_count, in_op.align == IndexOp::BEFORE);
      } else {
        str::apply_index_op(t.buffer, in_op.index, in_op.align == IndexOp::BEFORE);
        ++const_cast<size_t&>(in_op.index);
      }
    }
  }

  if (context.direct_output) {
    context.direct_output->write_output(t);
    ++context.out_count;
    if (context.out_count == context.args.out || context.out_count == context.args.in) {
      context.direct_output->finish_output();
      exit(EXIT_SUCCESS);
    }
    return false;
  }

  if (!context.args.sort) {
    context.output.push_back(std::move(t));
    if (context.args.unique) {
      // if unique, duplicate are remove
      if (!context.seen_check(context.output.size() - 1)) {
        context.output.pop_back();
        return false;
      }
    }
    ++context.out_count;
  } else {
    // sorted
    bool sort_is_reversed = context.args.sort_reverse;
    auto greater_or_lesser = [sort_is_reversed](const Token& lhs, const Token& rhs) -> bool {  //
      return sort_is_reversed ? lhs > rhs : lhs < rhs;
    };
    auto pos = std::lower_bound(context.output.cbegin(), context.output.cend(), t, greater_or_lesser);
    if (!context.args.unique || pos == context.output.cend() || *pos != t) {
      context.output.insert(pos, std::move(t));
      ++context.out_count;
    }
  }

  return context.out_count == context.args.in;
}

// equivalent to process_token when args.is_basic()
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
    exit(EXIT_SUCCESS);
  }
}

}  // namespace

// reads characters from stdin
// note that even though args is a const reference, the data pointed to by some of the ordered ops may be modified
// this function calls exit if args.is_direct_output() is true
std::vector<Token> create_tokens(const choose::Arguments& args) {
  // this function contains a lot of branching. for example, the case where
  // args.match is true vs false are both handled throughout in the logic below.
  // although it would be better if the logic were modified to instead have a
  // single branch at the beginning, followed by specialized logic for each
  // path, it doesn't lead to a noticeable performance improvement (with
  // -Ofast), and arguable less readable since the common logic is repeated

  std::vector<Token> output;
  // edge case on process_token logic. it processes the token, then checks if the limit is hit.
  if (args.in == 0 || args.out == 0)
    return output;

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
  bool is_basic = args.is_basic();

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

  Token token_being_built;  // only used when args.match == false

  bool input_done;
  // helper lambda. reads stdin into the subject.
  // sets input_done appropriately
  auto get_more_input = [&](size_t n) {
    subject.resize(subject.size() + n);
    char* write_pos = &*subject.end() - n;
    size_t bytes_read = get_n_bytes(n, write_pos);
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
      // required to make end anchors like \Z match at the end of the input.
      // Although this is clearly document, to me this is a quirk of PCRE2 that
      // doesn't make sense
      match_options &= ~PCRE2_PARTIAL_HARD;
    }

  skip_read:  // do another iteration but don't read in any more bytes

    // match for the pattern in the subject
    int match_result = regex::match(args.primary, subject.data(), subject.size(), match_data, id(args), start_offset, match_options);

    if (match_result != 0 && match_result != -1) {
      // a complete match
      regex::Match match = regex::get_match(subject.data(), match_data, id(args));

      auto match_handler = [&](const regex::Match& m) -> bool {
        if (is_basic) {
          basic_process_token(m.begin, m.end, ptc);
          return false;
        } else {
          std::vector<char> token_buf;
          str::append_to_buffer(token_buf, m.begin, m.end);
          Token t{std::move(token_buf)};
          return process_token(std::move(t), ptc);
        }
      };

      if (args.match) {
        if (match_handler(match)) {
          break;
        }
        if (regex::get_groups(subject.data(), match_result, match_data, match_handler, "match pattern")) {
          break;
        }
      } else {
        if (is_basic && token_being_built.buffer.size() == 0) {
          // if token_being_built is not empty (e.g. from the separator not
          // being found during the previous iteration), then revert to normal
          // append to token_being_built and revert to normal process token
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

        if (!args.match) {
          str::append_to_buffer(token_being_built.buffer, &*subject.begin() + start_offset, new_subject_begin);
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

        if (&*subject.end() - new_subject_begin >= args.retain_limit) {
          // the partial match length has exceeded the limit. count as a no match
          if (!args.match) {
            str::append_to_buffer(token_being_built.buffer, new_subject_begin_cp, &*subject.end());
          }
          subject.clear();
          start_offset = 0;
        } else {
          start_offset = new_subject_begin_cp - new_subject_begin;
          subject.erase(subject.begin(), typename std::vector<char>::const_iterator(new_subject_begin));
        }
      } else {
        // there was no match and there is no more input
        if (!args.match) {
          str::append_to_buffer(token_being_built.buffer, &*subject.begin() + start_offset, &*subject.end());
          if (token_being_built.buffer.size() != 0 || args.use_input_delimiter) {
            process_token(std::move(token_being_built), ptc);
          }
        }
        break;
      }
    }
  }

  if (ptc.direct_output) {
    ptc.direct_output->finish_output();
    exit(EXIT_SUCCESS);
  }

  if (args.flip) {
    std::reverse(output.begin(), output.end());
  }
  return output;
}

}  // namespace choose
