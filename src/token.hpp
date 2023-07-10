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

// Token is a thin wrapper around vector<char>. provides type clarity
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
// and (might, depending on args) a batch output delimiter on finish.
struct TokenOutputStream {
  // number of elements written to the output
  size_t out_count = 0;

  // is a delimiter required before the next write
  bool delimit_required_ = false;

  const Arguments& args;

  static void default_write(FILE* out, const char* begin, const char* end) { //
    str::write_f(out, begin, end);
  }

  TokenOutputStream(const Arguments& args) : args(args) {}

  // write a part of a token to the output.
  // the last part of a token must instead use write_output
  void write_output_fragment(const char* begin, const char* end) {
    if (delimit_required_) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = false;
    str::write_f(args.output, begin, end);
  }

  // write a part or whole of a token to the output.
  // if it is a part, then it must be the last part.
  // pass a handler void(FILE* out, const char* begin, const char* end).
  // the function will write the token to the output after applying transformations
  template <typename T = decltype(TokenOutputStream::default_write)>
  void write_output(const char* begin, //
                    const char* end,
                    T handler = TokenOutputStream::default_write) {
    if (delimit_required_) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = true;
    handler(args.output, begin, end);
    ++out_count;
  }

  void write_output(const Token& t) { //
    write_output(&*t.buffer.cbegin(), &*t.buffer.cend());
  }

  // call after all other writing has finished
  void finish_output() {
    if (!args.delimit_not_at_end && (out_count || args.delimit_on_empty)) {
      str::write_f(args.output, args.bout_delimiter);
    }
    delimit_required_ = false; // optional reset of state
    out_count = 0;
  }
};

// writes an output delimiter between tokens,
// and a batch delimiter between batches and at the end
struct BatchOutputStream {
  bool first_within_batch = true;
  bool first_batch = true;

  const Arguments& args;

  BatchOutputStream(const Arguments& args) : args(args) {}

  void write_output(const Token& t) {
    if (!first_within_batch) {
      str::write_f(args.output, args.out_delimiter);
    } else if (!first_batch) {
      str::write_f(args.output, args.bout_delimiter);
    }
    first_within_batch = false;
    str::write_f(args.output, t.buffer);
  }

  void finish_batch() {
    first_batch = false;
    first_within_batch = true;
  }

  void finish_output() {
    if (!args.delimit_not_at_end && (!first_batch || args.delimit_on_empty)) {
      str::write_f(args.output, args.bout_delimiter);
    }
    first_within_batch = true; // optional reset of state
    first_batch = true;
  }
};

// leads to an exit unless this is a unit test
// effectively skips the tui interface
struct termination_request : public std::exception {};

namespace {

const char* id(bool is_match) {
  if (is_match) {
    return "match pattern";
  } else {
    return "input delimiter";
  }
}

void drop_warning(Arguments& args) {
  if (args.can_drop_warn) {
    args.can_drop_warn = false;
    if (fileno(args.output) == STDOUT_FILENO) { // not unit test
      fputs(
          "Warning: bytes were dropped from overlong token. "
          "Set --no-warn, or increase --buf-size-frag, "
          "or set the delimiter to something matched more frequently.\n",
          stderr);
    }
  }
}

using indirect = std::vector<Token>::size_type; // an index into output

} // namespace

// reads from args.input
// if args.tui:
//      returns the tokens
// else
//      writes to args.output, then throws a termination_request exception,
//      which the caller should handle (exit unless unit test)
std::vector<Token> create_tokens(choose::Arguments& args) {
  const bool single_byte_delimiter = args.in_byte_delimiter.has_value();
  const bool is_utf = single_byte_delimiter ? false : regex::options(args.primary) & PCRE2_UTF;
  const bool is_invalid_utf = single_byte_delimiter ? false : regex::options(args.primary) & PCRE2_MATCH_INVALID_UTF;

  // single_byte_delimiter implies not match. stating below so the compiler can hopefully leverage it
  const bool is_match = !single_byte_delimiter && args.match;
  const bool is_direct_output = args.is_direct_output();
  // sed implies is_direct_output and is_match
  const bool is_sed = is_direct_output && is_match && args.sed;
  const bool tokens_not_stored = args.tokens_not_stored();
  const bool has_ops = !args.ordered_ops.empty();
  const bool flush = args.flush;

  const bool is_sort_reverse = args.sort_reverse;
  const bool is_unique = args.unique;
  const bool is_comp_unique = args.comp_unique;

  char subject[args.buf_size]; // match buffer
  size_t subject_size = 0;     // how full is the buffer
  PCRE2_SIZE match_offset = 0;
  PCRE2_SIZE prev_match_end = 0; // only used if !args.match or args.sed
  uint32_t match_options = PCRE2_PARTIAL_HARD | PCRE2_NOTEMPTY;

  // if sed, the output is written directly via fwrite
  TokenOutputStream direct_output(args); //  if is_direct_output and !sed, this is used
  std::vector<Token> output;             // !tokens_not_stored, this is used

  // edge case on logic
  if (args.out == 0 || args.in == 0) {
    goto skip_all;
  }

  {
    auto user_defined_comparison = [is_sort_reverse, &args = std::as_const(args)](const Token& lhs_arg, const Token& rhs_arg) -> bool {
      const Token* lhs = &lhs_arg;
      const Token* rhs = &rhs_arg;
      if (is_sort_reverse) {
        std::swap(lhs, rhs);
      }

      int lhs_result = regex::match(args.comp, lhs->buffer.data(), lhs->buffer.size(), args.comp_data, "user comp");
      int rhs_result = regex::match(args.comp, rhs->buffer.data(), rhs->buffer.size(), args.comp_data, "user comp");
      if (lhs_result && !rhs_result) {
        return 1;
      } else {
        return 0;
      }
    };

    auto lexicographical_comparison = [is_sort_reverse, &args = std::as_const(args)](const Token& lhs_arg, const Token& rhs_arg) -> bool {
      const Token* lhs = &lhs_arg;
      const Token* rhs = &rhs_arg;
      if (is_sort_reverse) {
        std::swap(lhs, rhs);
      }
      return std::lexicographical_compare( //
          lhs->buffer.cbegin(), lhs->buffer.cend(), rhs->buffer.cbegin(), rhs->buffer.cend());
    };

    auto uniqueness_comp = [&](indirect lhs, indirect rhs) -> bool {
      if (is_comp_unique) {
        return user_defined_comparison(output[lhs], output[rhs]);
      } else if (is_unique) {
        return lexicographical_comparison(output[lhs], output[rhs]);
      } else {
        return false; // never
      }
    };

    std::set<indirect, decltype(uniqueness_comp)> uniqueness_set(uniqueness_comp);

    // returns true if output[elem] is unique
    auto uniqueness_check = [&](indirect elem) -> bool { //
      return uniqueness_set.insert(elem).second;
    };

    // for when parts of a token are accumulated
    std::vector<char> fragment;

    // this lambda applies the operations specified in the args to a candidate token.
    // returns true iff this should be the last token added to the output
    auto process_token = [&](const char* begin, const char* end) -> bool {
      bool t_is_set = false;
      Token t;

      if (!fragment.empty()) {
        if (fragment.size() + (end - begin) > args.buf_size_frag) {
          drop_warning(args);
          fragment.clear();
          // still use an empty token to be consistent with the delimiters in the output
          end = begin;
        } else {
          str::append_to_buffer(fragment, begin, end);
          t.buffer = std::move(fragment);
          t_is_set = true;
          fragment = std::vector<char>();
          begin = &*t.buffer.cbegin();
          end = &*t.buffer.cend();
        }
      }

      auto check_unique_then_append = [&]() -> bool {
        output.push_back(std::move(t));
        if (args.unique || args.comp_unique) {
          // some form on uniqueness is being used
          if (!uniqueness_check(output.size() - 1)) {
            // the element is not unique. nothing was added to the uniqueness set
            output.pop_back();
            return false;
          }
        }
        return true;
      };

      for (OrderedOp& op : args.ordered_ops) {
        if (RmOrFilterOp* rf_op = std::get_if<RmOrFilterOp>(&op)) {
          if (rf_op->removes(begin, end)) {
            return false;
          }
        } else {
          if (tokens_not_stored && &op == &*args.ordered_ops.rbegin()) {
            if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
              auto direct_apply_sub = [&](FILE* out, const char* begin, const char* end) { //
                sub_op->direct_apply(out, begin, end);
              };
              if (is_sed) {
                direct_apply_sub(args.output, begin, end);
              } else {
                direct_output.write_output(begin, end, direct_apply_sub);
              }
            } else {
              IndexOp& in_op = std::get<IndexOp>(op);
              auto direct_apply_index = [&](FILE* out, const char* begin, const char* end) { //
                in_op.direct_apply(out, begin, end, direct_output.out_count);
              };
              if (is_sed) {
                direct_apply_index(args.output, begin, end);
              } else {
                direct_output.write_output(begin, end, direct_apply_index);
              }
            }
            goto after_direct_apply;
          } else {
            if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
              sub_op->apply(t.buffer, begin, end);
            } else {
              IndexOp& in_op = std::get<IndexOp>(op);
              if (!t_is_set) {
                str::append_to_buffer(t.buffer, begin, end);
              }
              in_op.apply(t.buffer, tokens_not_stored ? direct_output.out_count : output.size());
            }
            t_is_set = true;
            begin = &*t.buffer.cbegin();
            end = &*t.buffer.cend();
          }
        }
      }

      if (!tokens_not_stored && !t_is_set) {
        str::append_to_buffer(t.buffer, begin, end);
      }

      if (is_direct_output) {
        if (!tokens_not_stored) {
          if (!check_unique_then_append()) {
            return false;
          }
        }
        direct_output.write_output(begin, end);
      after_direct_apply:
        if (flush) {
          choose::str::flush_f(args.output);
        }
        if (direct_output.out_count == args.out || direct_output.out_count == args.in) {
          direct_output.finish_output();
          throw termination_request();
        }
        return false;
      } else {
        if (!check_unique_then_append()) {
          return false;
        }
        return output.size() == args.in;
      }
    };

    while (1) {
      char* write_pos = &subject[subject_size];
      size_t bytes_to_read = std::min(args.bytes_to_read, args.buf_size - subject_size);
      size_t bytes_read; // NOLINT
      bool input_done;   // NOLINT
      if (flush) {
        bytes_read = str::get_bytes_unbuffered(fileno(args.input), bytes_to_read, write_pos);
        input_done = bytes_read == 0;
      } else {
        bytes_read = str::get_bytes(args.input, bytes_to_read, write_pos);
        input_done = bytes_read != bytes_to_read;
      }
      subject_size += bytes_read;
      if (input_done) {
        // required to make end anchors like \Z match at the end of the input
        match_options &= ~PCRE2_PARTIAL_HARD;
      }

      // don't separate multibyte at end of subject
      const char* subject_effective_end; // NOLINT
      if (is_utf && !input_done) {
        subject_effective_end = str::utf8::last_completed_character_end(subject, subject + subject_size);
        if (subject_effective_end == NULL) {
          if (is_invalid_utf) {
            subject_effective_end = subject + subject_size;
          } else {
            throw std::runtime_error("utf8 decoding error");
          }
        }
      } else {
        subject_effective_end = subject + subject_size;
      }

    skip_read: // do another iteration but don't read in any more bytes

      int match_result;
      const char* single_byte_delimiter_pos; // points to position of match if match_result is 1
      if (single_byte_delimiter) {
        match_result = 0;
        single_byte_delimiter_pos = subject + prev_match_end;
        while (single_byte_delimiter_pos < subject + subject_size) {
          if (*single_byte_delimiter_pos == *args.in_byte_delimiter) {
            match_result = 1;
            break;
          }
          ++single_byte_delimiter_pos;
        }
      } else {
        match_result = regex::match(args.primary,                    //
                                    subject,                         //
                                    subject_effective_end - subject, //
                                    args.primary_data,               //
                                    id(is_match),                    //
                                    match_offset,                    //
                                    match_options);
      }

      if (match_result > 0) {
        // a complete match:
        // process the match, set the offsets, then do another iteration without
        // reading more input
        regex::Match match;
        if (single_byte_delimiter) {
          match = regex::Match{single_byte_delimiter_pos, single_byte_delimiter_pos + 1};
        } else {
          match = regex::get_match(subject, args.primary_data, id(is_match));
        }
        if (is_match) {
          if (is_sed) {
            // write everything before the match
            str::write_f(args.output, subject + prev_match_end, match.begin);
          }
          auto match_handler = [&](const regex::Match& m) -> bool { //
            return process_token(m.begin, m.end);
          };
          if (regex::get_match_and_groups(subject, match_result, args.primary_data, match_handler, "match pattern")) {
            break;
          }
        } else {
          if (process_token(subject + prev_match_end, match.begin)) {
            break;
          }
        }
        prev_match_end = match.end - subject;
        // set the start offset to just after the match
        match_offset = match.end - subject;
        goto skip_read;
      } else {
        if (!input_done) {
          // no or partial match and input is left
          const char* new_subject_begin;                    // NOLINT
          if (single_byte_delimiter || match_result == 0) { // single_byte_delimiter implies no partial match
            // there was no match but there is more input
            new_subject_begin = subject_effective_end;
          } else {
            // there was a partial match and there is more input
            regex::Match match = regex::get_match(subject, args.primary_data, id(is_match));
            new_subject_begin = match.begin;
          }

          // account for lookbehind bytes to retain prior to the match
          const char* new_subject_begin_cp = new_subject_begin;
          new_subject_begin -= args.max_lookbehind;
          if (new_subject_begin < subject) {
            new_subject_begin = subject;
          }
          if (is_utf) {
            // don't separate multibyte at begin of subject
            new_subject_begin = str::utf8::decrement_until_character_start(new_subject_begin, subject, subject_effective_end);
          }

          // pointer to begin of bytes retained, not including from the previous separator end
          const char* retain_marker = new_subject_begin;

          if (!is_match) {
            // keep the bytes required, either from the lookbehind retain,
            // or because the delimiter ended there
            const char* subject_const = subject;
            new_subject_begin = std::min(new_subject_begin, subject_const + prev_match_end);
          }

          // cut out the excess from the beginning and adjust the offsets
          match_offset = new_subject_begin_cp - new_subject_begin;

          if (is_sed) {
            if (subject + prev_match_end < new_subject_begin) {
              // write out the part that is being discarded
              str::write_f(args.output, subject + prev_match_end, new_subject_begin);
            }
          }
          prev_match_end -= new_subject_begin - subject;

          char* to = subject;
          const char* from = new_subject_begin;
          if (from != to) {
            while (from < subject + subject_size) {
              *to++ = *from++;
            }
            subject_size -= from - to;
          } else if (subject_size == args.buf_size) {
            // the buffer size has been filled

            auto clear_except_trailing_incomplete_multibyte = [&]() {
              if (is_utf //
                  && subject + subject_size != subject_effective_end //
                  && subject != subject_effective_end) {
                // "is_utf"
                //    in utf mode
                // "subject + subject_size != subject_effective_end"
                //    if the end of the buffer contains an incomplete multibyte
                // "subject != subject_effective_end"
                //    if clearing up to that point would do anything (entire buffer is incomplete multibyte)
                // then:
                //    clear the entire buffer not including the incomplete multibyte
                //    at the end (that wasn't used yet)
                subject_size = (subject + subject_size) - subject_effective_end;
                for (size_t i = 0; i < subject_size; ++i) {
                  subject[i] = subject[(args.buf_size - subject_size) + i];
                }
              } else {
                // clear the buffer
                subject_size = 0;
              }
            };

            if (is_match) {
              // count as match failure
              if (is_sed) {
                str::write_f(args.output, subject + prev_match_end, subject_effective_end);
                prev_match_end = 0;
              }
              match_offset = 0;
              clear_except_trailing_incomplete_multibyte();
            } else {
              // there is not enough room in the match buffer. moving the part
              // of the token at the beginning that won't be a part of a
              // successful match. moved to either a separate (fragment) buffer
              // or directly to the output

              auto process_fragment = [&](const char* begin, const char* end) {
                if (!has_ops && tokens_not_stored) {
                  direct_output.write_output_fragment(begin, end);
                } else {
                  if (fragment.size() + (end - begin) > args.buf_size_frag) {
                    drop_warning(args);
                    fragment.clear();
                  } else {
                    str::append_to_buffer(fragment, begin, end);
                  }
                }
              };

              if (prev_match_end != 0 || subject == retain_marker) {
                // the buffer is being retained because of lookbehind bytes.
                // can't properly match, so results in match failure
                process_fragment(subject + prev_match_end, subject_effective_end);
                prev_match_end = 0;
                match_offset = 0;
                clear_except_trailing_incomplete_multibyte();
              } else {
                // the buffer is being retained because of the previous delimiter
                // end position. write part
                process_fragment(subject, retain_marker);
                const char* remove_until = retain_marker;
                char* begin = subject;
                while (remove_until < subject + subject_size) {
                  *begin++ = *remove_until++;
                }
                subject_size -= remove_until - begin;
                match_offset = 0;
              }
            }
          }
        } else {
          // no match and no more input:
          // process the last token and break from the loop
          if (!is_match) {
            if (prev_match_end != subject_size || args.use_input_delimiter || !fragment.empty()) {
              // at this point subject_effective_end is subject + subject_size (since input_done)
              process_token(subject + prev_match_end, subject_effective_end);
            }
          } else if (is_sed) {
            str::write_f(args.output, subject + prev_match_end, subject_effective_end);
          }
          break;
        }
      }
    }

    if (is_direct_output) {
      direct_output.finish_output();
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
  } // scope for goto

  if (output.size() > args.out) {
    output.resize(args.out);
  }

skip_all:

  if (!args.tui) {
    for (const Token& t : output) {
      direct_output.write_output(t);
    }
    direct_output.finish_output();
    throw termination_request();
  }

  return output;
}

} // namespace choose
