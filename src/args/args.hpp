#pragma once

#include <vector>

#include "utils/regex.hpp"
#include "pipeline/unit/unit.hpp"

namespace choose {

#define choose_xstr(a) choose_str(a)
#define choose_str(a) #a

#define BUF_SIZE_DEFAULT 32768

struct Arguments {
  pipeline::NextUnit nu;
  // indicates that the tokens are displayed in the tui
  bool tui = false;

  bool selection_order = false;
  bool tenacious = false;
  bool use_input_delimiter = false;
  bool end = false;

  bool flush = false;
  bool multiple_selections = false;
  // match is false indicates that Arguments::primary is the delimiter after tokens.
  // else, it matches the tokens themselves
  bool match = false;
  // a modifier on match that also sends everything not included in the match
  bool sed = false;
  bool delimit_not_at_end = false;
  bool delimit_on_empty = false;

  // number of bytes
  // args will set it to a default value if it is unset. max indicates unset
  uint32_t max_lookbehind = std::numeric_limits<decltype(max_lookbehind)>::max();

  // number of bytes. can't be 0
  // args will set it to a default value if it is unset. max indicates unset
  size_t bytes_to_read = std::numeric_limits<decltype(bytes_to_read)>::max();

  size_t buf_size = BUF_SIZE_DEFAULT;
  // args will set it to a default value if it is unset. max indicates unset
  size_t buf_size_frag = std::numeric_limits<decltype(buf_size_frag)>::max();
  const char* locale = "";

  std::vector<char> out_delimiter = {'\n'};
  std::vector<char> bout_delimiter;

  const char* prompt = 0; // points inside one of the argv elements
  // primary is either the input delimiter if match = false, or the match target otherwise
  regex::code primary = 0;

  // shortcut for if the delimiter is a single byte; doesn't set/use primary.
  // doesn't have to go through pcre2 when finding the token separation
  std::optional<char> in_byte_delimiter;

  // testing purposes. if null, uses stdin and stdout.
  // if not null, files must be closed by the callee
  FILE* input = 0;
  FILE* output = 0;

  // disable or allow warning
  bool can_drop_warn = true;
};

}