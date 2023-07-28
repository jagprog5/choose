#pragma once

#include "utils/string_utils.hpp"

namespace choose {

// a limited scope of choose::Arguments, that need to be accessed by TokenOutputStream
struct TokenOutputStreamArgs {
  FILE* output = 0; // see Arguments::input
  bool sed = false; // a modifier on match that also sends everything not included in the match
  bool flush = false;
  bool delimit_not_at_end = false;
  bool delimit_on_empty = false;
  std::vector<char> out_delimiter = {'\n'};
  std::vector<char> bout_delimiter;

  // deleting copies and moves since TokenOutputStream keeps a pointer that should stay in place, below
  TokenOutputStreamArgs() = default;
  TokenOutputStreamArgs(const TokenOutputStreamArgs& o) = delete;
  TokenOutputStreamArgs& operator=(const TokenOutputStreamArgs&) = delete;
  TokenOutputStreamArgs(TokenOutputStreamArgs&& o) = delete;
  TokenOutputStreamArgs& operator=(TokenOutputStreamArgs&&) = delete;
};

namespace pipeline {


// writes an output delimiter between each token
// and (might, depending on args) a batch output delimiter on finish.
struct TokenOutputStream {
  bool has_written = false;

  // is a delimiter required before the next write
  bool delimit_required_ = false;

  const TokenOutputStreamArgs* args;

  static void default_write(FILE* out, const char* begin, const char* end) { //
    str::write_f(out, begin, end);
  }

  TokenOutputStream(const TokenOutputStreamArgs* args) : args(args) {}

  // write a part of a token to the output.
  // the last part of a token must instead use write_output
  void write_output_fragment(const char* begin, const char* end) {
    if (delimit_required_ && !args->sed) {
      str::write_f(args->output, args->out_delimiter);
    }
    delimit_required_ = false;
    has_written = true;
    str::write_f(args->output, begin, end);
  }

  // write a part or whole of a token to the output.
  // if it is a part, then it must be the last part.
  // pass a handler void(FILE* out, const char* begin, const char* end).
  // the function will write the token to the output after applying transformations
  template <typename T = decltype(TokenOutputStream::default_write)>
  void write_output(const char* begin, //
                    const char* end,
                    T handler = TokenOutputStream::default_write) {
    if (delimit_required_ && !args->sed) {
      str::write_f(args->output, args->out_delimiter);
    }
    delimit_required_ = true;
    has_written = true;
    handler(args->output, begin, end);
    if (args->flush) {
      choose::str::flush_f(args->output);
    }
  }

  // call after all other writing has finished
  void finish_output() {
    if (!args->delimit_not_at_end && (has_written || args->delimit_on_empty) && !args->sed) {
      str::write_f(args->output, args->bout_delimiter);
    }
    delimit_required_ = false; // optional reset of state
    has_written = false;
  }
};

} // namespace pipeline
} // namespace choose
