#pragma once
#include <algorithm>
#include <cassert>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "args.hpp"
#include "regex.hpp"
#include "string_utils.hpp"

/*
There's a lot going on in this file. It should have complete code coverage. View with:

cd build
cmake .. -DBUILD_TESTING=true -DCODE_COVERAGE=true
make cov-clean && make cov-show
*/

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
  // disambiguate between out_count of zero vs overflow to zero
  bool has_written = false;

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
    if (delimit_required_ && !args.sed) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = false;
    has_written = true;
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
    if (delimit_required_ && !args.sed) {
      str::write_f(args.output, args.out_delimiter);
    }
    delimit_required_ = true;
    has_written = true;
    handler(args.output, begin, end);
    ++out_count;
  }

  void write_output(const Token& t) { //
    write_output(&*t.buffer.cbegin(), &*t.buffer.cend());
  }

  // call after all other writing has finished
  void finish_output() {
    if (!args.delimit_not_at_end && (has_written || args.delimit_on_empty) && !args.sed) {
      str::write_f(args.output, args.bout_delimiter);
    }
    delimit_required_ = false; // optional reset of state
    has_written = false;
    out_count = 0;
  }
};

// writes an output delimiter between tokens,
// and a batch delimiter between batches and at the end
struct BatchOutputStream {
  bool first_within_batch = true;
  bool first_batch = true;

  const Arguments& args;
  str::QueuedOutput qo;

  BatchOutputStream(const Arguments& args)
      : args(args),                                        //
        qo{isatty(fileno(args.output)) && args.tenacious ? // NOLINT args.output can never by null here
               std::optional<std::vector<char>>(std::vector<char>())
                                                         : std::nullopt} {}

  void write_output(const Token& t) {
    if (!first_within_batch) {
      qo.write_output(args.output, args.out_delimiter);
    } else if (!first_batch) {
      qo.write_output(args.output, args.bout_delimiter);
    }
    first_within_batch = false;
    qo.write_output(args.output, t.buffer);
  }

  void finish_batch() {
    first_batch = false;
    first_within_batch = true;
  }

  void finish_output() {
    if (!args.delimit_not_at_end && (!first_batch || args.delimit_on_empty)) {
      qo.write_output(args.output, args.bout_delimiter);
    }
    qo.flush_output(args.output);
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
  const bool is_utf = args.primary ? regex::options(args.primary) & PCRE2_UTF : false;
  const bool is_invalid_utf = args.primary ? regex::options(args.primary) & PCRE2_MATCH_INVALID_UTF : false;
  regex::match_data primary_data = args.primary ? regex::create_match_data(args.primary) : NULL;

  // single_byte_delimiter implies not match. stating below so the compiler can hopefully leverage it
  const bool is_match = !single_byte_delimiter && args.match;
  const bool is_direct_output = args.is_direct_output();
  // sed implies is_direct_output and is_match
  const bool is_sed = is_direct_output && is_match && args.sed;
  const bool tokens_not_stored = args.tokens_not_stored();
  const bool has_ops = !args.ordered_ops.empty();
  const bool flush = args.flush;

  const bool is_unique = args.unique;
  const bool lex_unique_use_set = args.lex_unique_use_set;
  const bool is_comp_unique = args.comp_unique;

  regex::match_data comp_data = args.comp ? regex::create_match_data(args.comp) : NULL;

  char subject[args.buf_size]; // match buffer
  size_t subject_size = 0;     // how full is the buffer
  PCRE2_SIZE match_offset = 0;
  PCRE2_SIZE prev_sep_end = 0; // only used if !args.match
  uint32_t match_options = PCRE2_PARTIAL_HARD;

  TokenOutputStream direct_output(args); //  if is_direct_output, this is used
  std::vector<Token> output;             // !tokens_not_stored, this is used

  if (args.out == 0) {
    // edge case on logic. it adds a token, then checks if the out limit has been hit
    goto skip_all;
  }

  {
    auto user_defined_comparison = [&](const Token& lhs_arg, const Token& rhs_arg) -> bool {
      const Token* lhs = &lhs_arg;
      const Token* rhs = &rhs_arg;

      int lhs_result = regex::match(args.comp, lhs->buffer.data(), lhs->buffer.size(), comp_data, "user comp");
      int rhs_result = regex::match(args.comp, rhs->buffer.data(), rhs->buffer.size(), comp_data, "user comp");
      if (lhs_result && !rhs_result) {
        return 1;
      } else {
        return 0;
      }
    };

    auto user_defined_uniqueness_set_comp = [&](indirect lhs, indirect rhs) -> bool { //
      return user_defined_comparison(output[lhs], output[rhs]);
    };

    auto lexicographical_comparison = [&](const Token& lhs_arg, const Token& rhs_arg) -> bool {
      const Token* lhs = &lhs_arg;
      const Token* rhs = &rhs_arg;
      return std::lexicographical_compare( //
          lhs->buffer.cbegin(), lhs->buffer.cend(), rhs->buffer.cbegin(), rhs->buffer.cend());
    };

    auto lexicographical_uniqueness_set_comp = [&](indirect lhs, indirect rhs) -> bool { //
      return lexicographical_comparison(output[lhs], output[rhs]);
    };

    auto unordered_set_hash = [&](indirect val) -> size_t {
      const Token& t = output[val];
      auto view = std::string_view(t.buffer.data(), t.buffer.size());
      return std::hash<std::string_view>{}(view);
    };

    auto unordered_set_equals = [&](indirect lhs, indirect rhs) -> bool { //
      return output[lhs] == output[rhs];
    };

    // uniqueness is applied with an unordered_set if lexicographical comparison
    // is used. in contrast, the user defined comparison uses a std::set, as it
    // is easier in the args to specify a comparison with regex compared to a
    // hash with regex.
    using user_defined_uniqueness_set = std::set<indirect, decltype(user_defined_uniqueness_set_comp)>;
    using lexicographical_uniqueness_set = std::set<indirect, decltype(lexicographical_uniqueness_set_comp)>;
    using unordered_set_T = std::unordered_set<indirect, decltype(unordered_set_hash), decltype(unordered_set_equals)>;
    using unique_checker_T = std::variant<std::monostate, user_defined_uniqueness_set, lexicographical_uniqueness_set, unordered_set_T>;

    unique_checker_T unique_checker = [&]() -> unique_checker_T {
      if (is_comp_unique) {
        return unique_checker_T(user_defined_uniqueness_set(user_defined_uniqueness_set_comp));
      } else if (is_unique) {
        if (lex_unique_use_set) {
          return unique_checker_T(lexicographical_uniqueness_set(lexicographical_uniqueness_set_comp));
        } else {
          auto s = unordered_set_T(8, unordered_set_hash, unordered_set_equals);
          s.max_load_factor(0.125); // obtained experimentally. see perf.md
          return unique_checker_T(std::move(s));
        }
      } else {
        return unique_checker_T();
      }
    }();

    // returns true if output[elem] is unique
    auto uniqueness_check = [&](indirect elem) -> bool { //
      if (unordered_set_T* set = std::get_if<unordered_set_T>(&unique_checker)) {
        return set->insert(elem).second;
      } else if (lexicographical_uniqueness_set* set = std::get_if<lexicographical_uniqueness_set>(&unique_checker)) {
        return set->insert(elem).second;
      } else {
        user_defined_uniqueness_set& uniqueness_set = std::get<user_defined_uniqueness_set>(unique_checker);
        return uniqueness_set.insert(elem).second;
      }
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
        } else if (InLimitOp* rf_op = std::get_if<InLimitOp>(&op)) {
          if (rf_op->finished()) {
            return true;
          }
        } else {
          if (tokens_not_stored && &op == &*args.ordered_ops.rbegin()) {
            if (ReplaceOp* rep_op = std::get_if<ReplaceOp>(&op)) {
              auto direct_apply_replace = [&](FILE* out, const char*, const char*) { //
                rep_op->direct_apply(out, subject, subject_size, primary_data, args.primary);
              };
              direct_output.write_output(begin, end, direct_apply_replace);
            } else if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
              auto direct_apply_sub = [&](FILE* out, const char* begin, const char* end) { //
                sub_op->direct_apply(out, begin, end);
              };
              direct_output.write_output(begin, end, direct_apply_sub);
            } else {
              IndexOp& in_op = std::get<IndexOp>(op);
              auto direct_apply_index = [&](FILE* out, const char* begin, const char* end) { //
                in_op.direct_apply(out, begin, end, direct_output.out_count);
              };
              direct_output.write_output(begin, end, direct_apply_index);
            }
            goto after_direct_apply;
          } else {
            if (ReplaceOp* rep_op = std::get_if<ReplaceOp>(&op)) {
              rep_op->apply(t.buffer, subject, subject + subject_size, primary_data, args.primary);
            } else if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
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
        begin = &*t.buffer.cbegin(); // not needed since it now points to a copy
        end = &*t.buffer.cend();     // but keeps things clean
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
        if (direct_output.out_count == args.out) {
          // code coverage reaches here. mistakenly shows finish_output as
          // unreached but throw is reached. weird.
          direct_output.finish_output();
          throw termination_request();
        }
        return false;
      } else {
        check_unique_then_append(); // result ignored
        return false;
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

      int match_result;                      // NOLINT
      const char* single_byte_delimiter_pos; // NOLINT points to position of match if match_result is 1
      if (single_byte_delimiter) {
        match_result = 0;
        single_byte_delimiter_pos = subject + prev_sep_end;
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
                                    primary_data,                    //
                                    id(is_match),                    //
                                    match_offset,                    //
                                    match_options);
      }

      if (match_result > 0) {
        // a complete match:
        // process the match, set the offsets, then do another iteration without
        // reading more input
        regex::Match match; // NOLINT
        if (single_byte_delimiter) {
          match = regex::Match{single_byte_delimiter_pos, single_byte_delimiter_pos + 1};
        } else {
          match = regex::get_match(subject, primary_data, id(is_match));
          if (match.begin == match.end) {
            match_options |= PCRE2_NOTEMPTY_ATSTART;
          } else {
            match_options &= ~PCRE2_NOTEMPTY_ATSTART;
          }
        }
        if (is_match) {
          if (is_sed) {
            str::write_f(args.output, subject + match_offset, match.begin);
            if (process_token(match.begin, match.end)) {
              break;
            }
          } else {
            auto match_handler = [&](const regex::Match& m) -> bool { //
              return process_token(m.begin, m.end);
            };
            if (regex::get_match_and_groups(subject, match_result, primary_data, match_handler, "match pattern")) {
              break;
            }
          }
        } else {
          if (process_token(subject + prev_sep_end, match.begin)) {
            break;
          }
          prev_sep_end = match.end - subject;
        }
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
            regex::Match match = regex::get_match(subject, primary_data, id(is_match));
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
            new_subject_begin = std::min(new_subject_begin, subject_const + prev_sep_end);
          }

          // cut out the excess from the beginning and adjust the offsets
          size_t old_match_offset = match_offset;
          match_offset = new_subject_begin_cp - new_subject_begin;
          if (!is_match) {
            prev_sep_end -= new_subject_begin - subject;
          } else if (is_sed) {
            // write out the part that is being discarded
            const char* begin = subject + old_match_offset;
            const char* end = new_subject_begin + match_offset;
            if (begin < end) {
              str::write_f(args.output, begin, end);
            }
          }

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
              if (is_utf                                             //
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
                if (is_sed) {
                  str::write_f(args.output, subject + match_offset, subject_effective_end);
                }
                subject_size = (subject + subject_size) - subject_effective_end;
                for (size_t i = 0; i < subject_size; ++i) {
                  subject[i] = subject[(args.buf_size - subject_size) + i];
                }
              } else {
                // clear the buffer
                if (is_sed) {
                  str::write_f(args.output, subject + match_offset, subject + subject_size);
                }
                subject_size = 0;
              }
            };

            if (is_match) {
              // count as match failure
              clear_except_trailing_incomplete_multibyte();
              match_offset = 0;
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

              if (prev_sep_end != 0 || subject == retain_marker) {
                // the buffer is being retained because of lookbehind bytes.
                // can't properly match, so results in match failure
                process_fragment(subject + prev_sep_end, subject_effective_end);
                clear_except_trailing_incomplete_multibyte();
                prev_sep_end = 0;
                match_offset = 0;
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
            if (prev_sep_end != subject_size || args.use_input_delimiter || !fragment.empty()) {
              // at this point subject_effective_end is subject + subject_size (since input_done)
              process_token(subject + prev_sep_end, subject_effective_end);
            }
          } else if (is_sed) {
            str::write_f(args.output, subject + match_offset, subject_effective_end);
          }
          break;
        }
      }
    }

    if (is_direct_output) {
      direct_output.finish_output();
      throw termination_request();
    }

    if (unordered_set_T* uniqueness_unordered_set = std::get_if<unordered_set_T>(&unique_checker)) {
      uniqueness_unordered_set->clear();
    } else if (user_defined_uniqueness_set* set = std::get_if<user_defined_uniqueness_set>(&unique_checker)) {
      set->clear();
    } else if (lexicographical_uniqueness_set* set = std::get_if<lexicographical_uniqueness_set>(&unique_checker)) {
      set->clear();
    }

    if (args.out.has_value() && output.size() > *args.out && args.sort && !args.comp_sort) {
      // if lexicographically sorting and the output is being truncated then do a
      // partial sort instead. can only be applied to lexicographical since there's no
      // stable partial sort (and stability is required for user defined comp sort)
      std::partial_sort(output.begin(), output.begin() + *args.out, output.end(), lexicographical_comparison);
    } else {
      if (args.comp_sort) {
        std::stable_sort(output.begin(), output.end(), user_defined_comparison);
      } else if (args.sort) {
        std::sort(output.begin(), output.end(), lexicographical_comparison);
      }
    }

    if (args.reverse) {
      std::reverse(output.begin(), output.end());
    }

    if (args.out.has_value() && output.size() > *args.out) {
      output.resize(*args.out);
    }

  } // scope for goto

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
