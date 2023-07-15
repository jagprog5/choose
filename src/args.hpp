#pragma once
#include <getopt.h>
#include <unistd.h>
#include <cassert>
#include <csignal>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>
// for version
#include <ncursesw/curses.h>

#include "ordered_op.hpp"
#include "numeric_utils.hpp"

namespace choose {

#define choose_xstr(a) choose_str(a)
#define choose_str(a) #a

#define BUF_SIZE_DEFAULT 32768

struct Arguments {
  std::vector<OrderedOp> ordered_ops;
  // indicates that the tokens are displayed in the tui
  bool tui = false;

  bool selection_order = false;
  bool tenacious = false;
  bool use_input_delimiter = false;
  bool end = false;
  bool sort = false; // indicates that any sort is applied
  // if sort_reverse is set, then sort should also be set at the same time
  bool sort_reverse = false;

  // user defined comparison
  regex::code comp = 0;
  bool comp_unique = false;
  // if comp_sort is set, then sort should also be set at the same time
  bool comp_sort = false;

  bool unique = false;
  bool flip = false;
  bool flush = false;
  bool multiple_selections = false;
  // match is false indicates that Arguments::primary is the delimiter after tokens.
  // else, it matches the tokens themselves
  bool match = false;
  // a modifier on match that also sends everything not included in the match
  bool sed = false;
  bool delimit_not_at_end = false;
  bool delimit_on_empty = false;

  std::optional<typename std::vector<int>::size_type> in;
  std::optional<typename std::vector<int>::size_type> out;

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

  // a special case where the tokens can be sent directly to the output as they are received
  bool is_direct_output() const { return !tui && !sort && !flip; }

  // a subset of is_direct_output where the tokens don't need to be stored at all
  bool tokens_not_stored() const { //
    return is_direct_output() && !unique && !comp_unique;
  }
};

namespace {

struct UncompiledOrderedOp {
  enum Type { SUBSTITUTE, FILTER, REPLACE, REMOVE, INPUT_INDEX, OUTPUT_INDEX };
  Type type;
  // arg0 or arg1 might be set to null (and not used) based on the type
  const char* arg0;
  const char* arg1;

  OrderedOp compile(uint32_t options) const {
    if (type == REPLACE) {
      return ReplaceOp(arg0);
    }
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
      return "?"; // never
    };

    // at this point the op can only be sub rm or filter
    regex::code code = regex::compile(arg0, options, id());
    if (type == SUBSTITUTE) {
      return SubOp{std::move(code), arg1};
    }
    return RmOrFilterOp(type == REMOVE ? RmOrFilterOp::REMOVE : RmOrFilterOp::FILTER, std::move(code));
  }
};

struct UncompiledCodes {
  // all args must be parsed before the args are compiled
  // the uncompiled args are stored here before transfer to the Arguments output.
  uint32_t re_options = PCRE2_LITERAL;
  std::vector<UncompiledOrderedOp> ordered_ops;

  std::vector<char> primary;

  const char* comp = 0;

  // disambiguate between empty and unset
  // needed since they take default values
  bool bout_delimiter_set = false;
  bool primary_set = false;

  void compile(Arguments& output) const {
    for (const UncompiledOrderedOp& op : ordered_ops) {
      OrderedOp oo = op.compile(re_options);
      output.ordered_ops.push_back(std::move(oo));
    }

    // see if single byte delimiter optimization applies
    if (!output.match && primary.size() == 1) {
      if (re_options & PCRE2_LITERAL) {
        // if the expression is literal then any single byte works
        output.in_byte_delimiter = primary[0];
      } else {
        // there's definitely better ways of recognizing if a regex pattern
        // consists of a single byte, but this is enough for common cases
        char ch = primary[0];
        if ((ch == '\n' || ch == '\0' || num::in(ch, '0', '9') || num::in(ch, 'a', 'z') || num::in(ch, 'A', 'Z'))) {
          output.in_byte_delimiter = ch;
        }
      }
    }

    if (!output.in_byte_delimiter) {
      output.primary = regex::compile(primary, re_options, "positional argument", PCRE2_JIT_PARTIAL_HARD);
    }

    if (comp) {
      output.comp = regex::compile(comp, re_options, "defined comp");
    }
  }
};

// prefixes error message with the path of this executable
void arg_error_preamble(int argc, const char* const* argv) {
  const char* me; // NOLINT initialized below
  if (argc > 0 && argv && *argv) {
    me = *argv;
  } else {
    me = "choose";
  }
  fputs(me, stderr);
  fputs(": ", stderr);
}

// this function exits
void print_version_message() {
  int exit_code = puts("choose 0.2.0, "
    "ncurses " choose_xstr(NCURSES_VERSION_MAJOR) "." choose_xstr(NCURSES_VERSION_MINOR) "." choose_xstr(NCURSES_VERSION_PATCH) ", "
    "pcre2 " choose_xstr(PCRE2_MAJOR) "." choose_xstr(PCRE2_MINOR)) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
  exit(exit_code);
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
      "        Splits the input into tokens and provides stream manipulation and a tui\n"
      "        selector.\n"
      "positional argument:\n"
      "        [<input delimiter, default: '\\n'>]\n"
      "messages:\n"
      "        -h, --help\n"
      "        -v, --version\n"
      "ordered operations can be specified multiple times and are applied in the order\n"
      "they are stated. they are applied before any sorting or uniqueness options.\n"
      "        -f, --filter <target>\n"
      "                remove tokens that don't match. inherits the same match options\n"
      "                as the positional argument\n"
      "        --in-index [b[efore]|a[fter]|<default: b>]\n"
      "                on each token, insert the input index\n"
      "        --out-index [b[efore]|a[fter]|<default: b>]\n"
      "                on each token, insert the output index\n"
      "        --replace <replacement>\n"
      "                a special case of the substitution op where the match target is\n"
      "                the positional argument. --match or --sed must be specified.\n"
      "                this op must come before all ops that edit tokens: all except rm\n"
      "                or filter\n"
      "        --rm, --remove <target>\n"
      "                inverse of --filter\n"
      "        --sub, --substitute <target> <replacement>\n"
      "                apply a global text substitution on each token. the target\n"
      "                inherits the same match options as the positional argument. "
#ifdef PCRE2_SUBSTITUTE_LITERAL
      "the\n"
#else
      "if\n"
#endif
#ifdef PCRE2_SUBSTITUTE_LITERAL
      "                replacement is done literally if the positional argument is\n"
      "                literal (aka the default without -r). otherwise, the replacement\n"
      "                is a regular expression\n"
#else
      "                compiled with a later verion of PCRE2, then the replacement\n"
      "                would be been done literally if the positional argument is\n"
      "                literal (aka the default without -r). however, this version does\n"
      "                not support this, so the replacement is always a regex\n"
#endif
      "options:\n"
      "        -b, --batch-delimiter <delimiter, default: <output-delimiter>>\n"
      "                a batch is a group of tokens. typically the output consists\n"
      "                of a single batch. if --tui and --tenacious are specified\n"
      "                then the output can consist of multiple batches. a batch\n"
      "                delimiter is placed after every batch.\n"
      "        --buf-size <# bytes, default: " choose_xstr(BUF_SIZE_DEFAULT) ">\n"
      "                size of match buffer used. patterns that require more room will\n"
      "                never successfully match\n"
      "        --buf-size-frag <# bytes, default: <buf-size * 8>\n"
      "                this is only applicable if --match is not specified, meaning the\n"
      "                input delimiter is being matched. if the match buffer is full\n"
      "                when attempting to complete a match, the bytes at the beginning\n"
      "                of the buffer that have no possibility of being a part of a\n"
      "                match are moved to free up space in the buffer. they are moved\n"
      "                either directly to the output if the args allow for it, or to a\n"
      "                separate buffer where they are accumulated until the token is\n"
      "                completed. this separate buffer is cleared if its size would\n"
      "                exceed this arg. the bytes are instead sent directly to the\n"
      "                output if no ordered ops are specified, and there is no sorting,\n"
      "                no uniqueness, no flip, and no tui used\n"
      "        --comp <less than comparison>\n"
      "                user defined comparison. pairs of tokens are evaluated. if only\n"
      "                one matches, then it is less than the other. required by\n"
      "                --comp-sort and --comp-unique. inherits the same match options\n"
      "                as the positional argument\n"
      "        --comp-sort\n"
      "                requires --comp. does a stable sort using the comparison.\n"
      "                ignores --sort. can use --sort-reverse\n"
      "        --comp-unique\n"
      "                requires --comp. allow only first instances of unique\n"
      "                elements as defined by the comparison. ignores --unique\n"
      "        -d, --delimit-same\n"
      "                applies both --delimit-not-at-end and --use-delimiter. this\n"
      "                makes the output end with a delimiter when the input also ends\n"
      "                with a delimiter\n"
      "        --delimit-not-at-end\n"
      "                don't add a batch delimiter at the end of the output. ignores\n"
      "                --delimit-on-empty\n"
      "        --delimit-on-empty\n"
      "                even if the output would be empty, place a batch delimiter\n"
      "        -e, --end\n"
      "                begin cursor and prompt at the bottom of the tui\n"
      "        --flip\n"
      "                reverse the token order. this happens after all other operations\n"
      "        --flush\n"
      "                makes the input unbuffered, and the output is flushed after each\n"
      "                token is written\n"
      "        -i, --ignore-case\n"
      "                make the positional argument case-insensitive\n"
      "        --in <# tokens>\n"
      "                stop reading the input once n tokens have been created\n"
      "        --locale <locale>\n"
      "        -m, --multi\n"
      "                allow the selection of multiple tokens\n"
      "        --multiline\n"
      "                implies --regex. enable multiline matching (affects ^ and $).\n"
      "        --match\n"
      "                the positional argument matches the tokens instead of the\n"
      "                delimiter. the match and each match group is a token\n"
      "        --max-lookbehind <# characters>\n"
      "                the max number of characters that the pattern can look before\n"
      "                its beginning. if not specified, it is auto detected from the\n"
      "                pattern but may not be accurate for nested lookbehinds\n"
      "        --no-warn\n"
      "        -o, --output-delimiter <delimiter, default: '\\n'>\n"
      "                an output delimiter is placed after each token in the output\n"
      "        --out <# tokens>\n"
      "                send only the first n tokens to the output or tui\n"
      "        -p, --prompt <tui prompt>\n"
      "        -r, --regex\n"
      "                use PCRE2 regex for the positional argument.\n"
      "        --read <# bytes, default: <buf-size>>\n"
      "                the number of bytes read from stdin per iteration\n"
      "        -s, --sort\n"
      "                sort each token lexicographically\n"
      "        --sed\n"
      "                --match, but also writes everything around the tokens, and the\n"
      "                match groups aren't used as individual tokens\n"
      "        --selection-order\n"
      "                sort the token output based on tui selection order instead of\n"
      "                the input order. an indicator displays the order\n"
      "        --sort-reverse\n"
      "                sort the tokens in reverse order\n"
      "        -t, --tui\n"
      "                display the tokens in a selection tui. ignores --out\n"
      "        --take <# tokens>\n"
      "                both --in and --out\n"
      "        --tenacious\n"
      "                on tui confirmed selection, do not exit; but still flush the\n"
      "                current selection to the output as a batch\n"
      "        -u, --unique\n"
      "                remove duplicate input tokens. leaves first occurrences\n"
      "        --use-delimiter\n"
      "                don't ignore a delimiter at the end of the input\n"
      "        --utf\n"
      "                enable regex UTF-8\n"
      "        --utf-allow-invalid\n"
      "                implies --utf, skips invalid codes\n"
      "        -y, --batch-print0\n"
      "                use null as the batch delimiter\n"
      "        -z, --print0\n"
      "                use null as the output delimiter\n"
      "        -0, --null, --read0\n"
      "                delimit the input on null chars\n"
      "        --0-auto-completion-strings\n"
      "        --\n"
      "                stop option parsing\n"
      "examples:\n"
      "        echo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
      "        echo -n \"1A2a3\" | choose -i \"a\"\n"
      "        echo -n \"a b c\" | choose -o, -b$'\\n' \" \" -m --tenacious\\\n"
      "                --selection-order -p \"space, enter, escape\" --tui\n"
      "        echo -n 'hello world' | choose -r --sub 'hello (\\w+)' 'hi $1'\n"
      "        echo -n 'every other word is printed here' | choose ' ' -r --out\\\n"
      "                --in-index=after -f '[02468]$' --sub '(.*) [0-9]+' '$1'\n"
      "        echo -en \"John Doe\\nApple\\nJohn Doe\\nBanana\\nJohn Smith\" | choose\\\n"
      "                -r --comp '^John' --comp-sort\n"
      "        echo -n \"a b c d e f\" | choose ' ' -rt --sub '.+' '$0 in:' --in-index\\\n"
      "                after --rm '^c' --sub '.+' '$0 out:' --out-index after\n"
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

  // signal sent to child process. wait for pclose to finish
  if (signal(SIGINT, [](int) {}) == SIG_IGN) {
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
  // rather than stopping on error, continue to parse all the arguments and show all errors
  bool arg_has_errors = false;
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {

        // messages
        {"version", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        // args
        {"output-delimiter", required_argument, NULL, 'o'},
        {"batch-delimiter", required_argument, NULL, 'b'},
        {"prompt", required_argument, NULL, 'p'},
        {"comp", required_argument, NULL, 0},
        {"sub", required_argument, NULL, 0},
        {"substitute", required_argument, NULL, 0},
        {"filter", required_argument, NULL, 'f'},
        {"remove", required_argument, NULL, 0},
        {"buf-size", required_argument, NULL, 0},
        {"buf-size-frag", required_argument, NULL, 0},
        {"rm", required_argument, NULL, 0},
        {"max-lookbehind", required_argument, NULL, 0},
        {"read", required_argument, NULL, 0},
        {"in", required_argument, NULL, 0},
        {"in-index", optional_argument, NULL, 0},
        {"locale", required_argument, NULL, 0},
        {"out", required_argument, NULL, 0},
        {"out-index", optional_argument, NULL, 0},
        {"replace", required_argument, NULL, 0},
        {"take", required_argument, NULL, 0},
        // options
        {"tui", no_argument, NULL, 't'},
        {"comp-sort", no_argument, NULL, 0},
        {"comp-unique", no_argument, NULL, 0},
        {"delimit-same", no_argument, NULL, 'd'},
        {"delimit-not-at-end", no_argument, NULL, 0},
        {"delimit-on-empty", no_argument, NULL, 0},
        {"end", no_argument, NULL, 'e'},
        {"flip", no_argument, NULL, 0},
        {"flush", no_argument, NULL, 0},
        {"ignore-case", no_argument, NULL, 'i'},
        {"multi", no_argument, NULL, 'm'},
        {"multiline", no_argument, NULL, 0},
        {"match", no_argument, NULL, 0},
        {"no-warn", no_argument, NULL, 0},
        {"regex", no_argument, NULL, 'r'},
        {"sed", no_argument, NULL, 0},
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
        {"0-auto-completion-strings", no_argument, NULL, 0},
        {NULL, 0, NULL, 0}

    };
    int c = getopt_long(argc, argv, "-vho:b:p:f:trdeimrsuyz0", long_options, &option_index);
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
          auto on_num_err = [&]() {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "--%s parse error\n", name);
            arg_has_errors = true;
          };

          // long option with argument
          if (strcmp("rm", name) == 0 || strcmp("remove", name) == 0) {
            UncompiledOrderedOp op{UncompiledOrderedOp::REMOVE, optarg, NULL};
            uncompiled_output.ordered_ops.push_back(op);
          } else if (strcmp("buf-size", name) == 0) {
            ret.buf_size = num::parse_unsigned<decltype(ret.buf_size)>(on_num_err, optarg, false);
          } else if (strcmp("buf-size-frag", name) == 0) {
            ret.buf_size_frag = num::parse_unsigned<decltype(ret.buf_size_frag)>(on_num_err, optarg, true, false);
          } else if (strcmp("in", name) == 0) {
            ret.in = num::parse_unsigned<decltype(ret.in)::value_type>(on_num_err, optarg);
          } else if (strcmp("max-lookbehind", name) == 0) {
            ret.max_lookbehind = num::parse_unsigned<decltype(ret.max_lookbehind)>(on_num_err, optarg, true, false);
          } else if (strcmp("read", name) == 0) {
            ret.bytes_to_read = num::parse_unsigned<decltype(ret.bytes_to_read)>(on_num_err, optarg, false, false);
          } else if (strcmp("out", name) == 0) {
            ret.out = num::parse_unsigned<decltype(ret.out)::value_type>(on_num_err, optarg);
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
          } else if (strcmp("replace", name) == 0) {
            for (UncompiledOrderedOp op : uncompiled_output.ordered_ops) {
              if (op.type != UncompiledOrderedOp::REMOVE && op.type != UncompiledOrderedOp::FILTER) {
                arg_error_preamble(argc, argv);
                fprintf(stderr, "option '--%s' can't be proceeded by an editing op\n", name);
                arg_has_errors = true;
                break;
              }
            }
            UncompiledOrderedOp op;
            op.type = UncompiledOrderedOp::REPLACE;
            op.arg0 = optarg;
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
            uncompiled_output.comp = optarg;
          } else if (strcmp("locale", name) == 0) {
            ret.locale = optarg;
          } else if (strcmp("take", name) == 0) {
            ret.in = num::parse_unsigned<decltype(ret.in)::value_type>(on_num_err, optarg);
            ret.out = ret.in;
          } else {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "unknown arg \"%s\"\n", name);
            arg_has_errors = true;
          }
        } else {
          // long option without argument or optional argument
          if (strcmp("flip", name) == 0) {
            ret.flip = true;
          } else if (strcmp("flush", name) == 0) {
            ret.flush = true;
          } else if (strcmp("comp-sort", name) == 0) {
            ret.comp_sort = true;
            ret.sort = true;
          } else if (strcmp("comp-unique", name) == 0) {
            ret.comp_unique = true;
          } else if (strcmp("delimit-not-at-end", name) == 0) {
            ret.delimit_not_at_end = true;
          } else if (strcmp("delimit-on-empty", name) == 0) {
            ret.delimit_on_empty = true;
          } else if (strcmp("match", name) == 0) {
            ret.match = true;
          } else if (strcmp("no-warn", name) == 0) {
            ret.can_drop_warn = false;
          } else if (strcmp("multiline", name) == 0) {
            uncompiled_output.re_options &= ~PCRE2_LITERAL;
            uncompiled_output.re_options |= PCRE2_MULTILINE;
          } else if (strcmp("sed", name) == 0) {
            ret.match = true;
            ret.sed = true;
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
          } else if (strcmp("0-auto-completion-strings", name) == 0) {
            const option* pos = long_options;
            while (pos->name) {
              fputs("--", stdout);
              puts(pos->name);
              ++pos;
            }
            exit(0);
          } else {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "unknown arg \"%s\"\n", name);
            arg_has_errors = true; // will never happen
          }
        }
      } break;
      case 1:
        // positional argument
        if (uncompiled_output.primary_set) {
          arg_error_preamble(argc, argv);
          fprintf(stderr,
                  "the positional arg must be specified once. "
                  "the second instance was found at position %d: \"%s\"\n",
                  optind - 1, optarg);
          arg_has_errors = true;
        }
        {
          size_t len = std::strlen(optarg);
          uncompiled_output.primary.resize(len);
          std::memcpy(uncompiled_output.primary.data(), optarg, len * sizeof(char));
          uncompiled_output.primary_set = true;
        }
        break;
      case 'v':
        print_version_message();
        break;
      case 'h':
        print_help_message();
        break;
      case 'd':
        ret.use_input_delimiter = true;
        ret.delimit_not_at_end = true;
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
        ret.tui = true;
        break;
      case 'u':
        ret.unique = true;
        break;
      case 'y':
        // these options are made available since null can't be typed as a command line arg
        // there's precedent elsewhere, e.g. find -print0 -> xargs -0
        ret.bout_delimiter = {'\0'};
        uncompiled_output.bout_delimiter_set = true;
        break;
      case 'z':
        ret.out_delimiter = {'\0'};
        break;
      case '0':
        uncompiled_output.primary = {'\0'};
        uncompiled_output.primary_set = true;
        break;
      case 'o': {
        // NOLINTNEXTLINE optarg guaranteed non-null since ':' follows 'o' in opt string
        size_t len = std::strlen(optarg);
        ret.out_delimiter.resize(len);
        std::memcpy(ret.out_delimiter.data(), optarg, len * sizeof(char));
      } break;
      case 'b': {
        // NOLINTNEXTLINE optarg guaranteed non-null since ':' follows 'b' in opt string
        size_t len = std::strlen(optarg);
        ret.bout_delimiter.resize(len);
        std::memcpy(ret.bout_delimiter.data(), optarg, len * sizeof(char));
        uncompiled_output.bout_delimiter_set = true;
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

  if (!uncompiled_output.bout_delimiter_set) {
    ret.bout_delimiter = ret.out_delimiter;
  }

  if (!uncompiled_output.primary_set) {
    if (ret.match) {
      // this isn't needed for any mechanical reason, only that the caller isn't doing something sane.
      arg_error_preamble(argc, argv);
      fputs("the positional arg must be specified with --match\n", stderr);
      arg_has_errors = true;
    }
    // default sep
    uncompiled_output.primary = {'\n'};
  }

  if (!ret.match) {
    for (UncompiledOrderedOp op : uncompiled_output.ordered_ops) {
      if (op.type == UncompiledOrderedOp::REPLACE) {
        arg_error_preamble(argc, argv);
        fputs("replacement op requires --match or --sed\n", stderr);
        arg_has_errors = true;
      }
    }
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
    ret.max_lookbehind = ret.primary ? regex::max_lookbehind_size(ret.primary) : 0;
  }
  if (ret.bytes_to_read == std::numeric_limits<decltype(ret.bytes_to_read)>::max()) {
    ret.bytes_to_read = ret.buf_size;
  }
  if (ret.buf_size_frag == std::numeric_limits<decltype(ret.bytes_to_read)>::max()) {
    // more than the match buffer size by default, since storing is less intensive
    if (auto mul_result = num::mul_overflow(ret.buf_size, (decltype(ret.buf_size))8)) {
      ret.buf_size_frag = *mul_result;
    } else {
      arg_error_preamble(argc, argv);
      fputs("multiply overflow on fragment buffer size (when calculating default value).\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  // bytes for number of characters
  if (ret.primary && regex::options(ret.primary) & PCRE2_UTF) {
    if (auto mul_result = num::mul_overflow(ret.max_lookbehind, (decltype(ret.max_lookbehind))str::utf8::MAX_BYTES_PER_CHARACTER)) {
      ret.max_lookbehind = *mul_result;
    } else {
      arg_error_preamble(argc, argv);
      fputs("multiply overflow on max lookbehind (bytes per utf8 char).\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  if (input == NULL) { // if not unit test
    // these aren't needed, but make sure the user is doing something sane
    if (ret.primary) {
      auto min = regex::min_match_length(ret.primary);
      if (regex::options(ret.primary) & PCRE2_UTF) {
        min *= str::utf8::MAX_BYTES_PER_CHARACTER;
      }
      if (min > ret.buf_size) {
        arg_error_preamble(argc, argv);
        fputs("the retain limit is too small and will cause the subject to never match.\n", stderr);
        exit(EXIT_FAILURE);
      }
    }

    if (ret.sed && !ret.is_direct_output()) {
      arg_error_preamble(argc, argv);
      fputs("--sed is incompatible with options that prevents direct output, including: sorting, flip, and tui.\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  if (isatty(fileno(ret.input))) {
    int exit_code = puts("Try 'choose --help' for more information.") < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    exit(exit_code);
  }

  return ret;
}

} // namespace choose
