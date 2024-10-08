#pragma once
#include <algorithm>
#include <execution>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "algo_utils.hpp"
#include "args.hpp"
#include "regex.hpp"
#include "string_utils.hpp"
#include "termination_request.hpp"

/*
There's a lot going on in this file. It should have complete code coverage. View with:

cd build
cmake .. -DBUILD_TESTING=true -DCODE_COVERAGE=true
make cov-clean && make cov-show
*/

#ifdef OUTPUT_SIZE_BOUND_TESTING
extern std::optional<size_t> output_size_bound_testing; // NOLINT
#endif

namespace choose {

struct Token {
  std::vector<char> buffer; // NOLINT

  // ctor for testing
  Token(const char* in)
      : buffer(in, in + strlen(in))
#ifndef CHOOSE_DISABLE_FIELD
        ,
        field_begin(&*buffer.cbegin()),
        field_end(&*buffer.cend())
#endif
  {
  }

  // ctor for testing
  Token(std::vector<char>&& i)
      : buffer(std::move(i))
#ifndef CHOOSE_DISABLE_FIELD
        ,
        field_begin(&*buffer.cbegin()),
        field_end(&*buffer.cend())
#endif
  {
  }

  Token() = default;
  Token(const Token&) = default;
  Token(Token&&) = default;
  Token& operator=(const Token&) & = default;
  Token& operator=(Token&&) & = default;
  ~Token() = default;

  const char* cbegin() const {
#ifndef CHOOSE_DISABLE_FIELD
    return this->field_begin;
#else
    return &*this->buffer.cbegin();
#endif
  }

  const char* cend() const {
#ifndef CHOOSE_DISABLE_FIELD
    return this->field_end;
#else
    return &*this->buffer.cend();
#endif
  }

#ifndef CHOOSE_DISABLE_FIELD
  void set_field(const regex::code& code, const regex::match_data& data) {
    if (!code) {
      this->field_begin = &*buffer.cbegin();
      this->field_end = &*buffer.cend();
      return;
    }
    int rc = regex::match(code, buffer.data(), buffer.size(), data, "token field");
    if (rc > 0) {
      regex::Match m = regex::get_match(buffer.data(), data, "token field");
      this->field_begin = m.begin;
      this->field_end = m.end;
    } else {
      // already initialized field_begin and field_end to nulls (no match = empty string)
    }
  }

  // point to range in buffer, a special field of interest
  const char* field_begin = 0;
  const char* field_end = 0;
#endif
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

  bool begin_discard() const { //
    return this->args.out_start && this->out_count < *this->args.out_start;
  }

  // write a part of a token to the output.
  // the last part of a token must instead use write_output
  void write_output_fragment(const char* begin, const char* end) {
    if (!begin_discard()) {
      if (delimit_required_ && !args.sed) {
        str::write_f(args.output, args.out_delimiter);
      }
      delimit_required_ = false;
      has_written = true;
    }
    str::write_f(args.output, begin, end);
  }

  template <typename T = decltype(TokenOutputStream::default_write)>
  void write_output_no_truncate(const char* begin, //
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

  // write a part or whole of a token to the output.
  // if it is a part, then it must be the last part.
  // pass a handler void(FILE* out, const char* begin, const char* end).
  // the function will write the token to the output after applying transformations
  template <typename T = decltype(TokenOutputStream::default_write)>
  void write_output(const char* begin, //
                    const char* end,
                    T handler = TokenOutputStream::default_write) {
    if (!begin_discard()) {
      write_output_no_truncate(begin, end, handler);
    } else {
      ++out_count;
    }
  }

  void write_output_no_truncate(const Token& t) { //
    write_output_no_truncate(&*t.buffer.cbegin(), &*t.buffer.cend());
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

namespace {

const char* id(bool is_match) {
  if (is_match) {
    return "match pattern";
  } else {
    return "input delimiter";
  }
}

using indirect = std::vector<Token>::size_type; // an index into output

// various comparison related functions

bool lexicographical_comparison(const Token& lhs, const Token& rhs) { //
  return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

bool numeric_comparison(const Token& lhs, const Token& rhs) { //
  return numeric_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

bool general_numeric_comparison(const Token& lhs, const Token& rhs) { //
  return general_numeric_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

} // namespace

struct CreateTokensResult {
  std::vector<Token> tokens;
  // used for --tui-select
  std::optional<Token> initial_selected_token = {};
};

// reads from args.input
// if args.tui:
//      returns the tokens
// else
//      writes to args.output, then throws a termination_request exception,
//      which the caller should handle (exit unless unit test)
CreateTokensResult create_tokens(choose::Arguments& args) {
  const bool single_byte_delimiter = args.in_byte_delimiter.has_value();
  const bool is_utf = args.primary ? regex::options(args.primary) & PCRE2_UTF : false;
  const bool is_invalid_utf = args.primary ? regex::options(args.primary) & PCRE2_MATCH_INVALID_UTF : false;
  regex::match_data primary_data = args.primary ? regex::create_match_data(args.primary) : NULL;
#ifndef CHOOSE_DISABLE_FIELD
  regex::match_data field_data = args.field ? regex::create_match_data(args.field) : NULL;
#endif

  // single_byte_delimiter implies not match. stating below so the compiler can hopefully leverage it
  const bool is_match = !single_byte_delimiter && args.match;
  const bool is_direct_output = args.is_direct_output();
  // sed implies is_direct_output and is_match
  const bool is_sed = is_direct_output && is_match && args.sed;
  const bool tokens_not_stored = args.tokens_not_stored();
  const bool has_ops = !args.ordered_ops.empty();
  const bool flush = args.flush;
  const bool tail = args.tail;

  const bool unique = args.unique;
  const Comparison unique_type = args.unique_type;
  const bool sort = args.sort;
  const Comparison sort_type = args.sort_type;
  const bool sort_reversed = args.sort_reverse;
  const bool mem_is_bounded = args.mem_is_bounded();

  char subject[args.buf_size]; // match buffer
  size_t subject_size = 0;     // how full is the buffer
  PCRE2_SIZE match_offset = 0;
  PCRE2_SIZE prev_sep_end = 0; // only used if !args.match
  uint32_t match_options = PCRE2_PARTIAL_HARD;

  TokenOutputStream direct_output(args); //  if is_direct_output, this is used

  // fields for CreateTokensResult
  std::optional<Token> initial_selected_token = {}; // !tokens_not_stored, these two are used
  std::vector<Token> output;

  if (args.out_end == 0) {
    // edge case on logic. it adds a token, then checks if the out limit has been hit
    goto skip_all;
  }

  {
    auto uniqueness_set_comparison = [&](indirect lhs, indirect rhs) -> bool {
      switch (unique_type) {
        default:
          return lexicographical_comparison(output[lhs], output[rhs]);
          break;
        case numeric:
          return numeric_comparison(output[lhs], output[rhs]);
          break;
        case general_numeric:
          return general_numeric_comparison(output[lhs], output[rhs]);
          break;
      }
    };

    auto sort_comparison = [&](const Token& lhs_arg, const Token& rhs_arg) -> bool {
      const Token* lhs = &lhs_arg;
      const Token* rhs = &rhs_arg;
      if (sort_reversed) {
        std::swap(lhs, rhs);
      }
      switch (sort_type) {
        default:
          return lexicographical_comparison(*lhs, *rhs);
          break;
        case numeric:
          return numeric_comparison(*lhs, *rhs);
          break;
        case general_numeric:
          return general_numeric_comparison(*lhs, *rhs);
          break;
      }
    };

    auto unordered_set_hash = [&](indirect val) -> size_t {
      const Token& t = output[val];
      switch (unique_type) {
        default: {
          auto view = std::string_view(t.cbegin(), t.cend() - t.cbegin());
          return std::hash<std::string_view>{}(view);
        } break;
        case numeric:
          return numeric_hash(t.cbegin(), t.cend());
          break;
        case general_numeric:
          return general_numeric_hash(t.cbegin(), t.cend());
          break;
      }
    };

    auto equality_predicate = [&](const Token& lhs, const Token& rhs) -> bool { //
      switch (unique_type) {
        default:
          return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
          break;
        case numeric:
          return numeric_equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
          break;
        case general_numeric:
          return general_numeric_equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
          break;
      }
    };

    auto unordered_set_equals = [&](indirect lhs_arg, indirect rhs_arg) -> bool { //
      return equality_predicate(output[lhs_arg], output[rhs_arg]);
    };

    using unordered_uniqueness_set_T = std::unordered_set<indirect, decltype(unordered_set_hash), decltype(unordered_set_equals)>;
    using uniqueness_set_T = std::set<indirect, decltype(uniqueness_set_comparison)>;
    using unique_checker_T = std::variant<std::monostate, unordered_uniqueness_set_T, uniqueness_set_T>;

    unique_checker_T unique_checker = [&]() -> unique_checker_T {
      if (unique) {
        if (args.unique_use_set) {
          return unique_checker_T(uniqueness_set_T(uniqueness_set_comparison));
        } else {
          auto s = unordered_uniqueness_set_T(8, unordered_set_hash, unordered_set_equals);
          s.max_load_factor(args.unique_load_factor);
          return unique_checker_T(std::move(s));
        }
      } else {
        return unique_checker_T();
      }
    }();

    // returns true if output[elem] is unique. requires unique == true
    auto uniqueness_check = [&](indirect elem) -> bool { //
      if (unordered_uniqueness_set_T* set = std::get_if<unordered_uniqueness_set_T>(&unique_checker)) {
        return set->insert(elem).second;
      } else {
        return std::get<uniqueness_set_T>(unique_checker).insert(elem).second;
      }
    };

    // for when parts of a token are accumulated.
    // this is neccesary when there isn't enough room in the match buffer
    std::vector<char> fragment;

    // this lambda applies the operations specified in the args to a candidate token.
    // returns true iff this should be the last token added to the output
    auto process_token = [&](const char* begin, const char* end) -> bool {
      // the ops can be thought of as being applied in a pipeline, begin to end is the
      // range of bytes currently being worked with; it is the thing being passed from
      // the output of one op to the input of the next op. begin to end first points to
      // memory existing in the match buffer. this buffer will get overwritten on the
      // next match iteration, so it can be considered temporary. some ops need to
      // store the result somewhere. they will take an input (begin to end) and place
      // the result in t, a token (vec of chars). the next op will receive begin to end,
      // but now begin and end will have been set to point within t.
      bool t_is_set = false;
      Token t;

      bool token_is_selected = false; // for --tui-select

      if (!fragment.empty()) {
        if (fragment.size() + (end - begin) > args.buf_size_frag) {
          args.drop_warning();
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

      // moves from t. returns true if the output's size increased
      auto check_unique_then_append = [&]() -> bool {
#ifndef CHOOSE_DISABLE_FIELD
        t.set_field(args.field, field_data);
#endif
        if (!mem_is_bounded) {
          // typical case
          output.push_back(std::move(t));
          if (unique) {
            if (!uniqueness_check(output.size() - 1)) {
              // the element is not unique. nothing was added to the uniqueness set
              output.pop_back();
              return false;
            }
          }
        } else {
          // output size is bounded. from --tail or --out
          if (sort) {
            // note that the sorting is reversed if tail is used. so this
            // handles tail and non tail cases. see UncompiledCodes.

            // note also that mem_is_bounded means sort_comparison and equality_predicate
            // are using the same sort / unique comparison type (otherwise the next lines are confusing).
            // see Arguments::mem_is_bounded
            auto insertion_pos = std::upper_bound(output.begin(), output.end(), t, sort_comparison);
            if (!unique || (insertion_pos == output.begin() || !equality_predicate(insertion_pos[-1], t))) {
              // uniqueness is not used, or t does not yet exist in output
              if (likely(output.size() == *args.out_end)) {
                while (insertion_pos < output.end()) {
                  std::swap(*insertion_pos++, t);
                }
                return false;
              } else {
                output.insert(insertion_pos, std::move(t));
              }
            } else {
              // unique is being used and t already exists in the output
              return false;
            }
          } else {
            // unsorted memory bounded case.
            // precondition unique is false (can't be applied in a mem bounded way)
            if (tail && likely(output.size() == *args.out_end)) {
              // same reasoning as above. fixed length buffer being moved around
              auto it = output.rbegin();
              while (it != output.rend()) {
                std::swap(*it++, t);
              }
            } else {
              output.push_back(std::move(t));
              // for non tail case caller looks at size of output to determine
              // if finished
            }
          }
        }
#ifdef OUTPUT_SIZE_BOUND_TESTING
        if (output_size_bound_testing && output.size() > *output_size_bound_testing) {
          throw std::runtime_error("max output size exceeded!\n");
        }
#endif
        return true;
      };

      bool ret = false; // return value

      for (OrderedOp& op : args.ordered_ops) {
        if (RmOrFilterOp* rf_op = std::get_if<RmOrFilterOp>(&op)) {
          if (rf_op->removes(begin, end)) {
            return false;
          }
        } else if (InLimitOp* head_op = std::get_if<InLimitOp>(&op)) {
          switch (head_op->apply()) {
            case InLimitOp::REMOVE:
              return false;
              break;
            case InLimitOp::DONE:
              return true;
              break;
            default:
              break;
          }
        } else if (TuiSelectOp* tui_select_op = std::get_if<TuiSelectOp>(&op)) {
          if (!initial_selected_token.has_value() && tui_select_op->matches(begin, end)) {
            // set the cursor to here, only if it makes it through all following ops
            token_is_selected = true;
          }
        } else {
          if (tokens_not_stored && &op == &*args.ordered_ops.rbegin()) {
            if (ReplaceOp* rep_op = std::get_if<ReplaceOp>(&op)) {
              std::vector<char> out;
              rep_op->apply(out, subject, subject + subject_size, primary_data, args.primary);
              direct_output.write_output(&*out.cbegin(), &*out.cend());
            } else if (SubOp* sub_op = std::get_if<SubOp>(&op)) {
              auto direct_apply_sub = [&](FILE* out, const char* begin, const char* end) { //
                sub_op->direct_apply(out, begin, end);
              };
              direct_output.write_output(begin, end, direct_apply_sub);
            } else {
              IndexOp& in_op = std::get<IndexOp>(op);
              auto direct_apply_index = [&](FILE* out, const char* begin, const char* end) { //
                in_op.direct_apply(out, begin, end);
              };
              direct_output.write_output(begin, end, direct_apply_index);
            }
            // shortcut. the above ops wrote directly to the output
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
              in_op.apply(t.buffer);
            }
            t_is_set = true;
            begin = &*t.buffer.cbegin();
            end = &*t.buffer.cend();
          }
        }
      }

      // if the tokens haven't been stored yet by the ops above,
      // and a token t is needed
      if (!tokens_not_stored && !t_is_set) {
        str::append_to_buffer(t.buffer, begin, end);
        begin = &*t.buffer.cbegin();
        end = &*t.buffer.cend();
      }

      if (is_direct_output) {
        if (!tokens_not_stored) {
          if (!check_unique_then_append()) {
            ret = false;
            goto end;
          }
        }
        direct_output.write_output(begin, end);
after_direct_apply:
        if (flush) {
          choose::str::flush_f(args.output);
        }
        if (direct_output.out_count == args.out_end) {
          // code coverage reaches here. mistakenly shows finish_output as
          // unreached but throw is reached. weird.
          direct_output.finish_output();
          throw termination_request();
        }
        ret = false;
        goto end;
      } else {
        check_unique_then_append(); // result ignored
        // handle the case mentioned in check_unique_then_append
        if (mem_is_bounded && !sort && !tail) {
          if (output.size() == *args.out_end) {
            ret = true;
            goto end;
          }
        }
        ret = false;
        goto end;
      }

end:
      if (unlikely(token_is_selected && !initial_selected_token.has_value())) {
        Token selected;
        // manual copy here (since copying is disabled on tokens otherwise)
        selected.buffer = output.rbegin()->buffer;
#ifndef CHOOSE_DISABLE_FIELD
        selected.field_begin = output.rbegin()->field_begin;
        selected.field_end = output.rbegin()->field_end;
#endif
        initial_selected_token = selected;
      }
      return ret;
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
            // counting this as a regex failure for the fuzzer to catch
            throw regex::regex_failure("utf8 decoding error");
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
          if (new_subject_begin - subject < args.max_lookbehind) {
            new_subject_begin = subject;
          } else {
            new_subject_begin -= args.max_lookbehind;
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
                    args.drop_warning();
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

    if (unordered_uniqueness_set_T* uniqueness_unordered_set = std::get_if<unordered_uniqueness_set_T>(&unique_checker)) {
      uniqueness_unordered_set->clear();
    } else if (uniqueness_set_T* set = std::get_if<uniqueness_set_T>(&unique_checker)) {
      set->clear();
    }

#ifdef CHOOSE_FUZZING_APPLIED
    auto policy = std::execution::seq; // tbb false positive
#else
    auto policy = std::execution::par_unseq;
#endif

    if (!args.out_start && !args.out_end) {
      // no truncation needed. this is the simplest case
      if (args.sort) {
        if (args.sort_stable) {
          std::stable_sort(policy, output.begin(), output.end(), sort_comparison);
        } else {
          std::sort(policy, output.begin(), output.end(), sort_comparison);
        }
      }
    } else {
      // truncate the ends
      if (mem_is_bounded) {
        // sort and end truncation has already been applied
      } else {
        // truncate the end
        if (tail && !sort) {
          // !sort since tail reversed the sorting order. see UncompiledCodes
          // truncate based on tail
          if (*args.out_end < output.size()) {
            output.erase(output.begin(), output.end() - *args.out_end); // NOLINT
          }
        } else {
          typename std::vector<Token>::iterator middle;
          middle = output.begin() + *args.out_end; // NOLINT
          if (middle > output.end()) {
            middle = output.end();
          }
          if (args.sort) {
            if (args.sort_stable) {
              stable_partial_sort(policy, output.begin(), middle, output.end(), sort_comparison);
            } else {
              std::partial_sort(policy, output.begin(), middle, output.end(), sort_comparison);
            }
          }
          output.resize(middle - output.begin());
        }
      }
      // truncate the beginning
      if (args.out_start) {
        if (tail && !sort) {
          if (*args.out_start < output.size()) {
            output.erase(output.end() - *args.out_start, output.end()); // NOLINT
          } else {
            output.clear();
          }
        } else {
          if (*args.out_start < output.size()) {
            output.erase(output.begin(), output.begin() + *args.out_start); // NOLINT
          } else {
            output.clear();
          }
        }
      }
    }

    // applied last
    if (args.flip) {
      std::reverse(output.begin(), output.end());
    }
  } // scope for goto

skip_all:

  if (!args.tui) {
    for (const Token& t : output) {
      // don't apply truncation again since it was already done above
      direct_output.write_output_no_truncate(t);
    }
    direct_output.finish_output();
    throw termination_request();
  }

  return CreateTokensResult{std::move(output), std::move(initial_selected_token)};
}

} // namespace choose
