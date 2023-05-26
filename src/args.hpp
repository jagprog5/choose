#pragma once
#include <getopt.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <limits>
#include <variant>
#include <vector>

// for version
#include <ncursesw/curses.h>

#include "regex.hpp"
#include "string_utils.hpp"

namespace choose {

struct RmOrFilterOp {
  enum Type { REMOVE, FILTER };
  Type type;
  regex::code arg;
  regex::match_data match_data;
};

struct SubOp {
  regex::code target;
  const char* replacement;
};

struct IndexOp {
  enum Align { BEFORE, AFTER };
  enum Type { INPUT, OUTPUT };
  IndexOp(Type type, Align align) : index(type == INPUT ? 0 : std::numeric_limits<size_t>::max()), align(align) {}
  // index is only used if this is an input op.
  // if it is set to max, this indicates that it is an output op
  size_t index;
  Align align;

  bool is_output_index_op() const { return index == std::numeric_limits<size_t>::max(); }
};

using OrderedOp = std::variant<RmOrFilterOp, SubOp, IndexOp>;

#define choose_xstr(a) choose_str(a)
#define choose_str(a) #a

#define RETAIN_LIMIT_DEFAULT 32768

struct Arguments {
  std::vector<OrderedOp> ordered_ops;
  bool selection_order = false;
  bool tenacious = false;
  bool use_input_delimiter = false;
  bool end = false;
  bool sort = false; // indicates that any sort is applied
  // if sort_reverse is set, then sort should also be set at the same time
  bool sort_reverse = false;

  // if sort_comp is set, then sort should also be set at the same time
  bool comp_sort = false;

  // user defined comparison
  std::vector<char> comp_sep;
  regex::code comp = 0;
  regex::match_data comp_data = 0;
  bool comp_unique = false;

  bool unique = false;
  bool flip = false;
  bool multiple_selections = false;
  // match is false indicates that Arguments::primary is the separator between tokens.
  // else, it matches the tokens themselves
  bool match = false;
  bool no_delimit = false;
  bool delimit_on_empty = false;
  // max is entirely valid, and the default
  typename std::vector<int>::size_type in = std::numeric_limits<decltype(in)>::max();

  // max indicates unset
  typename std::vector<int>::size_type out = std::numeric_limits<decltype(out)>::max();
  // skip the interface
  bool out_set() const { //
    return out != std::numeric_limits<decltype(out)>::max();
  }

  // number of bytes
  // args will set it to a default value if it is unset. max indicates unset
  uint32_t max_lookbehind = std::numeric_limits<uint32_t>::max();

  // number of bytes. can't be 0
  // args will set it to a default value if it is unset. max indicates unset
  size_t bytes_to_read = std::numeric_limits<size_t>::max();

  size_t retain_limit = RETAIN_LIMIT_DEFAULT;
  const char* locale = "";

  std::vector<char> out_separator = {'\n'};
  std::vector<char> bout_separator;

  const char* prompt = 0; // points inside one of the argv elements
  // primary is either the input separator if match = false, or the match target otherwise
  regex::code primary = 0;

  // testing purposes. if null, uses stdin and stdout.
  // if not null, files must be closed by the callee
  FILE* input = 0;
  FILE* output = 0;

  // a subset of out_set() which doesn't require storing any of the tokens.
  // instead just send straight to the output
  bool is_direct_output() const { //
    return out_set() && !sort && !unique && !flip;
  }

  // a subset of is_direct_output() which allows for simplified logic and avoids
  // a copy of the input
  bool is_basic() const {
    bool has_modifying_op = false;
    for (const auto& op : ordered_ops) {
      if (!std::get_if<RmOrFilterOp>(&op)) {
        has_modifying_op = true;
        break;
      }
    }
    return is_direct_output() && !has_modifying_op;
  }
};

namespace {

struct UncompiledOrderedOp {
  enum Type { SUBSTITUTE, FILTER, REMOVE, INPUT_INDEX, OUTPUT_INDEX };
  Type type;
  // arg0 or arg1 might be set to null (and not used) based on the type
  const char* arg0;
  const char* arg1;

  OrderedOp compile(uint32_t options) const {
    if (type == INPUT_INDEX || type == OUTPUT_INDEX) {
      return IndexOp(type == INPUT_INDEX ? IndexOp::INPUT : IndexOp::OUTPUT, //
                     arg0 ? IndexOp::BEFORE : IndexOp::AFTER);
    }

    auto id = [this]() {
      if (this->type == SUBSTITUTE) {
        return "substitute";
      }
      if (this->type == REMOVE) {
        return "remove";
      }
      if (this->type == FILTER) {
        return "filter";
      }
      {
        return "?"; // will never happen in the context this is used
      }
    };

    // at this point the op can only be sub rm or filter
    regex::code code = regex::compile(arg0, options, id());
    if (type == SUBSTITUTE) {
      return SubOp{std::move(code), arg1};
    }
    // RM or FILTER
    regex::match_data data = regex::create_match_data(code);
    return RmOrFilterOp{type == REMOVE ? RmOrFilterOp::REMOVE : RmOrFilterOp::FILTER, std::move(code), std::move(data)};
  }
};

struct UncompiledCodes {
  // all args must be parsed before the args are compiled
  // the uncompiled args are stored here before transfer to the Arguments output.
  uint32_t re_options = PCRE2_LITERAL;
  std::vector<UncompiledOrderedOp> ordered_ops;
  const char* primary = 0;

  const char* comp = 0;

  void compile(Arguments& output) const {
    for (const UncompiledOrderedOp& op : ordered_ops) {
      OrderedOp oo = op.compile(re_options);
      output.ordered_ops.push_back(std::move(oo));
    }
    output.primary = regex::compile(primary, re_options, "positional argument", PCRE2_JIT_PARTIAL_HARD);

    if (comp) {
      output.comp = regex::compile(comp, re_options, "defined comp");
      output.comp_data = regex::create_match_data(output.comp);
    }
  }
};

void arg_error_preamble(int argc, const char* const* argv, FILE* err = stderr) {
  const char* me; // NOLINT initialized below
  if (argc > 0 && argv && *argv) {
    me = *argv;
  } else {
    me = "choose";
  }
  fputs(me, err);
  fputs(": ", err);
}

// parse a non-negative long, placed in out, from null terminating string, str.
// on parse or range error, arg_has_errors is set to true, and an error is
// printed to err, which includes the name and args; out will have been written
// to but should not be used
void parse_ul(const char* str, //
              long* out,
              unsigned long min_inclusive,
              unsigned long max_inclusive,
              bool* arg_has_errors,
              const char* name,
              int argc,
              const char* const* argv,
              FILE* err = stderr) {
  // based off https://stackoverflow.com/a/14176593/15534181
  char* temp; // NOLINT initialized by strtol
  errno = 0;
  // atoi/atol gives UB for value out of range
  // strtoul is trash and doesn't handle negative values in a sane way
  *out = std::strtol(str, &temp, 0);
  if (temp == str || *temp != '\0' || ((*out == LONG_MIN || *out == LONG_MAX) && errno == ERANGE)) {
    if (err) {
      arg_error_preamble(argc, argv, err);
      fprintf(stderr, "--%s parse error\n", name);
    }
    *arg_has_errors = true;
  } else if (*out < 0 || (unsigned long)*out < min_inclusive || (unsigned long)*out > max_inclusive) {
    if (err) {
      arg_error_preamble(argc, argv, err);
      fprintf(stderr, "--%s value out of range\n", name);
    }
    *arg_has_errors = true;
  }
}

// this function exits
void print_version_message() {
  int exit_code = puts("choose 0.1.0, "
    "ncurses " choose_xstr(NCURSES_VERSION_MAJOR) "." choose_xstr(NCURSES_VERSION_MINOR) "." choose_xstr(NCURSES_VERSION_PATCH) ", "
    "pcre2 " choose_xstr(PCRE2_MAJOR) "." choose_xstr(PCRE2_MINOR)) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
  exit(exit_code);
}

void sigint_handler(int) {
  // signal sent to child process. wait for pclose to finish
}

// this function exits
void print_help_message() {
  // respects 80 char width, and pipes the text to less to accommodate terminal height
  const char* const help_text =
      "                             .     /======\\                                .    \n"
      "   .. .......................;;.   |      |  .. ...........................;;.  \n"
      "    ..::::::::::stdin::::::::;;;;. |choose|   ..::::::::chosen stdout::::::;;;;.\n"
      "  . . :::::::::::::::::::::::;;:'  | >    | . . :::::::::::::::::::::::::::;;:' \n"
      "                             :'    \\======/                                :'   \n"
      "description:\n"
      "        Splits the input into tokens, and provides a tui for selecting which\n"
      "        tokens are sent to the output.\n"
      "positional argument:\n"
      "        <input separator, default: '\\n'>\n"
      "                describes how to split the input into tokens. each token is\n"
      "                displayed for selection in the interface.\n"
      "messages:\n"
      "        -h, --help\n"
      "        -v, --version\n"
      "ordered operations can be specified multiple times and are applied in the order\n"
      "they are stated. they are applied before any sorting or uniqueness options.\n"
      "        -f, --filter <target>\n"
      "                remove tokens that don't match. it inherits the same match\n"
      "                options as the input separator\n"
      "        --in-index [before|after]\n"
      "                on each token, insert the input index\n"
      "        --out-index [before|after]\n"
      "                on each token, insert the output index\n"
      "        --rm, --remove <target>\n"
      "                inverse of --filter\n"
      "        --sub, --substitute <target> <replacement>\n"
      "                apply a global text substitution on each token. the target\n"
      "                inherits the same match options as the input separator. "
#ifdef PCRE2_SUBSTITUTE_LITERAL
      "the\n"
#else
      "if\n"
#endif
#ifdef PCRE2_SUBSTITUTE_LITERAL
      "                replacement is done literally if the input separator is literal\n"
      "                (aka the default without -r). otherwise, the replacement is a\n"
      "                regular expression.\n"
#else
      "                compiled with a later verion of PCRE2, then the replacement would\n"
      "                be been done literally if the input separator is literal (aka the\n"
      "                default without -r). however, this version does not support this,\n"
      "                so the replacement is always a regex.\n"
#endif
      "options:\n"
      "        -b, --batch-separator <separator, default: <output separator>>\n"
      "                selecting multiple tokens and sending them to the output\n"
      "                together is a \"batch\". if multiple batches are send to the\n"
      "                output (which is enabled via --tenacious), then a batch\n"
      "                separator is used between batches. it is also placed at the end\n"
      "                of the output if any output was given and without --no-delimit\n"
      "        --comp <sep> <comp>\n"
      "                user defined comparison. less-than comparison is indicated by\n"
      "                concatenating two tokens with sep and successfully matching\n"
      "                against comp. required by --comp-sort and --comp-unique\n"
      "        --comp-sort\n"
      "                requires --comp. does a stable sort using the comparison.\n"
      "                ignores --sort. can use --sort-reverse\n"
      "        --comp-unique\n"
      "                requires --comp. allow only first instances of unique\n"
      "                elements as defined by the comparison. ignores --unique\n"
      "        --comp-z <comp>\n"
      "                --comp with a null char separator\n"
      "        -d, --no-delimit\n"
      "                don't add a batch separator at the end of the output. ignores\n"
      "                --delimit-on-empty\n"
      "        --delimit-on-empty\n"
      "                even if the output would be empty, place a batch separator\n"
      "        -e, --end\n"
      "                begin cursor and prompt at the bottom\n"
      "        --flip\n"
      "                reverse the token order just before displaying. this happens\n"
      "                after all other operations\n"
      "        -i, --ignore-case\n"
      "                make the input separator case-insensitive\n"
      "        --in <# tokens>\n"
      "                stop reading the input once n tokens have been finalized\n"
      "        --locale <locale>\n"
      "        -m, --multi\n"
      "                allow the selection of multiple tokens\n"
      "        --multiline\n"
      "                implies --regex. enable multiline matching. (affects ^ and $)\n"
      "        --match\n"
      "                the positional argument matches the tokens instead of the\n"
      "                separator. the match and each match group is a token\n"
      "        --max-lookbehind <# characters>\n"
      "                the max number of characters that the pattern can look before\n"
      "                its beginning. if not specified, it is auto detected from the\n"
      "                pattern but may not be accurate for nested lookbehinds\n"
      "        -o, --output-separator <separator, default: '\\n'>\n"
      "                if multiple tokens are selected (which is enabled via -m), then\n"
      "                a separator is placed between each token in the output\n"
      "        --out [<# tokens>]\n"
      "                skip the interface. select the first n tokens if arg is\n"
      "                specified, or all tokens if unspecified\n"
      "        -p, --prompt <prompt>\n"
      "        -r, --regex\n"
      "                use PCRE2 regex for the input separator.\n"
      "        --read <# bytes>\n"
      "                the number of bytes read from stdin per iteration\n"
      "        --retain-limit <# bytes, default: " choose_xstr(RETAIN_LIMIT_DEFAULT) ">\n"
      "                this ensures that the memory usage is bounded in the event of\n"
      "                parasitic matching. if a match is greedy (e.g. \".*\"), then the\n"
      "                match buffer increases to hold the entire input as it tries to\n"
      "                complete the match. reaching this limit results in a match\n"
      "                failure. additionally, if the separator never matches (e.g. $'')\n"
      "                then the limit of the size of the token being built is also\n"
      "                subject to this limit. reaching this limit results in the\n"
      "                token's content thus far being discarded.\n"
      "        -s, --sort\n"
      "                sort each token lexicographically\n"
      "        --selection-order\n"
      "                sort the token output based on selection order instead of the\n"
      "                input order\n"
      "        --sort-reverse\n"
      "                sort each token reverse lexicographically\n"
      "        -t, --take [<# tokens>]\n"
      "                both --in and --out\n"
      "        --tenacious\n"
      "                don't exit on confirmed selection\n"
      "        -u, --unique\n"
      "                remove duplicate input tokens. leaves first occurrences\n"
      "        --use-delimiter\n"
      "                don't ignore a separator at the end of the input\n"
      "        --utf\n"
      "                enable UTF-8\n"
      "        --utf-allow-invalid\n"
      "                implies --utf, skips invalid codes\n"
      "        -y, --batch-print0\n"
      "                use null as the batch separator\n"
      "        -z, --print0\n"
      "                use null as the output separator\n"
      "        -0, --null, --read0\n"
      "                use null as the input separator this is the same as -r and \\x00\n"
      "        --\n"
      "                stop option parsing\n"
      "examples:\n"
      "        echo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
      "        echo -n \"1A2a3\" | choose -i \"a\"\n"
      "        echo -n \"a b c\" | choose -o, -b$'\\n' \" \" -m --tenacious\\\n"
      "                --selection-order -p \"space, enter, escape\"\n"
      "        echo -n 'hello world' | choose -r --sub 'hello (\\w+)' 'hi $1'\n"
      "        echo -n 'every other word is printed here' | choose ' ' -r --out\\\n"
      "                --in-index=after -f '[02468]$' --sub '(.*) [0-9]+' '$1'\n"
      "        echo -en \"John Doe\\nApple\\nJohn Doe\\nBanana\\nJohn Smith\" | choose\\\n"
      "                -r --comp-z $'^John[ a-zA-Z]*\\0(?!John)' --comp-sort\n"
      "        # some options are only available via prefix\n"
      "        echo -n \"1a2A3\" | choose -r '(*NO_JIT)(*LIMIT_HEAP=1000)(?i)a'\n"
      "controls:\n"
      "        confirm selections: enter, d, or f\n"
      "                      exit: q, backspace, escape\n"
      "        multiple selection: space   <-- enabled with --multi\n"
      "          clear selections: c\n"
      "                 scrolling: arrow/page up/down, home/end, "
#ifdef BUTTON5_PRESSED
      "mouse scroll, "
#endif
      "j/k\n\n"
      "to view the license, or report an issue, visit:\n"
      "        github.com/jagprog5/choose\n";

  FILE* fp = popen("less -KQ", "w");
  if (fp == NULL) {
    perror(NULL);
    puts(help_text); // less didn't work, try the normal way anyways
    exit(EXIT_FAILURE);
  }

  if (signal(SIGINT, sigint_handler) == SIG_IGN) {
    signal(SIGINT, SIG_IGN);
  }

  if (fputs(help_text, fp) < 0) {
    perror(NULL);
    pclose(fp);
    puts(help_text);
    exit(EXIT_FAILURE);
  }

  int close_result = pclose(fp);
  // 2 is less exit code on ctrl-c with -K
  if (close_result == 2) {
    exit(128 + 2);
  } else if (close_result != EXIT_SUCCESS) {
    if (close_result < 0) {
      perror(NULL);
    } else {
      fprintf(stderr, "error running less");
    }
    puts(help_text);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

} // namespace

// https://stackoverflow.com/a/69177115
#define OPTIONAL_ARGUMENT_IS_PRESENT ((optarg == NULL && optind < argc && argv[optind][0] != '-') ? (bool)(optarg = argv[optind++]) : (optarg != NULL))

// this function may call exit. input and output is for testing purposes; if
// unspecified, uses stdin and stdout, otherwise must be managed be the callee
// (e.g. fclose)
Arguments handle_args(int argc, char* const* argv, FILE* input = NULL, FILE* output = NULL) {
  UncompiledCodes uncompiled_output;
  Arguments ret;
  bool arg_has_errors = false;
  bool bout_separator_set = false;
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {

        // messages
        {"version", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        // args
        {"output-separator", required_argument, NULL, 'o'},
        {"batch-separator", required_argument, NULL, 'b'},
        {"prompt", required_argument, NULL, 'p'},
        {"comp", required_argument, NULL, 0},
        {"comp-z", required_argument, NULL, 0},
        {"sub", required_argument, NULL, 0},
        {"substitute", required_argument, NULL, 0},
        {"filter", required_argument, NULL, 'f'},
        {"remove", required_argument, NULL, 0},
        {"retain-limit", required_argument, NULL, 0},
        {"rm", required_argument, NULL, 0},
        {"max-lookbehind", required_argument, NULL, 0},
        {"read", required_argument, NULL, 0},
        {"in", required_argument, NULL, 0},
        {"in-index", optional_argument, NULL, 0},
        {"locale", required_argument, NULL, 0},
        {"out", optional_argument, NULL, 0},
        {"out-index", optional_argument, NULL, 0},
        {"take", optional_argument, NULL, 't'},
        // options
        {"comp-sort", no_argument, NULL, 0},
        {"comp-unique", no_argument, NULL, 0},
        {"no-delimit", no_argument, NULL, 'd'},
        {"delimit-on-empty", no_argument, NULL, 0},
        {"end", no_argument, NULL, 'e'},
        {"flip", no_argument, NULL, 0},
        {"ignore-case", no_argument, NULL, 'i'},
        {"multi", no_argument, NULL, 'm'},
        {"multiline", no_argument, NULL, 0},
        {"match", no_argument, NULL, 0},
        {"regex", no_argument, NULL, 'r'},
        {"sort", no_argument, NULL, 's'},
        {"selection-order", no_argument, NULL, 0},
        {"sort-reverse", no_argument, NULL, 0},
        {"tenacious", no_argument, NULL, 0},
        {"unique", no_argument, NULL, 'u'},
        {"use-delimiter", no_argument, NULL, 0},
        {"utf", no_argument, NULL, 0},
        {"utf-allow-invalid", no_argument, NULL, 0},
        {"batch-print0", no_argument, NULL, 'y'},
        {"print0", no_argument, NULL, 'z'},
        {"null", no_argument, NULL, '0'},
        {"read0", no_argument, NULL, '0'},
        {NULL, 0, NULL, 0}

    };
    int c = getopt_long(argc, argv, "-vho:b:p:f:t::rdeimrsuyz0", long_options, &option_index);
    if (c == -1) {
      break; // end of args
    }

    switch (c) {
      default:
        arg_has_errors = true;
        break;
      case 0: {
        // long option
        const char* name = long_options[option_index].name;
        if (optarg) {
          // long option with argument
          if (strcmp("rm", name) == 0 || strcmp("remove", name) == 0) {
            UncompiledOrderedOp op{UncompiledOrderedOp::REMOVE, optarg, NULL};
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("retain-limit", name) == 0) {
            long v; // NOLINT
            // minimum is enforced to 2 via bytes_to_read check below
            parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.retain_limit)>::max(), &arg_has_errors, name, argc, argv);
            ret.retain_limit = v;
          } else if (strcmp("in", name) == 0) {
            long v; // NOLINT
            parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.in)>::max(), &arg_has_errors, name, argc, argv);
            ret.in = v;
          } else if (strcmp("max-lookbehind", name) == 0) {
            long v; // NOLINT
            parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.max_lookbehind)>::max() - 1, &arg_has_errors, name, argc, argv);
            ret.max_lookbehind = v;
          } else if (strcmp("read", name) == 0) {
            long v; // NOLINT
            // minimum value enforced below
            parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.bytes_to_read)>::max() - 1, &arg_has_errors, name, argc, argv);
            ret.bytes_to_read = v;
          } else if (strcmp("out", name) == 0) {
            long v; // NOLINT
            parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.out)>::max() - 1, &arg_has_errors, name, argc, argv);
            ret.out = v;
          } else if (strcmp("in-index", name) == 0) {
            UncompiledOrderedOp op; // NOLINT
            op.type = UncompiledOrderedOp::INPUT_INDEX;
            if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
              op.arg0 = (const char*)1;
            } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
              op.arg0 = 0;
            } else {
              arg_error_preamble(argc, argv);
              fprintf(stderr, "alignment must be \"before\" or \"after\"\n");
              arg_has_errors = true;
            }
            op.arg1 = NULL;
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("out-index", name) == 0) {
            UncompiledOrderedOp op; // NOLINT
            op.type = UncompiledOrderedOp::OUTPUT_INDEX;
            if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
              op.arg0 = (const char*)1;
            } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
              op.arg0 = 0;
            } else {
              arg_error_preamble(argc, argv);
              fprintf(stderr, "alignment must be \"before\" or \"after\"\n");
              arg_has_errors = true;
            }
            op.arg1 = NULL;
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("sub", name) == 0 || strcmp("substitute", name) == 0) {
            // special handing here since getopt doesn't normally support multiple arguments
            if (optind >= argc) {
              // ran off end
              arg_error_preamble(argc, argv);
              fprintf(stderr, "option '--%s' requires two arguments\n", name);
              arg_has_errors = true;
            } else {
              ++optind;
              UncompiledOrderedOp op; // NOLINT
              op.type = UncompiledOrderedOp::SUBSTITUTE;
              op.arg0 = argv[optind - 2];
              op.arg1 = argv[optind - 1];
              uncompiled_output.ordered_ops.push_back(op);
            }
          } else if (strcmp("comp", name) == 0) {
            if (optind >= argc) {
              // ran off end
              arg_error_preamble(argc, argv);
              fputs("option '--comp' requires two arguments\n", stderr);
              arg_has_errors = true;
            } else {
              ++optind;
              size_t len = std::strlen(argv[optind - 2]);
              ret.comp_sep.resize(len);
              std::memcpy(ret.comp_sep.data(), argv[optind - 2], len * sizeof(char));
              uncompiled_output.comp = argv[optind - 1];
            }
          } else if (strcmp("comp-z", name) == 0) {
            ret.comp_sep = {'\0'};
            uncompiled_output.comp = optarg;
          } else if (strcmp("locale", name) == 0) {
            ret.locale = optarg;
          } else {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "unknown arg \"%s\"\n", name);
            arg_has_errors = true;
          }
        } else {
          // long option without argument or with optional argument
          if (strcmp("out", name) == 0) {
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              long v; // NOLINT
              parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.out)>::max() - 1, &arg_has_errors, name, argc, argv);
              ret.out = v;
            } else {
              ret.out = std::numeric_limits<decltype(ret.out)>::max() - 1;
            }
          } else if (strcmp("flip", name) == 0) {
            ret.flip = true;
          } else if (strcmp("comp-sort", name) == 0) {
            ret.comp_sort = true;
            ret.sort = true;
          } else if (strcmp("comp-unique", name) == 0) {
            ret.comp_unique = true;
          } else if (strcmp("delimit-on-empty", name) == 0) {
            ret.delimit_on_empty = true;
          } else if (strcmp("match", name) == 0) {
            ret.match = true;
          } else if (strcmp("multiline", name) == 0) {
            uncompiled_output.re_options &= ~PCRE2_LITERAL;
            uncompiled_output.re_options |= PCRE2_MULTILINE;
          } else if (strcmp("sort-reverse", name) == 0) {
            ret.sort = true;
            ret.sort_reverse = true;
          } else if (strcmp("selection-order", name) == 0) {
            ret.selection_order = true;
          } else if (strcmp("tenacious", name) == 0) {
            ret.tenacious = true;
          } else if (strcmp("in-index", name) == 0) {
            UncompiledOrderedOp op; // NOLINT
            op.type = UncompiledOrderedOp::INPUT_INDEX;
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
                op.arg0 = (const char*)1;
              } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
                op.arg0 = 0;
              } else {
                arg_error_preamble(argc, argv);
                fprintf(stderr, "alignment must be \"before\" or \"after\"\n");
                arg_has_errors = true;
              }
            } else {
              op.arg0 = (const char*)1; // default = before
            }
            op.arg1 = NULL;
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("out-index", name) == 0) {
            UncompiledOrderedOp op; // NOLINT
            op.type = UncompiledOrderedOp::OUTPUT_INDEX;
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
                op.arg0 = (const char*)1;
              } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
                op.arg0 = 0;
              } else {
                arg_error_preamble(argc, argv);
                fprintf(stderr, "alignment must be \"before\" or \"after\"\n");
                arg_has_errors = true;
              }
            } else {
              op.arg0 = (const char*)1; // default = before
            }
            op.arg1 = NULL;
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("use-delimiter", name) == 0) {
            ret.use_input_delimiter = true;
          } else if (strcmp("utf", name) == 0) {
            uncompiled_output.re_options |= PCRE2_UTF;
          } else if (strcmp("utf-allow-invalid", name) == 0) {
            uncompiled_output.re_options |= PCRE2_MATCH_INVALID_UTF;
          } else if (strcmp("null", name) == 0 || strcmp("read0", name) == 0) {
            uncompiled_output.re_options &= ~PCRE2_LITERAL;
            uncompiled_output.primary = "\\x00";
          } else {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "unknown arg \"%s\"\n", name);
            arg_has_errors = true; // will never happen
          }
        }
      } break;
      case 1:
        // positional argument
        if (uncompiled_output.primary) {
          arg_error_preamble(argc, argv);
          fprintf(stderr,
                  "the positional arg must be specified once. "
                  "the second instance was found at position %d: \"%s\"\n",
                  optind - 1, optarg);
          arg_has_errors = true;
        }
        uncompiled_output.primary = optarg;
        break;
      case 'v':
        print_version_message();
        break;
      case 'h':
        print_help_message();
        break;
      case 'd':
        ret.no_delimit = true;
        break;
      case 'e':
        ret.end = true;
        break;
      case 'i':
        uncompiled_output.re_options |= PCRE2_CASELESS;
        break;
      case 'm':
        ret.multiple_selections = true;
        break;
      case 'r':
        uncompiled_output.re_options &= ~PCRE2_LITERAL;
        break;
      case 's':
        ret.sort = true;
        break;
      case 't':
        if (OPTIONAL_ARGUMENT_IS_PRESENT) {
          if (*optarg == '=') {
            ++optarg;
          }
          long v; // NOLINT
          parse_ul(optarg, &v, 0, std::numeric_limits<decltype(ret.in)>::max(), &arg_has_errors, "take", argc, argv);
          ret.in = v;
        }
        ret.out = std::numeric_limits<decltype(ret.out)>::max() - 1;
        break;
      case 'u':
        ret.unique = true;
        break;
      case 'y':
        // these options are made available since null can't be typed as a command line arg
        // there's precedent elsewhere, e.g. find -print0 -> xargs -0
        ret.bout_separator = {'\0'};
        bout_separator_set = true;
        break;
      case 'z':
        ret.out_separator = {'\0'};
        break;
      case '0':
        uncompiled_output.primary = "\\x00";
        uncompiled_output.re_options &= ~PCRE2_LITERAL;
        break;
      case 'o': {
        // NOLINTNEXTLINE optarg guaranteed non-null since ':' follows 'o' in opt string
        size_t len = std::strlen(optarg);
        ret.out_separator.resize(len);
        std::memcpy(ret.out_separator.data(), optarg, len * sizeof(char));
      } break;
      case 'b': {
        // NOLINTNEXTLINE optarg guaranteed non-null since ':' follows 'b' in opt string
        size_t len = std::strlen(optarg);
        ret.bout_separator.resize(len);
        std::memcpy(ret.bout_separator.data(), optarg, len * sizeof(char));
        bout_separator_set = true;
      } break;
      case 'p':
        ret.prompt = optarg;
        break;
      case 'f':
        UncompiledOrderedOp op{UncompiledOrderedOp::FILTER, optarg, NULL};
        uncompiled_output.ordered_ops.push_back(op);
        break;
    }
  }

  if (!bout_separator_set) {
    ret.bout_separator = ret.out_separator;
  }

  if (!uncompiled_output.primary) {
    if (ret.match) {
      // this isn't needed for any mechanical reason, only that the caller isn't doing something sane.
      arg_error_preamble(argc, argv);
      fputs("the positional arg must be specified with --match\n", stderr);
      arg_has_errors = true;
    }
    // default sep
    uncompiled_output.primary = "\n";
  }

  // give failure on dangerous args
  if (!ret.match && strcmp(uncompiled_output.primary, "") == 0) {
    arg_error_preamble(argc, argv);
    fputs("A non-matchable separator will discard the token thus far when the retain limit is hit.\n", stderr);
    arg_has_errors = true;
  }

  if ((uncompiled_output.re_options & PCRE2_LITERAL) == 0 && strcmp(uncompiled_output.primary, ".*") == 0) {
    arg_error_preamble(argc, argv);
    fputs("A parasitic match will result in a match failure when the retain limit is hit.\n", stderr);
    arg_has_errors = true;
  }

  if (arg_has_errors) {
    exit(EXIT_FAILURE);
  }

  if (input) {
    ret.input = input;
  } else {
    ret.input = stdin;
  }

  if (output) {
    ret.output = output;
  } else {
    ret.output = stdout;
  }

  // compile the arguments now that the entire context has been obtained
  uncompiled_output.compile(ret);

  // defaults
  if (ret.max_lookbehind == std::numeric_limits<decltype(ret.max_lookbehind)>::max()) {
    ret.max_lookbehind = regex::max_lookbehind_size(ret.primary);
  }
  if (regex::options(ret.primary) & PCRE2_UTF) {
    ret.max_lookbehind *= str::utf8::MAX_BYTES_PER_CHARACTER;
  }

  if (ret.bytes_to_read == std::numeric_limits<decltype(ret.bytes_to_read)>::max()) {
    ret.bytes_to_read = 8192; // some value based on cursory profiling
    if (ret.bytes_to_read > ret.retain_limit / 2) {
      ret.bytes_to_read = ret.retain_limit / 2;
    }
  }

  if (ret.bytes_to_read == 0) {
    arg_error_preamble(argc, argv);
    fputs("the bytes to read cannot be set to zero\n", stderr);
    arg_error_preamble(argc, argv);
    fputs("this can be caused by a small --retain-limit or small --read\n", stderr);
    exit(EXIT_FAILURE);
  }

  if (isatty(fileno(ret.input))) {
    int exit_code = puts("Try 'choose --help' for more information.") < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    exit(exit_code);
  }

  return ret;
}

} // namespace choose
