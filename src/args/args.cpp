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

#include "pipeline/head.hpp"
#include "pipeline/index.hpp"
#include "pipeline/replace.hpp"
#include "pipeline/reverse.hpp"
#include "pipeline/rm_or_filter.hpp"
#include "pipeline/sort.hpp"
#include "pipeline/substitute.hpp"
#include "pipeline/tail.hpp"
#include "pipeline/terminal.hpp"
#include "pipeline/token_output_stream.hpp"
#include "pipeline/unique.hpp"
#include "pipeline/user_defined_sort.hpp"
#include "utils/numeric_utils.hpp"

namespace choose {

void UncompiledCodes::compile(Arguments& output) {
  // create pipeline back to front.
  pipeline::NextUnit downstream = output.tui ? pipeline::NextUnit() : pipeline::NextUnit(pipeline::TokenOutputStream(output));
  for (pipeline::UncompiledPipelineUnit& unit : this->units) {
    auto pu = std::make_unique<pipeline::PipelineUnit>(unit.compile(std::move(downstream), this->re_options));
    downstream = std::move(pu);
  }
  output.nu = std::move(downstream);

  // see if single byte delimiter optimization applies
  if (!output.match && primary.size() == 1) {
    if (re_options & PCRE2_LITERAL) {
      // if the expression is literal then any single byte works
      output.in_byte_delimiter = primary[0];
    } else {
      // there's definitely better ways of recognizing if a regex pattern
      // consists of a single byte, but this is enough for common cases
      char ch = primary[0];
      if ((ch == '\n' || ch == '\0' || ch == '\t' || num::in(ch, '0', '9') || num::in(ch, 'a', 'z') || num::in(ch, 'A', 'Z'))) {
        output.in_byte_delimiter = ch;
      }
    }
  }

  if (!output.in_byte_delimiter) {
    output.primary = regex::compile(primary, re_options, "positional argument", PCRE2_JIT_PARTIAL_HARD);
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
      "pipeline units: applied in the order they are stated\n"
      "        -f, --filter <target>\n"
      "                remove tokens that don't match. inherits the same match options\n"
      "                as the positional argument\n"
      "        --head [<# tokens, default 10>]\n"
      "                take the first n tokens\n"
      "        --index [b[efore]|a[fter]|<default: b>]\n"
      "                concatenate the arrival order on each token\n"
      "        --replace <replacement>\n"
      "                a special case of the substitution op where the match target is\n"
      "                the positional argument. --match or --sed must be specified.\n"
      "        --reverse\n"
      "                flip the token order\n"
      "        --rm, --remove <target>\n"
      "                inverse of --filter\n"
      "        -s, --sort\n"
      "                sort the tokens lexicographically\n"
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
      "        --tail [<# tokens, default 10>]\n"
      "                take the last n tokens\n"
      "        -u, --unique\n"
      "                remove all except first instances of unique elements\n"
      "        --user-sort <less than comparison expr>\n"
      "                pairs of tokens are compared. if only one matches, then it is\n"
      "                less than the other. using this comparison, sort the tokens\n"
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
      "                this is only applicable if --match and --sed are not specified,\n"
      "                meaning the input delimiter is being matched. if the match\n"
      "                buffer is full when attempting to complete a match, the bytes at\n"
      "                the beginning of the buffer that have no possibility of being a\n"
      "                part of a match are moved to free up space in the buffer. they\n"
      "                are moved either directly to the output if the args allow for it\n"
      "                or to a separate buffer where they are accumulated until the\n"
      "                token is completed. this separate buffer is cleared if its size\n"
      "                would exceed this arg. the bytes are instead sent directly to\n"
      "                the output if no ordered ops are specified, and no tui used\n"
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
      "        --flush\n"
      "                makes the input unbuffered, and the output is flushed after each\n"
      "                token is written\n"
      "        -i, --ignore-case\n"
      "                make the positional argument case-insensitive\n"
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
      "        --sed\n"
      "                --match, but also writes everything around the tokens, and the\n"
      "                match groups aren't used as individual tokens\n"
      "        --selection-order\n"
      "                sort the token output based on tui selection order instead of\n"
      "                the input order. an indicator displays the order\n"
      "        -t, --tui\n"
      "                display the tokens in a selection tui. ignores --out\n"
      "        --tenacious\n"
      "                on tui confirmed selection, do not exit; but still flush the\n"
      "                current selection to the output as a batch\n"
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
      "        echo -n 'every other word is printed here' | choose ' ' -r\\\n"
      "                --index=after -f '[02468]$' --sub '(.*) [0-9]+' '$1'\n"
      "        echo -en \"John Doe\\nApple\\nJohn Doe\\nBanana\\nJohn Smith\" | choose\\\n"
      "                -r --user-sort '^John'\n"
      "        echo -n \"a b c d e f\" | choose ' ' -rt --sub '.+' '$0 in:' --index\\\n"
      "                after --rm '^c' --sub '.+' '$0 out:' --index after\n"
      "        echo -e \"this\\nis\\na\\ntest\" | choose -r --sed \".+\" --replace banana\n"
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
      "j/k\n"
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

// https://stackoverflow.com/a/69177115
#define OPTIONAL_ARGUMENT_IS_PRESENT ((optarg == NULL && optind < argc && argv[optind][0] != '-') ? (bool)(optarg = argv[optind++]) : (optarg != NULL))

// this function may call exit. input and output is for testing purposes; if
// unspecified, uses stdin and stdout, otherwise must be managed be the callee
// (e.g. fclose)

Arguments Arguments::create_args(int argc, char* const* argv, FILE* input = NULL, FILE* output = NULL) {
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
        {"sub", required_argument, NULL, 0},
        {"substitute", required_argument, NULL, 0},
        {"filter", required_argument, NULL, 'f'},
        {"remove", required_argument, NULL, 0},
        {"reverse", required_argument, NULL, 0},
        {"buf-size", required_argument, NULL, 0},
        {"buf-size-frag", required_argument, NULL, 0},
        {"rm", required_argument, NULL, 0},
        {"replace", required_argument, NULL, 0},
        {"max-lookbehind", required_argument, NULL, 0},
        {"read", required_argument, NULL, 0},
        {"locale", required_argument, NULL, 0},
        {"user-sort", required_argument, NULL, 0},
        {"out", required_argument, NULL, 0},
        {"index", optional_argument, NULL, 0},
        {"head", optional_argument, NULL, 0},
        {"tail", optional_argument, NULL, 0},
        // options
        {"tui", no_argument, NULL, 't'},
        {"delimit-same", no_argument, NULL, 'd'},
        {"delimit-not-at-end", no_argument, NULL, 0},
        {"delimit-on-empty", no_argument, NULL, 0},
        {"end", no_argument, NULL, 'e'},
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
    const char* name = long_options[option_index].name;
    auto on_num_err = [&]() {
      arg_error_preamble(argc, argv);
      fprintf(stderr, "--%s parse error\n", name);
      arg_has_errors = true;
    };

    switch (c) {
      default:
        arg_has_errors = true;
        break;
      case 0: {
        // long option
        if (optarg) {
          // long option with argument
          if (strcmp("buf-size", name) == 0) {
            ret.buf_size = num::parse_unsigned<decltype(ret.buf_size)>(on_num_err, optarg, false);
          } else if (strcmp("buf-size-frag", name) == 0) {
            ret.buf_size_frag = num::parse_unsigned<decltype(ret.buf_size_frag)>(on_num_err, optarg, true, false);
          } else if (strcmp("head", name) == 0) {
            size_t n = num::parse_unsigned<size_t>(on_num_err, optarg);
            uncompiled_output.units.push_back(pipeline::UncompiledHeadUnit(n));
          } else if (strcmp("tail", name) == 0) {
            size_t n = num::parse_unsigned<size_t>(on_num_err, optarg);
            uncompiled_output.units.push_back(pipeline::UncompiledTailUnit(n));
          } else if (strcmp("index", name) == 0) {
            pipeline::IndexUnit::Align align;
            if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
              align = pipeline::IndexUnit::BEFORE;
            } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
              align = pipeline::IndexUnit::AFTER;
            } else {
              arg_error_preamble(argc, argv);
              fprintf(stderr, "alignment must be before or after\n");
              arg_has_errors = true;
              align = pipeline::IndexUnit::BEFORE;
            }
            uncompiled_output.units.push_back(pipeline::UncompiledIndexUnit(align));
          } else if (strcmp("locale", name) == 0) {
            ret.locale = optarg;
          } else if (strcmp("user-sort", name) == 0) {
            uncompiled_output.units.push_back(pipeline::UncompiledUserDefinedSortUnit(optarg));
          } else if (strcmp("max-lookbehind", name) == 0) {
            ret.max_lookbehind = num::parse_unsigned<decltype(ret.max_lookbehind)>(on_num_err, optarg, true, false);
          } else if (strcmp("read", name) == 0) {
            ret.bytes_to_read = num::parse_unsigned<decltype(ret.bytes_to_read)>(on_num_err, optarg, false, false);
          } else if (strcmp("rm", name) == 0 || strcmp("remove", name) == 0) {
            uncompiled_output.units.push_back(pipeline::UncompiledRmOrFilterUnit(pipeline::RmOrFilterUnit::REMOVE, optarg));
          } else if (strcmp("replace", name) == 0) {
            uncompiled_output.units.push_back(pipeline::UncompiledReplaceUnit(optarg));
          } else if (strcmp("sub", name) == 0 || strcmp("substitute", name) == 0) {
            // special handing here since getopt doesn't normally support multiple arguments
            if (optind >= argc) {
              // ran off end
              arg_error_preamble(argc, argv);
              fprintf(stderr, "option '--%s' requires two arguments\n", name);
              arg_has_errors = true;
            } else {
              ++optind;
              uncompiled_output.units.push_back(pipeline::UncompiledSubUnit(argv[optind - 2], argv[optind - 1]));
            }
          } else {
            arg_error_preamble(argc, argv);
            fprintf(stderr, "unknown arg \"%s\"\n", name);
            arg_has_errors = true;
          }
        } else {
          // long option without argument or optional argument
          if (strcmp("flush", name) == 0) {
            ret.flush = true;
          } else if (strcmp("delimit-not-at-end", name) == 0) {
            ret.delimit_not_at_end = true;
          } else if (strcmp("delimit-on-empty", name) == 0) {
            ret.delimit_on_empty = true;
          } else if (strcmp("match", name) == 0) {
            ret.match = true;
          } else if (strcmp("no-warn", name) == 0) {
            ret.can_drop_warn = false;
          } else if (strcmp("head", name) == 0) {
            size_t n;
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              size_t n = num::parse_unsigned<size_t>(on_num_err, optarg);
            } else {
              n = 10;
            }
            uncompiled_output.units.push_back(pipeline::UncompiledHeadUnit(n));
          } else if (strcmp("tail", name) == 0) {
            size_t n;
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              size_t n = num::parse_unsigned<size_t>(on_num_err, optarg);
            } else {
              n = 10;
            }
            uncompiled_output.units.push_back(pipeline::UncompiledTailUnit(n));
          } else if (strcmp("multiline", name) == 0) {
            uncompiled_output.re_options &= ~PCRE2_LITERAL;
            uncompiled_output.re_options |= PCRE2_MULTILINE;
          } else if (strcmp("index", name) == 0) {
            pipeline::IndexUnit::Align align;
            if (OPTIONAL_ARGUMENT_IS_PRESENT) {
              if (strcasecmp("before", optarg) == 0 || strcasecmp("b", optarg) == 0) {
                align = pipeline::IndexUnit::BEFORE;
              } else if (strcasecmp("after", optarg) == 0 || strcasecmp("a", optarg) == 0) {
                align = pipeline::IndexUnit::AFTER;
              } else {
                arg_error_preamble(argc, argv);
                fprintf(stderr, "alignment must be before or after\n");
                arg_has_errors = true;
                align = pipeline::IndexUnit::BEFORE;
              }
            } else {
              align = pipeline::IndexUnit::BEFORE;
            }
            uncompiled_output.units.push_back(pipeline::UncompiledIndexUnit(align));
          } else if (strcmp("reverse", name) == 0) {
            uncompiled_output.units.push_back(pipeline::UncompiledReverseUnit());
          } else if (strcmp("sed", name) == 0) {
            ret.match = true;
            ret.sed = true;
          } else if (strcmp("selection-order", name) == 0) {
            ret.selection_order = true;
          } else if (strcmp("tenacious", name) == 0) {
            ret.tenacious = true;
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
        uncompiled_output.units.push_back(pipeline::UncompiledSortUnit());
        break;
      case 't':
        ret.tui = true;
        break;
      case 'u':
        uncompiled_output.units.push_back(pipeline::UncompiledUniqueUnit());
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
        uncompiled_output.units.push_back(pipeline::UncompiledRmOrFilterUnit(pipeline::RmOrFilterUnit::FILTER, optarg));
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
  }

  if (isatty(fileno(ret.input))) {
    int exit_code = puts("Try 'choose --help' for more information.") < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    exit(exit_code);
  }

  return ret;
}

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

std::vector<pipeline::SimplePacket> Arguments::create_packets() {
  const bool single_byte_delimiter = this->in_byte_delimiter.has_value();
  const bool is_utf = this->primary ? regex::options(this->primary) & PCRE2_UTF : false;
  const bool is_invalid_utf = this->primary ? regex::options(this->primary) & PCRE2_MATCH_INVALID_UTF : false;
  regex::match_data primary_data = this->primary ? regex::create_match_data(this->primary) : NULL;

  // single_byte_delimiter implies not match. stating below so the compiler can hopefully leverage it
  const bool is_match = !single_byte_delimiter && this->match;
  // sed implies is_match
  const bool is_sed = is_match && this->sed;
  const bool has_ops = !this->ordered_ops.empty();
  const bool flush = this->flush;

  char subject[this->buf_size]; // match buffer
  size_t subject_size = 0;      // how full is the buffer
  PCRE2_SIZE match_offset = 0;
  PCRE2_SIZE prev_sep_end = 0; // only used if !this->match
  uint32_t match_options = PCRE2_PARTIAL_HARD;

  std::vector<pipeline::SimplePacket> ret;

  // for when parts of a token are accumulated
  std::vector<char> fragment;

  // this lambda applies the operations specified in the args to a candidate token.
  // returns true iff this should be the last token added to the output
  auto process_token = [&](const char* begin, const char* end) -> bool {
    bool t_is_set = false;
    Token t;

    if (!fragment.empty()) {
      if (fragment.size() + (end - begin) > this->buf_size_frag) {
        drop_warning(*this);
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
      if (this->unique || this->comp_unique) {
        // some form on uniqueness is being used
        if (!uniqueness_check(output.size() - 1)) {
          // the element is not unique. nothing was added to the uniqueness set
          output.pop_back();
          return false;
        }
      }
      return true;
    };

    for (OrderedOp& op : this->ordered_ops) {
      if (RmOrFilterOp* rf_op = std::get_if<RmOrFilterOp>(&op)) {
        if (rf_op->removes(begin, end)) {
          return false;
        }
      } else if (InLimitOp* rf_op = std::get_if<InLimitOp>(&op)) {
        if (rf_op->finished()) {
          return true;
        }
      } else {
        if (tokens_not_stored && &op == &*this->ordered_ops.rbegin()) {
          if (ReplaceOp* rep_op = std::get_if<ReplaceOp>(&op)) {
            std::vector<char> out;
            rep_op->apply(out, subject, subject + subject_size, primary_data, this->primary);
            direct_output.write_output(&*out.cbegin(), &*out.cend());
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
            rep_op->apply(t.buffer, subject, subject + subject_size, primary_data, this->primary);
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
        choose::str::flush_f(this->output);
      }
      if (direct_output.out_count == this->out) {
        // code coverage reaches here. mistakenly shows finish_output as
        // unreached but throw is reached. weird.
        direct_output.finish_output();
        throw output_finished();
      }
      return false;
    } else {
      check_unique_then_append(); // result ignored
      return false;
    }
  };

  while (1) {
    char* write_pos = &subject[subject_size];
    size_t bytes_to_read = std::min(this->bytes_to_read, this->buf_size - subject_size);
    size_t bytes_read; // NOLINT
    bool input_done;   // NOLINT
    if (flush) {
      bytes_read = str::get_bytes_unbuffered(fileno(this->input), bytes_to_read, write_pos);
      input_done = bytes_read == 0;
    } else {
      bytes_read = str::get_bytes(this->input, bytes_to_read, write_pos);
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
        if (*single_byte_delimiter_pos == *this->in_byte_delimiter) {
          match_result = 1;
          break;
        }
        ++single_byte_delimiter_pos;
      }
    } else {
      match_result = regex::match(this->primary,                   //
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
          str::write_f(this->output, subject + match_offset, match.begin);
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
        new_subject_begin -= this->max_lookbehind;
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
            str::write_f(this->output, begin, end);
          }
        }

        char* to = subject;
        const char* from = new_subject_begin;
        if (from != to) {
          while (from < subject + subject_size) {
            *to++ = *from++;
          }
          subject_size -= from - to;
        } else if (subject_size == this->buf_size) {
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
                str::write_f(this->output, subject + match_offset, subject_effective_end);
              }
              subject_size = (subject + subject_size) - subject_effective_end;
              for (size_t i = 0; i < subject_size; ++i) {
                subject[i] = subject[(this->buf_size - subject_size) + i];
              }
            } else {
              // clear the buffer
              if (is_sed) {
                str::write_f(this->output, subject + match_offset, subject + subject_size);
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
                if (fragment.size() + (end - begin) > this->buf_size_frag) {
                  drop_warning(*this);
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
          if (prev_sep_end != subject_size || this->use_input_delimiter || !fragment.empty()) {
            // at this point subject_effective_end is subject + subject_size (since input_done)
            process_token(subject + prev_sep_end, subject_effective_end);
          }
        } else if (is_sed) {
          str::write_f(this->output, subject + match_offset, subject_effective_end);
        }
        break;
      }
    }
  }

  return ret;
}

} // namespace choose
