#pragma once
#include <algorithm>
#include <cassert>
#include <optional>
#include <set>
#include <utility>

#include "args.hpp"
#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

struct Token {
  std::vector<char> buffer;

  // for testing
  Token(const char* in) : buffer(in, in + strlen(in)) {}
  bool operator==(const Token& other) const { return this->buffer == other.buffer; }

  Token(std::vector<char>&& i) : buffer(std::move(i)){};
  Token() = default;
  Token(const Token&) = default;
  Token(Token&&) = default;
  Token& operator=(const Token&) & = default;
  Token& operator=(Token&&) & = default;
  ~Token() = default;
};

// writes an output delimiter between each token
// and a batch output delimiter on finish.
class TokenOutputStream {
  // has anything been written to the output yet
  bool has_written_ = false;

  // is a delimiter required before the next write
  bool delimit_required_ = false;

  const Arguments& args;

 public:
  TokenOutputStream(const Arguments& args) : args(args) {}

  bool has_written() const { return has_written_; }

  // write a part of a token to the output
  void write_output_fragment(const char* begin, const char* end) {
    if (delimit_required_) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = false;
    has_written_ = true;
    str::write_f(args.output, begin, end);
  }

  void write_output(const char* begin, const char* end) {
    if (delimit_required_) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = true;
    has_written_ = true;
    str::write_f(args.output, begin, end);
  }

  void write_output(const Token& t) { write_output(&*t.buffer.cbegin(), &*t.buffer.cend()); }

  void finish_output() {
    if (!args.no_delimit && (has_written_ || args.delimit_on_empty)) {
      str::write_f(args.output, args.bout_delimiter);
    }
    delimit_required_ = false; // optional reset of state
    has_written_ = false;
  }
};

// writes an output delimiter between tokens,
// and a batch delimiter between batches and at the end
class BatchOutputStream {
  bool first_within_batch = true;
  bool first_batch = true;

  const Arguments& args;
  std::optional<std::vector<char>> output;

 public:
  BatchOutputStream(const Arguments& args, //
                    std::optional<std::vector<char>>&& output)
      : args(args), output(output) {}

  void write_output(const Token& t) {
    if (!first_within_batch) {
      str::write_optional_buffer(args.output, output, args.out_delimiter);
    } else if (!first_batch) {
      str::write_optional_buffer(args.output, output, args.bout_delimiter);
    }
    first_within_batch = false;
    str::write_optional_buffer(args.output, output, t.buffer);
  }

  void finish_batch() {
    first_batch = false;
    first_within_batch = true;
  }

  void finish_output() {
    if (!args.no_delimit && (!first_batch || args.delimit_on_empty)) {
      str::write_optional_buffer(args.output, output, args.bout_delimiter);
    }
    str::finish_optional_buffer(args.output, output);
    first_within_batch = true; // optional
    first_batch = true;
  }

  bool uses_buffer() const { return output.has_value(); }
};

// leads to an exit unless this is a unit test
// effectively skips the tui interface
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
    return "input delimiter";
  }
}

using indirect = std::vector<Token>::size_type; // an index into output

struct ProcessTokenContext {
  choose::Arguments& args;
  std::vector<Token>& output;
  std::function<bool(indirect)> uniqueness_check; // returns true if the output[elem] is unique
  std::optional<TokenOutputStream> direct_output;
  // out_count is the number of tokens that have been written to the output.
  // if args.direct_output() isn't set, then this value will mirror output.size()
  decltype(args.out) out_count;
  std::vector<char> fragment; // for cases where a token is being built up over time
};

// this function applies the operations specified in the args to a candidate token.
// returns true iff this should be the last token added to the output
bool process_token(Token&& t, ProcessTokenContext& context) {
  if (!context.fragment.empty()) {
    if (context.fragment.size() + t.buffer.size() > context.args.buf_size_frag) {
      context.fragment.clear();
      return false;
    }
    // handle a part of a token
    str::append_to_buffer(context.fragment, &*t.buffer.cbegin(), &*t.buffer.cend());
    t.buffer = std::move(context.fragment);
    context.fragment = std::vector<char>();
  }
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

  context.output.push_back(std::move(t));
  if (context.uniqueness_check) {
    // some form on uniqueness is being used
    if (!context.uniqueness_check(context.output.size() - 1)) {
      // the element is not unique. nothing was added to the uniqueness set
      context.output.pop_back();
      return false;
    }
  }
  ++context.out_count;

  return context.out_count == context.args.in;
}

// equivalent to process_token when args.is_basic(). it's faster and avoids a copy
void basic_process_token(const char* begin, const char* end, ProcessTokenContext& context) {
  if (!context.fragment.empty()) {
    if (context.fragment.size() + (end - begin) > context.args.buf_size_frag) {
      context.fragment.clear();
      return;
    }
    // handle a part of a token
    str::append_to_buffer(context.fragment, begin, end);
    begin = &*context.fragment.cbegin();
    end = &*context.fragment.cend();
  }
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
  if (!context.fragment.empty()) {
    context.fragment.clear();
  }
}

} // namespace

// reads from args.input, and returns the tokens if not args.out_set().
// if args.out is set, then this function writes the output to args.output
// and then throws a termination_request exception, which the callee should handle.
std::vector<Token> create_tokens(choose::Arguments& args) {
  std::vector<Token> output;
  // edge case on process_token logic. it processes the token, increments, then checks if the limit is hit.
  if (args.out == 0) {
    throw termination_request();
  }

  if (args.in == 0) {
    return output;
  }

  const bool is_uft = regex::options(args.primary) & PCRE2_UTF;
  const bool is_invalid_uft = regex::options(args.primary) & PCRE2_MATCH_INVALID_UTF;

  char subject[args.buf_size];
  size_t subject_size = 0; // how full is the buffer
  PCRE2_SIZE match_offset = 0;
  PCRE2_SIZE prev_sep_end = 0; // only used if !args.match
  const regex::match_data match_data = regex::create_match_data(args.primary);
  uint32_t match_options = PCRE2_PARTIAL_HARD | PCRE2_NOTEMPTY;
  const bool is_match = args.match;
  const bool is_basic = args.is_basic();
  const bool has_ops = !args.ordered_ops.empty();

  std::function<bool(indirect, indirect)> uniqueness_comp = 0;

  auto user_defined_comparison = [&args = std::as_const(args)](const Token& lhs_arg, const Token& rhs_arg) -> bool {
    const Token* lhs = &lhs_arg;
    const Token* rhs = &rhs_arg;
    if (args.sort_reverse) {
      std::swap(lhs, rhs);
    }
    Token combined;
    str::append_to_buffer(combined.buffer, lhs->buffer);
    str::append_to_buffer(combined.buffer, args.comp_sep);
    str::append_to_buffer(combined.buffer, rhs->buffer);
    int comp_result = regex::match(args.comp, combined.buffer.data(), combined.buffer.size(), args.comp_data, "user comp");
    return comp_result > 0;
  };

  auto lexicographical_comparison = [&args = std::as_const(args)](const Token& lhs_arg, const Token& rhs_arg) -> bool {
    const Token* lhs = &lhs_arg;
    const Token* rhs = &rhs_arg;
    if (args.sort_reverse) {
      std::swap(lhs, rhs);
    }
    return std::lexicographical_compare( //
        lhs->buffer.cbegin(), lhs->buffer.cend(), rhs->buffer.cbegin(), rhs->buffer.cend());
  };

  if (args.comp_unique) {
    uniqueness_comp = [&user_defined_comparison, &output = std::as_const(output), &args = std::as_const(args)](indirect lhs, indirect rhs) -> bool { //
      return user_defined_comparison(output[lhs], output[rhs]);
    };
  } else if (args.unique) {
    uniqueness_comp = [&lexicographical_comparison, &output = std::as_const(output)](indirect lhs, indirect rhs) -> bool { //
      return lexicographical_comparison(output[lhs], output[rhs]);
    };
  }

  std::set<indirect, std::function<bool(indirect, indirect)>> uniqueness_set(uniqueness_comp);

  std::function<bool(indirect)> uniqueness_check = 0;
  if (uniqueness_comp) {
    uniqueness_check = [&uniqueness_set](indirect elem) -> bool { //
      return uniqueness_set.insert(elem).second;
    };
  }

  ProcessTokenContext ptc{args,   //
                          output, //
                          uniqueness_check,
                          args.is_direct_output() ? //
                              std::optional(TokenOutputStream(args))
                                                  : std::nullopt,
                          0, //
                          std::vector<char>()};

  while (1) {
    char* write_pos = &subject[subject_size];
    size_t bytes_to_read = std::min(args.bytes_to_read, args.buf_size - subject_size);
    size_t bytes_read = get_n_bytes(args.input, bytes_to_read, write_pos);
    bool input_done = bytes_read != bytes_to_read;
    subject_size += bytes_read;
    if (input_done) {
      // required to make end anchors like \Z match at the end of the input
      match_options &= ~PCRE2_PARTIAL_HARD;
    }

    // don't separate multibyte at end of subject
    const char* subject_effective_end; // NOLINT
    if (is_uft && !input_done) {
      subject_effective_end = str::utf8::last_completed_character_end(subject, subject + subject_size);
      if (subject_effective_end == NULL) {
        if (is_invalid_uft) {
          subject_effective_end = subject + subject_size;
        } else {
          throw std::runtime_error("utf8 decoding error");
        }
      }
    } else {
      subject_effective_end = subject + subject_size;
    }

  skip_read: // do another iteration but don't read in any more bytes

    int match_result = regex::match(args.primary,                    //
                                    subject,                         //
                                    subject_effective_end - subject, //
                                    match_data,                      //
                                    id(is_match),                    //
                                    match_offset,                    //
                                    match_options);

    if (match_result > 0) {
      // a complete match:
      // process the match, set the offsets, then do another iteration without
      // reading more input
      regex::Match match = regex::get_match(subject, match_data, id(is_match));
      if (is_match) {
        auto match_handler = [&](const regex::Match& m) -> bool {
          if (is_basic) {
            basic_process_token(m.begin, m.end, ptc);
            return false;
          } else {
            Token t;
            str::append_to_buffer(t.buffer, m.begin, m.end);
            return process_token(std::move(t), ptc);
          }
        };
        if (regex::get_match_and_groups(subject, match_result, match_data, match_handler, "match pattern")) {
          break;
        }
      } else {
        if (is_basic) {
          basic_process_token(subject + prev_sep_end, match.begin, ptc);
        } else {
          Token t;
          str::append_to_buffer(t.buffer, subject + prev_sep_end, match.begin);
          if (process_token(std::move(t), ptc)) {
            break;
          }
        }
        prev_sep_end = match.end - subject;
      }
      // set the start offset to just after the match
      match_offset = match.end - subject;
      goto skip_read;
    } else {
      if (!input_done) {
        // no or partial match and input is left
        const char* new_subject_begin; // NOLINT
        if (match_result == 0) {
          // there was no match but there is more input
          new_subject_begin = subject_effective_end;
        } else {
          // there was a partial match and there is more input
          regex::Match match = regex::get_match(subject, match_data, id(is_match));
          new_subject_begin = match.begin;
        }

        // account for lookbehind bytes to retain prior to the match
        const char* new_subject_begin_cp = new_subject_begin;
        new_subject_begin -= args.max_lookbehind;
        if (new_subject_begin < subject) {
          new_subject_begin = subject;
        }
        if (is_uft) {
          // don't separate multibyte at begin of subject
          new_subject_begin = str::utf8::decrement_until_character_start(new_subject_begin, subject, subject_effective_end);
        }

        // pointer to begin of bytes retained, not including from the previous separator end
        const char* retain_marker = new_subject_begin;

        if (!is_match) {
          // keep the bytes required, either from the lookback retain for the next iteration,
          // or because the delimiter ended there
          const char* subject_const = subject;
          new_subject_begin = std::min(new_subject_begin, subject_const + prev_sep_end);
        }

        // cut out the excess from the beginning and adjust the offsets
        match_offset = new_subject_begin_cp - new_subject_begin;
        prev_sep_end -= new_subject_begin - subject;

        char* to = subject;
        const char* from = new_subject_begin;
        if (from != to) {
          while (from < subject + subject_size) {
            *to++ = *from++;
          }
          subject_size -= from - to;
        } else if (subject_size == args.buf_size) {
          // the buffer size has been filled
          if (is_match) {
            // count as match failure
            match_offset = 0;
            subject_size = 0;
          } else {
            // there is not enough room in the match buffer, so we're moving the part of the token
            // that is before the delimiter into a separate buffer or directly to the output
            char* begin;     // NOLINT
            const char* end; // NOLINT

            if (prev_sep_end != 0) {
              // the buffer is being retained because of lookbehind bytes.
              // can't properly match, so results in match failure
              begin = subject;
              end = subject + subject_size;
              prev_sep_end = 0;
            } else {
              begin = subject;
              end = retain_marker;

              if (begin == end) {
                // the entire buffer is composed of the partially matched delimiter
                // results in match failure
                begin = subject;
                end = subject + subject_size;
              }
            }

            if (!has_ops && is_basic) {
              ptc.direct_output->write_output_fragment(begin, end);
            } else {
              if (ptc.fragment.size() + (end - begin) > args.buf_size_frag) {
                ptc.fragment.clear();
              } else {
                str::append_to_buffer(ptc.fragment, begin, end);
              }
            }

            while (end < subject + subject_size) {
              *begin++ = *end++;
            }
            subject_size -= end - begin;
            match_offset = 0;
          }
        }
      } else {
        // no match and no more input:
        // process the last token and break from the loop
        if (!is_match) {
          if (prev_sep_end != subject_size || args.use_input_delimiter || !ptc.fragment.empty()) {
            if (is_basic) {
              basic_process_token(subject + prev_sep_end, subject_effective_end, ptc);
            } else {
              std::vector<char> v;
              str::append_to_buffer(v, subject + prev_sep_end, subject_effective_end);
              Token t{std::move(v)};
              process_token(std::move(t), ptc);
            }
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

  uniqueness_set.clear();

  if (args.comp_sort) {
    std::stable_sort(output.begin(), output.end(), user_defined_comparison);
  } else if (args.sort) {
    std::sort(output.begin(), output.end(), lexicographical_comparison);
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

} // namespace choose
