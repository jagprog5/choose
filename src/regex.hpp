#pragma once

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <functional>
#include <memory>
#include <stdexcept>

namespace choose {

namespace regex {

struct code_destroyer {
  void operator()(pcre2_code* code) {
    pcre2_code_free(code); // library does null check
  }
};

struct match_data_destroyer {
  void operator()(pcre2_match_data* match_data) {
    pcre2_match_data_free(match_data); // library does null check
  }
};

using code = std::unique_ptr<pcre2_code, code_destroyer>;
using match_data = std::unique_ptr<pcre2_match_data, match_data_destroyer>;

void apply_null_guard(const char*& pattern, PCRE2_SIZE size) {
  // guard against what is detected as an error in PCRE2 but I don't agree with
  // https://github.com/PCRE2Project/pcre2/issues/270
  if (pattern == NULL && size == 0) {
    pattern = (const char*)1;
  }
}

code compile(const char* pattern, uint32_t options, const char* identification, uint32_t jit_options = PCRE2_JIT_COMPLETE, PCRE2_SIZE size = PCRE2_ZERO_TERMINATED) {
  apply_null_guard(pattern, size);
  int error_number;        // NOLINT
  PCRE2_SIZE error_offset; // NOLINT
  pcre2_code* re = pcre2_compile((PCRE2_SPTR)pattern, size, options, &error_number, &error_offset, NULL);
  if (re == NULL) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(error_number, buffer, sizeof(buffer));
    char msg[512];
    snprintf(msg, 512, "PCRE2 compilation in %s failed at offset %d: %s", identification, (int)error_offset, buffer);
    throw std::runtime_error(msg);
  }
  // ignore the return code here. falls back to normal code
  pcre2_jit_compile(re, jit_options);
  return code(re);
}

code compile(const std::vector<char>& pattern, uint32_t options, const char* identification, uint32_t jit_options = PCRE2_JIT_COMPLETE) {
  return compile(pattern.data(), options, identification, jit_options, pattern.size());
}

match_data create_match_data(const code& code) {
  pcre2_match_data* data = pcre2_match_data_create_from_pattern(code.get(), NULL);
  if (data == NULL) {
    throw std::runtime_error("PCRE2 err");
  }
  return match_data(data);
}

// returns -1 if partial matching was specified in match_options and the subject is a partial match.
// returns 0 if there are no matches, else 1 + the number of groups
int match(const code& re, //
          const char* subject,
          PCRE2_SIZE subject_length,
          const match_data& match_data,
          const char* identification,
          PCRE2_SIZE start_offset = 0,
          uint32_t match_options = 0) {
  apply_null_guard(subject, subject_length);
  int rc = pcre2_match(re.get(), (PCRE2_SPTR)subject, subject_length, start_offset, match_options, match_data.get(), NULL);
  if (rc == PCRE2_ERROR_PARTIAL) {
    return -1;
  } else if (rc == PCRE2_ERROR_NOMATCH) {
    return 0;
  } else if (rc <= 0) {
    // < 0 is a regex error
    // = 0 means the match_data ovector wasn't big enough
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(rc, buffer, sizeof(buffer));
    char msg[512];
    snprintf(msg, 512, "Matching error in %s: %s", identification, buffer);
    throw std::runtime_error(msg);
  }
  return rc;
}

uint32_t options(const code& c) {
  uint32_t options; // NOLINT
  pcre2_pattern_info(c.get(), PCRE2_INFO_ALLOPTIONS, &options);
  return options;
}

uint32_t max_lookbehind_size(const code& c) {
  uint32_t out; // NOLINT
  pcre2_pattern_info(c.get(), PCRE2_INFO_MAXLOOKBEHIND, &out);
  return out;
}

uint32_t min_match_length(const code& c) {
  uint32_t out; // NOLINT
  pcre2_pattern_info(c.get(), PCRE2_INFO_MINLENGTH, &out);
  return out;
}

// replacement is null terminating
std::vector<char> substitute_global(const code& re, //
                                    const char* subject,
                                    PCRE2_SIZE subject_length,
                                    const char* replacement) {
  apply_null_guard(subject, subject_length);
  uint32_t sub_flags = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
#ifdef PCRE2_SUBSTITUTE_LITERAL
  if (options(re) & PCRE2_LITERAL) {
    sub_flags |= PCRE2_SUBSTITUTE_LITERAL;
  }
#endif
  PCRE2_SIZE output_size = 0;               // initial pass calculates length of output
  pcre2_substitute(re.get(),                //
                   (PCRE2_SPTR)subject,     //
                   subject_length,          //
                   0,                       //
                   sub_flags,               //
                   NULL,                    //
                   NULL,                    //
                   (PCRE2_SPTR)replacement, //
                   PCRE2_ZERO_TERMINATED,   //
                   NULL,                    //
                   &output_size);

  std::vector<char> sub_out(output_size);
  sub_flags &= ~PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;

  int sub_rc = pcre2_substitute(re.get(),                      //
                                (PCRE2_SPTR)subject,           //
                                subject_length,                //
                                0,                             //
                                sub_flags,                     //
                                NULL,                          //
                                NULL,                          //
                                (PCRE2_SPTR)replacement,       //
                                PCRE2_ZERO_TERMINATED,         //
                                (PCRE2_UCHAR8*)sub_out.data(), //
                                &output_size);

  if (sub_rc < 0) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(sub_rc, buffer, sizeof(buffer));
    char msg[512];
    snprintf(msg, 512, "PCRE2 substitution error: %s", buffer);
    throw std::runtime_error(msg);
  }
  sub_out.resize(sub_out.size() - 1); // pcre2_substitute always places an extra null char at the end
  return sub_out;
}

struct Match {
  const char* begin;
  const char* end;

  void ensure_sane(const char* identification) {
    // regarding code coverage, this does show up depending on the distribution of pcre2
    if (begin > end) {
      char msg[512];
      snprintf(msg, 512,
               "In %s, \\K was used in an assertion to set the match start after its end.\n"
               "From end to start the match was: %.*s",
               identification, (int)(begin - end), end);
      throw std::runtime_error(msg);
    }
  }
};

Match get_match(const char* subject, const match_data& match_data, const char* identification) {
  PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data.get());
  auto m = Match{subject + ovector[0], subject + ovector[1]};
  m.ensure_sane(identification);
  return m;
}

// T is a handler lambda bool(Match&&), which is called with the match and each match group
// the handler should return true iff no other groups should be processed
// rc is the return value from regex::match
// returns true iff no other matches or groups should be processed
template <typename T>
bool get_match_and_groups(const char* subject, int rc, const match_data& match_data, T handler, const char* identification) {
  PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data.get());
  for (int i = 0; i < rc; ++i) {
    auto m = Match{subject + ovector[2 * i], subject + ovector[2 * i + 1]};
    m.ensure_sane(identification);
    if (handler(std::move(m))) {
      return true;
    }
  }
  return false;
}

} // namespace regex
} // namespace choose
