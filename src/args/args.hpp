#pragma once

#include <variant>
#include <vector>
#include "pipeline/unit.hpp"
#include "utils/regex.hpp"

namespace choose {

#define BUF_SIZE_DEFAULT 32768

struct Arguments {
  TokenOutputStreamArgs tos_args;
  pipeline::PipelineUnit pipeline;
  // indicates that the tokens are displayed in the tui
  bool tui = false;

  bool selection_order = false;
  bool tenacious = false;
  bool use_input_delimiter = false;
  bool end = false;

  bool multiple_selections = false;
  // match is false indicates that Arguments::primary is the delimiter after tokens.
  // else, it matches the tokens themselves
  bool match = false;
  
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

  const char* prompt = 0; // points inside one of the argv elements
  // primary is either the input delimiter if match = false, or the match target otherwise
  regex::code primary = 0;

  // shortcut for if the delimiter is a single byte; doesn't set/use primary.
  // doesn't have to go through pcre2 when finding the token separation
  std::optional<char> in_byte_delimiter;

  // testing purposes. if null, uses stdin and stdout.
  // if not null, files must be closed by the callee
  FILE* input = 0;

  // disable or allow warning
  bool can_drop_warn = true;

  static void populate_args(Arguments& out, int argc, char* const* argv, FILE* input = NULL, FILE* output = NULL);

  // reads from this->input
  // if this->tui:
  //      throws a pipeline_complete exception containing the tokens (to be displayed)
  // else:
  //      writes to this->output, then throws a output_finished exception,
  //      which the caller should handle (exit unless unit test)
  void create_packets();

  void drop_warning();
};

struct UncompiledCodes {
  // all args must be parsed before the args are compiled
  // the uncompiled args are stored here before transfer to the Arguments output.
  uint32_t re_options = PCRE2_LITERAL;
  std::vector<std::unique_ptr<pipeline::UncompiledPipelineUnit>> units;

  std::vector<char> primary;

  // disambiguate between empty and unset
  // needed since they take default values
  bool bout_delimiter_set = false;
  bool primary_set = false;

  void compile(Arguments& output);
};

} // namespace choose