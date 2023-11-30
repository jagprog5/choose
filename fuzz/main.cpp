#include "../src/termination_request.hpp"

#define CHOOSE_FUZZING_APPLIED
#include "../src/args.hpp"
#include "../src/token.hpp"

extern int optreset;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // generate arguments and stdin from the fuzz data
  std::vector<std::string> arg_strs;
  arg_strs.emplace_back();

  while (size != 0) {
    char ch = *data;
    if (ch == '\0') {
      // n arg_strs are used. the rest is piped to stdin
      if (arg_strs.size() >= 10) {
        data += 1;
        size -= 1;
        break;
      }
      arg_strs.emplace_back();
    } else {
      arg_strs.rbegin()->push_back(ch);
    }
    data += 1;
    size -= 1;
  }

  std::vector<char*> argv;
  for (std::string& elem : arg_strs) {
    argv.push_back(&elem[0]);
  }

  // resetting getopt global state
  // https://github.com/dnsdb/dnsdbq/commit/efa68c0499c3b5b4a1238318345e5e466a7fd99f
#ifdef linux
  optind = 0;
#else
  optind = 1;
  optreset = 1;
#endif

  // any remaining bytes are used as stdin
  int input_pipe[2];
  (void)!pipe(input_pipe);

  FILE* input_writer = fdopen(input_pipe[1], "w");
  FILE* input_reader = fdopen(input_pipe[0], "r");

  choose::str::write_f(input_writer, (const char*)data, (const char*)data + size);
  fclose(input_writer);

  FILE* output_writer = fopen("/dev/null", "w");
  try {
    auto args = choose::handle_args((int)argv.size(), argv.data(), input_reader, output_writer);
    choose::create_tokens(args);
  } catch (const choose::termination_request&) {
  } catch (const choose::regex::regex_failure&) {
  }

  fclose(output_writer);
  fclose(input_reader);
  return 0;
}
