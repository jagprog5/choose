#include <ncursesw/curses.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <memory>
#include <cstdlib>
#include <cmath>
#include <errno.h>
#include <vector>
#include <set>
#include <locale.h>
#include <cwchar>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

constexpr int PAIR_SELECTED = 1;

volatile sig_atomic_t sigint_occured = 0;

// read_done allows ctrl+c to stop the program when no input has been received yet
volatile sig_atomic_t read_done = 0;

void sig_handler(int) {
  if (read_done == 0) {
    exit(0);
  }
  sigint_occured = 1;
}

int main(int argc, char** argv) {
  setlocale(LC_ALL, ""); // for utf8
  
  // ===========================================================================
  // ============================= messages ====================================
  // ===========================================================================

  #define xstr(a) str(a)
  #define str(a) #a

  if (argc == 2 && (strcmp("-v", argv[1]) == 0 || strcmp("--version", argv[1]) == 0)) {
    return puts("choose 0.0.0, "
      "ncurses " xstr(NCURSES_VERSION_MAJOR) "." xstr(NCURSES_VERSION_MINOR) "." xstr(NCURSES_VERSION_PATCH) ", "
      "pcre2 " xstr(PCRE2_MAJOR) "." xstr(PCRE2_MINOR) ) < 0;
  }
  if (argc == 2 && (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    // respects 80 char width, and pipes the text to less to accommodate terminal height

    const char* const help_text = ""
"                             .     /======\\                                .    \n"
"   .. .......................;;.   |      |  .. ...........................;;.  \n"
"    ..::::::::::stdin::::::::;;;;. |choose|   ..::::::::chosen stdout::::::;;;;.\n"
"  . . :::::::::::::::::::::::;;:'  | >    | . . :::::::::::::::::::::::::::;;:' \n"
"                             :'    \\======/                                :'   \n"
"description:\n"
"        Splits the input into tokens based on a separator, and provides a text\n"
"        based ui for selecting which tokens are sent to the output.\n"
"usage:                    \n"
"        choose (-h|--help)\n"
"        choose (-v|--version)\n"
"        choose <options> [<input separator>]\n"
"                [(-o|--output-separator) <output separator, default: '\\n'>]\n"
"                [(-b|--batch-separator)\n"
"                        <batch separator, default: <output separator>>]\n"
"                [(-p|--prompt) <prompt>]\n"
"                [(--sub|--substitute) <target> <replacement>]\n"
"                [(-f|--filter) <filter>]\n"
"                [(--rm|--remove) <target>]\n"
"order of operations:\n"
"        the operations --substitute, --sort, --filter, and --unique are applied\n"
"        in the order they are stated\n"
"args:\n"
"         input separator: describes how to split the input into tokens. each\n"
"                          token is displayed for selection in the interface.\n"
"        output separator: if multiple tokens are selected (which is enabled via\n"
"                          -m), then an output separator is placed between each\n"
"                          token in the output\n"
"         batch separator: selecting multiple tokens and sending them to the\n"
"                          output together is a \"batch\". if multiple batches are\n"
"                          sent to the output (which is enabled via -t), then a\n"
"                          batch separator is used between batches, instead of an\n"
"                          output separator\n"
"            substitution: apply a text substitution on each token before it\n"
"                          appears in the interface. the target inherits the same\n"
"                          match options as the input separator. the replacement\n"
#ifdef PCRE2_SUBSTITUTE_LITERAL
"                          is literal iff the input separator is literal.\n"
#else 
"                          is a regex, but this will change if compiled with a \n"
"                          newer version of pcre2 where it will instead be\n"
"                          literal iff the input separator is literal\n"
#endif
"                  filter: remove tokens that don't match. it inherits the same\n"
"                          match options as the input separator\n"
"                  remove: this is the inverse of --filter\n"
"options:\n"
"        -d, --delimit   add a batch separator at the end of the output\n"
"        -e, --end       begin cursor at the bottom\n"
"        --flip          reverse the token order just before displaying\n"
"        -i, --ignore-case\n"
"        -m, --multi     allow the selection of multiple tokens\n"
"                        make the input separator case-insensitive\n"
"        --match         the input separator matches the tokens instead of the\n"
"                        separation between tokens. the match and each match\n"
"                        group is a token\n"
"        -r, --regex     use (PCRE2) regex for the input separator.\n"
"        -s, --sort      sort each token lexicographically\n"
"        --selection-order\n"
"                        sort the token output based on selection order instead\n"
"                        of input order\n"
"        -t, --tenacious don't exit on confirmed selection\n"
"        -u, --unique    remove duplicate input tokens. leaves first occurrences\n"
"        --use-delimiter\n"
"                        don't ignore a separator at the end of the output\n"
"        --utf           enable regex UTF-8\n"
"        -y, --batch-print0\n"
"                        use null as the batch separator\n"
"        -z, --print0    use null as the output separator\n"
"        -0, --null, --read0\n"
"                        use null as the input separator\n"
"                        this is the same as -r and \\x00\n"
"        --\n"
"                        stop option parsing\n"
"examples:\n"
"        echo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
"        echo -n \"1A2a3\" | choose -i \"a\"\n"
"        echo -n \"a b c\" | choose -o, -b$'\\n' \" \" -dmt --selection-order -pspace\n"
"        echo -n 'hello world' | choose -r --sub 'hello (\\w+)' 'hi $1'\n"
"controls:\n"
"        confirm selections: enter, d, or f\n"
"        multiple selection: space   <-}\n"
"         invert selections: i       <-} enabled with -m\n"
"          clear selections: c       <-}\n"
"                      exit: q, backspace, or escape\n"
"                 scrolling: arrow/page up/down, home/end, "
#ifdef BUTTON5_PRESSED
"mouse scroll, "
#endif
"j/k\n\n"
"to view the license, or report an issue, visit:\n"
"        github.com/jagprog5/choose\n";

    FILE *fp = popen("less", "w");
    if (fp == NULL) {
      perror(NULL);
      puts(help_text); // less didn't work, try the normal way anyways
      return 1;
    }

    if (fputs(help_text, fp) < 0) {
      perror(NULL);
      pclose(fp);
      puts(help_text);
      return 1;
    }

    int close_result = pclose(fp);
    if (close_result != 0) {
      if (close_result < 0) {
        perror(NULL);
      } else {
        fprintf(stderr, "error running less");
      }
      puts(help_text);
      return 1;
    }
    return 0;
  }

  if (signal(SIGINT, sig_handler) == SIG_IGN) {
    // for SIG_IGN: https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html
    // also, I don't check for SIG_ERR here since SIGINT as an arg guarantees this can't happen
    signal(SIGINT, SIG_IGN);
  }

  // ===========================================================================
  // ========================= cli arg parsing =================================
  // ===========================================================================

  // store the args that apply in the order that they appear
  struct OrderedOp {
    enum Type { SUB, SORT, FILTER, RM, UNIQUE };
    Type type;
    // args may be unset depending on op type
    const char* arg0 = 0;
    const char* arg1 = 0;
  };

  std::vector<OrderedOp> ordered_ops;

  uint32_t match_flags = PCRE2_LITERAL;
  bool selection_order = false;
  bool tenacious = false;
  bool use_input_delimiter = false;
  bool end = false;
  bool flip = false;
  bool multiple_selections = false;
  bool match = false;
  bool bout_delimit = false;

  // these options are made available since null can't be typed as a command line arg
  // there's precedent elsewhere, e.g. find -print0 -> xargs -0
  bool out_sep_null = false;
  bool bout_sep_null = false;

  // these pointers point inside one of the argv elements
  const char* in_separator = 0;
  const char* out_separator = "\n";
  const char* bout_separator = 0;
  const char* prompt = 0;

  {
    // stop parsing flags after -- is encountered
    bool options_stopped = false;

    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] == '-' && !options_stopped) {
        if (argv[i][1] == '\0') {
          fprintf(stderr, "dash specified without anything after it, in arg %d\n", i);
          return 1;
        }
        char* pos = argv[i] + 1;
        char ch;
        while ((ch = *pos)) {
          switch (ch) {
            // short flags
            default:
              fprintf(stderr, "unknown flag -%c in arg %d\n", ch, i);
              return 1;
              break;
            case 'd':
              bout_delimit = true;
              break;
            case 'e':
              end = true;
              break;
            case 'i':
              match_flags |= PCRE2_CASELESS;
              break;
            case 'm':
              multiple_selections = true;
              break;
            case 'r':
              match_flags &= ~PCRE2_LITERAL;
              break;
            case 's':
              ordered_ops.push_back(OrderedOp{OrderedOp::SORT});
              break;
            case 't':
              tenacious = true;
              break;
            case 'u':
              ordered_ops.push_back(OrderedOp{OrderedOp::UNIQUE});
              break;
            case 'y':
              bout_sep_null = true;
              break;
            case 'z':
              out_sep_null = true;
              break;
            case '0':
              in_separator = "\\x00";
              match_flags &= ~PCRE2_LITERAL;
              break;
            case '-':
              // long form of flags / args
              pos += 1; // point to just after the --
              if (*pos == '\0') {
                // -- found
                options_stopped = true;
              } else if (strcmp("delimit", pos) == 0) {
                bout_delimit = true;
              } else if (strcmp("end", pos) == 0) {
                end = true;
              } else if (strcmp("flip", pos) == 0) {
                flip = true;
              } else if (strcmp("ignore-case", pos) == 0) {
                match_flags |= PCRE2_CASELESS;
              } else if (strcmp("match", pos) == 0) {
                match = true;
              } else if (strcmp("multi", pos) == 0) {
                multiple_selections = true;
              } else if (strcmp("regex", pos) == 0) {
                match_flags &= ~PCRE2_LITERAL;
              } else if (strcmp("sort", pos) == 0) {
                ordered_ops.push_back(OrderedOp{OrderedOp::SORT});
              } else if (strcmp("selection-order", pos) == 0) {
                selection_order = true;
              } else if (strcmp("tenacious", pos) == 0) {
                tenacious = true;
              } else if (strcmp("use-delimiter", pos) == 0) {
                use_input_delimiter = true;
              } else if (strcmp("unique", pos) == 0) {
                ordered_ops.push_back(OrderedOp{OrderedOp::UNIQUE});
              } else if (strcmp("utf", pos) == 0) {
                match_flags |= PCRE2_UTF;
              } else if (strcmp("batch-print0", pos) == 0) {
                bout_sep_null = true;
              } else if (strcmp("print0", pos) == 0) {
                out_sep_null = true;
              } else if (strcmp("null", pos) == 0 || strcmp("read0", pos) == 0) {
                in_separator = "\\x00";
                match_flags &= ~PCRE2_LITERAL;
              } else if (strcmp("output-separator", pos) == 0) {
                // reuse the code below after putting the variables in an equivalent state
                ch = 'o';
                pos += strlen("output-separator") - 1; // done with this arg
                goto parse_param;
              } else if (strcmp("batch-separator", pos) == 0) {
                ch = 'b';
                pos += strlen("batch-separator") - 1;
                goto parse_param;
              } else if (strcmp("prompt", pos) == 0) {
                ch = 'p';
                pos += strlen("prompt") - 1;
                goto parse_param;
              } else if (strcmp("sub", pos) == 0) {
                ch = 's';
                pos += strlen("sub") - 1;
                goto parse_param;
              } else if (strcmp("substitute", pos) == 0) {
                ch = 's';
                pos += strlen("substitute") - 1;
                goto parse_param;
              } else if (strcmp("filter", pos) == 0) {
                ch = 'f';
                pos += strlen("filter") - 1;
                goto parse_param;
              } else if (strcmp("rm", pos) == 0) {
                ch = 'r';
                pos += strlen("rm") - 1;
                goto parse_param;
              } else if (strcmp("remove", pos) == 0) {
                ch = 'r';
                pos += strlen("remove") - 1;
                goto parse_param;
              } else {
                fprintf(stderr, "unknown long option in arg %d: \"%s\"\n", i, argv[i]);
                return 1;
              }
              goto next_arg;
              break;
            // optional args
            case 'o':
            case 'b':
            case 'p':
            case 'f':
              if (pos != argv[i] + 1) {
                // checking that the flag is just after the dash. e.g. -o, not -io
                fprintf(stderr, "-%c can't be specified with other flags "
                    "in the same arg, but it was in arg %d: \"%s\"\n", ch, i, argv[i]);
                return 1;
              }
              // if it is only -o, then the next arg is the separator
              if (*(pos + 1) == '\0') {
                parse_param:
                if (ch == 's') {
                  if (i >= argc - 2) {
                    fprintf(stderr, "\"%s\" must be followed by two args\n", argv[i]);
                    return 1;
                  }
                  i += 2;
                } else {
                  if (i == argc - 1) {
                    fprintf(stderr, "\"%s\" must be followed by an arg\n", argv[i]);
                    return 1;
                  }
                  // skip next cli arg. e.g. -o --output-thing-here
                  ++i;
                }
                if (ch == 'o') {
                  out_separator = argv[i];
                } else if (ch == 'b') {
                  bout_separator = argv[i];
                } else if (ch == 'p') {
                  prompt = argv[i];
                } else if (ch == 'f') {
                  ordered_ops.push_back(OrderedOp{OrderedOp::FILTER, argv[i]});
                } else if (ch == 'r') {
                  ordered_ops.push_back(OrderedOp{OrderedOp::RM, argv[i]});
                } else { // 's'
                  ordered_ops.push_back(OrderedOp{OrderedOp::SUB, argv[i - 1], argv[i]});
                }
              } else {
                // if not, then it is specified without a space, like -ostuff
                if (ch == 'o') {
                  out_separator = argv[i] + 2;
                } else if (ch == 'b') {
                  bout_separator = argv[i] + 2;
                } else { // 'p'
                  prompt = argv[i] + 2;
                }
                goto next_arg;
              }
              break;
          }
          ++pos;
        }
        next_arg:
          (void)0;
      } else {
        // the sole positional argument
        if (in_separator) {
          fprintf(stderr, "the input separator can only be specified once. "
            "the second instance was found at position %d: \"%s\"\n", i, argv[i]);
          return 1;
        }
        in_separator = argv[i];
      }
    }

    if (!bout_separator) {
      bout_separator = out_separator;
    }
  }

  // ===========================================================================
  // ================================ stdin ====================================
  // ===========================================================================

  std::vector<char> raw_input;  // hold stdin

  // points to sections of raw_input
  struct Token {
    const char* begin;
    const char* end;
    bool operator<(const Token& other) const {
      return std::lexicographical_compare(begin, end, other.begin, other.end);
    }
    bool operator==(const Token& other) const {
      return std::equal(begin, end, other.begin, other.end);
    }
  };
  std::vector<Token> tokens;

  {
    // =========================== read input ==================================

    char ch;
    bool read_flag = true;
    while (read_flag) {
      auto read_ret = read(STDIN_FILENO, &ch, sizeof(char));
      switch (read_ret) {
        default:
          raw_input.push_back(ch);
          break;
        case -1:
          perror(NULL);
          return read_ret;
        case 0:
          read_flag = false;
          break;
      }
    }
    read_done = 1;

    // ============================= make tokens ===============================

    const char* pos = &*raw_input.cbegin();

    if (!in_separator) {
      if (match) {
        // this isn't needed for any mechanical reason, only that the caller isn't doing something sane.
        fputs("the positional arg must be specified with --match.\n", stderr);
        return 1;
      }
      // default sep
      in_separator = "\n";
    }

    /*
     * I'm following the example here:
     * https://www.pcre.org/current/doc/html/pcre2demo.html
     */

    pcre2_code* re;
    pcre2_match_data* match_data;
    {
      // compile the regex
      PCRE2_SPTR pattern = (PCRE2_SPTR)in_separator;
      int errornumber;
      PCRE2_SIZE erroroffset;
      
      re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, match_flags, &errornumber,
                              &erroroffset, NULL);
      if (re == NULL) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        fprintf(stderr, "PCRE2 compilation of input separator failed at offset %d: %s\n",
                (int)erroroffset, buffer);
        return 1;
      }

      match_data = pcre2_match_data_create_from_pattern(re, NULL);
      if (match_data == NULL) {
        fputs("error allocating match data.", stderr);
        return 1;
      }
      PCRE2_SPTR subject = (PCRE2_SPTR) & *raw_input.cbegin();
      PCRE2_SIZE subject_length = (PCRE2_SIZE)raw_input.size();

      // old pcre2 incorrectly handles empty input with null ptr
      if (PCRE2_MAJOR <= 10 && PCRE2_MINOR <= 39) {
        if (subject == NULL and subject_length == 0) {
          goto match_done;
        }
      }

      int rc = pcre2_match(re, subject, subject_length, 0, 0, match_data, NULL);

      // check the result
      if (rc == PCRE2_ERROR_NOMATCH) {
        goto match_done;
      } else if (rc <= 0) {
        // < 0 is a regex error
        // = 0 means the match_data ovector wasn't big enough
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(rc, buffer, sizeof(buffer));
        fprintf(stderr, "Matching error in input separator: \"%s\"\n", buffer);
        return 1;
      }

      // access the result
      PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
      if (ovector[0] > ovector[1]) {
        fprintf(stderr,
                "In the input separator, \\K was used in an assertion to set the match start after its end.\n"
                "From end to start the match was: %.*s\n",
                (int)(ovector[0] - ovector[1]), (char*)(subject + ovector[1]));
        fprintf(stderr, "Run abandoned\n");
        return 1;
      }

      if (!match) {
        // just take the entire match, [0]
        PCRE2_SPTR substring_start = subject + ovector[0];
        PCRE2_SIZE substring_length = ovector[1] - ovector[0];
        tokens.push_back(Token{pos, (char*)substring_start});
        pos = (const char*)substring_start + substring_length;
      } else {
        // a token for the match and each match group 
        for (int i = 0; i < rc; i++) {
          PCRE2_SPTR substring_start = subject + ovector[2 * i];
          PCRE2_SIZE substring_length = ovector[2 * i + 1] - ovector[2 * i];
          tokens.push_back(Token{(char*)substring_start, (char*)(substring_start + substring_length)});
          if (i == 0) {
            pos = (const char*)substring_start + substring_length;
          }
        }
      }


      // for the next matches
      uint32_t options_bits;
      (void)pcre2_pattern_info(re, PCRE2_INFO_ALLOPTIONS, &options_bits);
      int utf8 = (options_bits & PCRE2_UTF) != 0;

      uint32_t newline;
      (void)pcre2_pattern_info(re, PCRE2_INFO_NEWLINE, &newline);
      int crlf_is_newline = newline == PCRE2_NEWLINE_ANY ||
                            newline == PCRE2_NEWLINE_CRLF ||
                            newline == PCRE2_NEWLINE_ANYCRLF;

      // get subsequent matches
      while (1) {
        uint32_t options = 0;
        PCRE2_SIZE start_offset = ovector[1];
        if (ovector[0] == ovector[1]) {
          if (ovector[0] == subject_length)
            break;
          options |= PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
        } else {
          PCRE2_SIZE startchar = pcre2_get_startchar(match_data);
          if (start_offset <= startchar) {
            if (startchar >= subject_length)
              break;
            start_offset = startchar + 1;
            if (utf8) {
              for (; start_offset < subject_length; start_offset++)
                if ((subject[start_offset] & 0xc0) != 0x80)
                  break;
            }
          }
        }

        // the next matches
        rc = pcre2_match(re, subject, subject_length, start_offset, options,
                         match_data, NULL);

        if (rc == PCRE2_ERROR_NOMATCH) {
          if (options == 0)
            break;
          ovector[1] = start_offset + 1;
          if (crlf_is_newline && start_offset < subject_length - 1 &&
              subject[start_offset] == '\r' &&
              subject[start_offset + 1] == '\n')
            ovector[1] += 1;
          else if (utf8) {
            while (ovector[1] < subject_length) {
              if ((subject[ovector[1]] & 0xc0) != 0x80)
                break;
              ovector[1] += 1;
            }
          }
          continue;
        }

        if (rc <= 0) {
          PCRE2_UCHAR buffer[256];
          pcre2_get_error_message(rc, buffer, sizeof(buffer));
          fprintf(stderr, "Matching error in input separator: \"%s\"\n", buffer);
          return 1;
        }

        if (ovector[0] > ovector[1]) {
          fprintf(stderr,
                "In the input separator, \\K was used in an assertion to set the match start after its end.\n"
                "From end to start the match was: %.*s\n",
                (int)(ovector[0] - ovector[1]), (char*)(subject + ovector[1]));
          fprintf(stderr, "Run abandoned\n");
          return 1;
        }

        if (!match) {
          // just take the entire match, [0]
          PCRE2_SPTR substring_start = subject + ovector[0];
          PCRE2_SIZE substring_length = ovector[1] - ovector[0];
          tokens.push_back(Token{pos, (char*)substring_start});
          pos = (const char*)substring_start + substring_length;
        } else {
          // a token for the match and each match group 
          for (int i = 0; i < rc; i++) {
            PCRE2_SPTR substring_start = subject + ovector[2 * i];
            PCRE2_SIZE substring_length = ovector[2 * i + 1] - ovector[2 * i];
            tokens.push_back(Token{(char*)substring_start, (char*)(substring_start + substring_length)});
            if (i == 0) {
              pos = (const char*)substring_start + substring_length;
            }
          }
        }
      }
    }
  match_done:
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    if (!match) {
      // last token (anchored to end of input)
      if (pos != &*raw_input.cend() || use_input_delimiter) {
        tokens.push_back(Token{pos, &*raw_input.cend()});
      }
    }
  }

  std::vector<std::unique_ptr<PCRE2_UCHAR[]>> substitution_buffers;
  substitution_buffers.resize(tokens.size());

  // apply each of the ordered ops
  auto op_pos = ordered_ops.begin();
  while (op_pos != ordered_ops.end()) {
    const OrderedOp& op = *op_pos;
    switch (op.type) {
      case OrderedOp::SUB:
        {
          auto substitution_buffers_pos = substitution_buffers.begin();
          PCRE2_SIZE replacement_length = strlen(op.arg1);
          PCRE2_SPTR replacement_str = (PCRE2_SPTR)op.arg1;

          PCRE2_SPTR pattern = (PCRE2_SPTR)op.arg0;
          int errornumber;
          PCRE2_SIZE erroroffset;

          pcre2_code* re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, match_flags, &errornumber, &erroroffset, NULL);
          if (re == NULL) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
            fprintf(stderr, "PCRE2 compilation of substitution target failed at offset %d: %s\n",
                    (int)erroroffset, buffer);
            
            return 1;
          }

          pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
          if (match_data == NULL) {
            fputs("error allocating match data.", stderr);
            return 1;
          }

          for (Token& t : tokens) {
            PCRE2_SPTR subject = (PCRE2_SPTR)t.begin;
            PCRE2_SIZE subject_length = (PCRE2_SIZE)(t.end - t.begin);
            uint32_t sub_flags = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
            #ifdef PCRE2_SUBSTITUTE_LITERAL
              if (match_flags & PCRE2_LITERAL) {
                  sub_flags |= PCRE2_SUBSTITUTE_LITERAL;
              }
            #endif

            PCRE2_SIZE output_size = 0; // initial pass calculates length of output
            pcre2_substitute(re,
                                subject,
                                subject_length,
                                0,
                                sub_flags,
                                NULL,
                                NULL,
                                replacement_str,
                                replacement_length,
                                NULL,
                                &output_size);
            

            std::unique_ptr<PCRE2_UCHAR[]> sub_out(new PCRE2_UCHAR[output_size]);
            sub_flags &= ~PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;

            // second pass
            int sub_rc = pcre2_substitute(re,
                                subject,
                                subject_length,
                                0,
                                sub_flags,
                                NULL,
                                NULL,
                                replacement_str,
                                replacement_length,
                                sub_out.get(),
                                &output_size);

            if (sub_rc < 0) {
              PCRE2_UCHAR buffer[256];
              pcre2_get_error_message(sub_rc, buffer, sizeof(buffer));
              fprintf(stderr, "PCRE2 substitution error: %s\n", buffer);
              return 1;
            }

            t = Token{(char*)sub_out.get(), (char*)sub_out.get() + output_size};
            *substitution_buffers_pos++ = std::move(sub_out);
          }
          pcre2_match_data_free(match_data);
          pcre2_code_free(re);
        }
        break;
      case OrderedOp::FILTER:
      case OrderedOp::RM:
        {
          PCRE2_SPTR pattern = (PCRE2_SPTR)op.arg0;
          int errornumber;
          PCRE2_SIZE erroroffset;
          pcre2_code* re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, match_flags, &errornumber,
                                  &erroroffset, NULL);
          if (re == NULL) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
            fprintf(stderr, "PCRE2 compilation of input separator failed at offset %d: %s\n",
                    (int)erroroffset, buffer);
            return 1;
          }

          // it seems strange to me that a match data is required...
          // https://stackoverflow.com/q/66162670/15534181
          pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
          if (match_data == NULL) {
            fputs("error allocating match data.", stderr);
            return 1;
          }

          auto new_end = std::remove_if(tokens.begin(), tokens.end(), [&](const Token& t) {
            PCRE2_SPTR subject = (PCRE2_SPTR)t.begin;
            PCRE2_SIZE subject_length = (PCRE2_SIZE)(t.end - t.begin);
            int rc = pcre2_match(re, subject, subject_length, 0, 0, match_data, NULL);
            
            if (rc == PCRE2_ERROR_NOMATCH) {
              return op.type != OrderedOp::RM;
            } else if (rc <= 0) {
              PCRE2_UCHAR buffer[256];
              pcre2_get_error_message(rc, buffer, sizeof(buffer));
              fprintf(stderr, "Matching error in filter: \"%s\"\n", buffer);
              pcre2_match_data_free(match_data);
              pcre2_code_free(re);
              substitution_buffers.clear();
              exit(1);
            }
            return op.type == OrderedOp::RM;
          });
          tokens.resize(new_end - tokens.begin());
          pcre2_match_data_free(match_data);
          pcre2_code_free(re);
        }
        break;
      case OrderedOp::SORT:
        // use stable_sort later if comparison becomes user defined.
        // right now sort is ok because equal elements are objectively equal.
        // this could break unique's leaving of first occurrence
        std::sort(tokens.begin(), tokens.end());
        break;
      case OrderedOp::UNIQUE:
        {
          bool is_sorted = false;
          for (auto rit = std::make_reverse_iterator(op_pos); rit != ordered_ops.rend(); ++rit) {
            const OrderedOp& previous_op = *rit;
            switch (previous_op.type) {
              case OrderedOp::SORT:
                is_sorted = true;
                break;
              case OrderedOp::SUB:
                is_sorted = false;
                break;
              case OrderedOp::UNIQUE:
              case OrderedOp::FILTER:
              case OrderedOp::RM:
                continue;
                break;
            }
          }
          if (is_sorted) {
            auto new_end = std::unique(tokens.begin(), tokens.end());
            tokens.resize(new_end - tokens.begin());
          } else {
            std::set<Token> seen;
            auto new_end = std::remove_if(tokens.begin(), tokens.end(), [&seen](const Token& value) {
                if (seen.find(value) != std::end(seen)) return true;
                seen.insert(value);
                return false;
            });
            tokens.resize(new_end - tokens.begin());
          }
        }
        break;
      default:
        break;
    }
    ++op_pos;
  }

  if (flip) {
    std::reverse(tokens.begin(), tokens.end());
  }

  // ===========================================================================
  // ============================= init tui ====================================
  // ===========================================================================

  bool is_tty = isatty(fileno(stdout));

  // don't use queued_output, instead send directly to the output
  bool immediate_output = tenacious && !is_tty;

  /*
  problem: re-entering ncurses mode clears the last line printed to the
  tty

  solution: queue up the output (it isn't needed until after the user
  exits the ui anyway), and send it at the end.
  */
  std::vector<char> queued_output;

  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  FILE* f = fopen("/dev/tty", "r+");
  if (f == NULL) {
    perror(NULL);
    return 1;
  }
  SCREEN* screen = newterm(NULL, f, f);
  if (screen == NULL) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }
  if (set_term(screen) == NULL) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }
  // enable arrow keys
  if (keypad(stdscr, true) == ERR) {
    // shouldn't be handled
  }
  // pass keys directly from input without buffering
  if (cbreak() == ERR) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }
  // disable echo back of keys entered
  if (noecho() == ERR) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }
  // invisible cursor
  if (curs_set(0) == ERR) {
    // shouldn't be handled
  }
  // as opposed to: nodelay(stdscr, false) // make getch block
  // a very large timeout still allows sigint to be effective immediately
  wtimeout(stdscr, std::numeric_limits<int>::max());
  // get mouse events right away
  if (mouseinterval(0) == ERR) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }

  WINDOW* prompt_window = 0;
  WINDOW* selection_window = 0;

  /*
   * the doc says that the mousemask must be set to enable mouse control,
   * however, it seems to work even without calling the function
   * 
   * calling the function makes the left mouse button captured, which prevents a
   * user from selecting and copying text
   * 
   * so with no benefit and a small downside, I leave this commented out
   * 
   * // #ifdef BUTTON5_PRESSED
   * //   mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
   * // #endif
   */

  // I don't handle ERR for anything color or attribute related since
  // the application still works, even on failure (just without color)
  // I also don't check ERR for ncurses printing, since if that stuff
  // is not working, it will be very apparent to the user
  start_color();
  init_pair(PAIR_SELECTED, COLOR_GREEN, COLOR_BLACK);

  int scroll_position = 0;
  int selection_position = end ? (int)tokens.size() - 1 : 0;

  int tenacious_single_select_indicator = 0;

  std::vector<int> selections;

  // ============================= resize handling =============================
on_resize:
  int num_rows; // of the entire screen
  int num_columns;

  int prompt_rows; 
  int selection_rows;

  getmaxyx(stdscr, num_rows, num_columns);

  if (num_rows < (prompt ? 2 : 1) || num_columns < 1) {
    // too small to be functional. lock out everything until it's big enough
    if (num_rows > 0 || num_columns > 0) {
      mvprintw(0, 0, "too small!");
    }
    int ch;
    do {
      ch = getch();
    } while (ch != KEY_RESIZE);
    goto on_resize;
  }

  // convert the prompt to a vector of wide char null terminating strings
  std::vector<std::vector<wchar_t>> prompt_lines;
  if (prompt) {
    std::mbstate_t ps; // text decode context
    memset(&ps, 0, sizeof(ps));

    const char* prompt_terminator = prompt;
    while (*prompt_terminator != 0) {
      ++prompt_terminator;
    }
    // prompt_terminator points to the position of the null terminator in the prompt
    // it is needed for the "n" arg in mbrtowc
    prompt_lines.emplace_back();

    const char* pos = prompt;
    const int INITIAL_AVAILBLE_WIDTH = num_columns - 2;
    int available_width = INITIAL_AVAILBLE_WIDTH;
    while (pos != prompt_terminator) {
      wchar_t ch;
      auto consume_ch = [&]() -> bool {
        size_t num_bytes = std::mbrtowc(&ch, pos, prompt_terminator - pos, &ps);
        if (num_bytes == 0) {
          // will never happen, since prompt_terminator points to the first null
          endwin();
          fputs("decode err in prompt\n", stderr);
          return false;
        } else if (num_bytes == (size_t)-1) {
          const char* err = strerror(errno);
          endwin();
          fprintf(stderr, "%s\n", err);
          return false;
        } else if (num_bytes == (size_t)-2) {
          endwin();
          fputs("incomplete multibyte in prompt\n", stderr);
          return false;
        }
        pos += num_bytes;
        return true;
      };
      if (!consume_ch()) {
        return 1;
      }

      auto insert_new_prompt_line = [&]() {
        available_width = INITIAL_AVAILBLE_WIDTH;
        prompt_lines.rbegin()->push_back(L'\0');
        prompt_lines.emplace_back();
      };

      switch (ch) {
        case L'\n':
          insert_new_prompt_line();
          break;
        case L'\r':
          // ignore
          break;
        case L'\t':
          ch = L' ';
          [[fallthrough]];
        default:
          available_width -= wcwidth(ch);
          #ifdef CHOOSE_PROMPT_LETTER_WRAP
            if (available_width < 0) {
              insert_new_prompt_line();
              available_width -= wcwidth(ch);
            }
          #else // word wrap
            if (available_width < 0) {
              // edge case on word boundary wrap, looks like:
              //                |
              //                V wrap end of screen here
              //      test  test  test  test   
              bool edge_case = ch == L' '
                            && prompt_lines.rbegin()->size() != 0
                            && (*prompt_lines.rbegin()->rbegin()) != L' ';

              // consume excess whitespace before line wrapping
              // if there was nothing except whitespace then abort the wrap
              while (ch == L' ') {
                if (pos >= prompt_terminator) goto get_out;
                if (!consume_ch()) {
                  return 1;
                }
              }

              insert_new_prompt_line();

              // we just inserted a line, so previous_line is
              // the line that we just tried to print past the width
              auto& previous_line = prompt_lines.rbegin()[1];

              // find the first character that isn't a space and is proceeded by a space
              // starting point is just before the null char
              wchar_t* word_begin = &previous_line.rbegin()[1]; // a b 0
              while (word_begin != &*previous_line.rend()) {
                  if (*word_begin == ' ' && word_begin[1] != ' ') {
                    ++word_begin;
                    break;
                  }
                  --word_begin;
              }

              if (!edge_case) {
                if (word_begin != &*previous_line.rend()) {
                  // word boundary found. cut the word into the new line from the previous line
                  wchar_t* word_begin_copy = word_begin;
                  wchar_t ch;
                  while ((ch = *word_begin++)) {
                    available_width -= wcwidth(ch);
                    prompt_lines.rbegin()->push_back(ch);
                  }
                  previous_line.erase(decltype(previous_line.end())(word_begin_copy), previous_line.end() - 1); // -1, don't erase null terminator
                }
              }

              available_width -= wcwidth(ch);
            }
          #endif
          prompt_lines.rbegin()->push_back(ch);
          break;
      }

    }

    [[maybe_unused]] get_out:

    prompt_lines.rbegin()->push_back(L'\0');
  }

  int initial_prompt_rows = prompt ? prompt_lines.size() + 2 : 0; // top and bottom border
  prompt_rows = initial_prompt_rows;
  selection_rows = num_rows - prompt_rows;

  if (selection_rows <= 0) {
    // the prompt has a fixed size, and the selection fills the remaining space
    // unless the selection would have 0 height, in which case it eats into the prompt to stay visible
    prompt_rows = initial_prompt_rows + selection_rows - 1;
    selection_rows = 1;
  }

  if (prompt_window) {
    if (delwin(prompt_window) == ERR) {
      endwin();
      fputs("ncurses err\n", stderr);
      return 1;
    }
  }
  if (selection_window) {
    if (delwin(selection_window) == ERR) {
      endwin();
      fputs("ncurses err\n", stderr);
      return 1;
    }
  }

  if (prompt) {
    prompt_window = newwin(prompt_rows, num_columns, 0, 0);
    if (!prompt_window) {
      endwin();
      fputs("ncurses err\n", stderr);
      return 1;
    }
    box(prompt_window, 0, 0);
    for (size_t i = 0; i < prompt_lines.size(); ++i) {
       mvwaddwstr(prompt_window, 1 + i, 1, &*prompt_lines[i].begin());
    }
  }

  selection_window = newwin(selection_rows, num_columns, prompt_rows, 0);
  if (!selection_window) {
    endwin();
    fputs("ncurses err\n", stderr);
    return 1;
  }

  // how close is the selection to the top or bottom while scrolling
#ifdef CHOOSE_NO_SCROLL_BORDER
  static constexpr int scroll_border = 0;
#else
  int scroll_border = selection_rows / 3;
#endif

  auto apply_constraints = [&]() {
    // constrain selection
    if (selection_position < 0) {
      selection_position = 0;
    } else if (selection_position >= (int)tokens.size()) {
      selection_position = (int)tokens.size() - 1;
    }

    // constrain scroll to selection
    int selection_pos_min = scroll_position;
    if (selection_position >= scroll_border) {
      selection_pos_min += scroll_border;
    }
    if (selection_position < selection_pos_min) {
      scroll_position -= selection_pos_min - selection_position;
    }

    int selection_pos_max = scroll_position + selection_rows - 1;
    if (selection_position < (int)tokens.size() - scroll_border) {
      selection_pos_max -= scroll_border;
    }

    if (selection_position > selection_pos_max) {
      scroll_position -= selection_pos_max - selection_position;
    }
  };

  // this scroll constraint that only comes into effect when resizing:
  // when the window height is increased at the end of the tokens,
  // if there are available token above then pull the entire scroll down
  if (scroll_position + selection_rows > (int)tokens.size() && (int)tokens.size() >= selection_rows) {
    scroll_position = (int)tokens.size() - selection_rows;
  }

  apply_constraints();

  refresh();

  // returns true on success, false on write failure
  auto send_output_separator = [&](bool sep_null, const char* const sep) -> bool {
    if (immediate_output) {
      if (sep_null) {
        return putchar('\0') != EOF;
      } else {
        return fprintf(stdout, "%s", sep) >= 0;
      }
    } else {
      if (sep_null) {
        return putchar('\0') != EOF;
      } else {
        char c;
        const char* delim_iter = sep;
        while ((c = *delim_iter++)) {
          queued_output.push_back(c);
        }
        return true;
      }
    }
  };

  if (tokens.size() == 0) {
    wattron(selection_window, A_DIM);
    mvprintw(0, 0, "No tokens.");
    int ch;
    do {
      ch = getch(); // wait for any exit or confirmation input
    } while (ch != '\n' && ch != 'd' && ch != 'f' && ch != KEY_BACKSPACE && ch != 'q' && ch != 27);
    if (endwin() == ERR) {
      return 1;
    }
    if (bout_delimit) {
      immediate_output = true;
      return !send_output_separator(bout_sep_null, bout_separator);
    }
    return 0;
  }

  while (true) {
    // =========================================================================
    // ============================= draw tui ==================================
    // =========================================================================

    werase(selection_window);

    const int selection_text_space = selections.size() == 0 || !selection_order ?
                                     0 : int(std::log10(selections.size())) + 1;

    for (int y = 0; y < selection_rows; ++y) {

      // =============================== draw line =============================

      int current_row = y + scroll_position;
      if (current_row >= 0 && current_row < (int)tokens.size()) {
        bool row_highlighted = current_row == selection_position;
        auto it = std::find(selections.cbegin(), selections.cend(), current_row);
        bool row_selected = it != selections.cend();

        if (selection_order && row_selected) {
          wattron(selection_window, A_DIM);
          mvwprintw(selection_window, y, 0, "%d", (int)(1 + it - selections.begin()));
          wattroff(selection_window, A_DIM);
        }

        if (row_highlighted || row_selected) {
          wattron(selection_window, A_BOLD);
          if (row_highlighted) {
            mvwaddch(selection_window, y, selection_text_space, tenacious_single_select_indicator & 0b1 ? '}' : '>');
          }
          if (row_selected) {
            wattron(selection_window, COLOR_PAIR(PAIR_SELECTED));
          }
        }

        // 2 leaves a space for the indicator '>' and a single space
        const int INITIAL_X = selection_text_space + 2;
        int x = INITIAL_X;
        auto pos = tokens[y + scroll_position].begin;
        auto end = tokens[y + scroll_position].end;

        // ============================ draw token =============================

        // if the token only contains chars which are not drawn visibly by choose
        bool invisible_only = true;
        std::mbstate_t ps; // text decode state gets reset per token
        memset(&ps, 0, sizeof(ps));
        while (pos != end) {
          wchar_t ch[2];
          ch[1] = L'\0';

          const char* escape_sequence = 0; // draw non printing ascii via escape sequence
          bool char_is_invalid = false;

          size_t num_bytes = std::mbrtowc(&ch[0], pos, end - pos, &ps);
          if (num_bytes == 0) {
            // null char was decoded. this is perfectly valid
            num_bytes = 1; // keep going
          } else if (num_bytes == (size_t)-1) {
            // this sets errno, but we can try?? to keep going
            num_bytes = 1;
            char_is_invalid = true;
          } else if (num_bytes == (size_t)-2) { 
            // the remaining bytes in the token do not complete a character
            num_bytes = end - pos; // go to the end
            char_is_invalid = true;
          }

          pos += num_bytes;

          if (char_is_invalid) {
            escape_sequence = "?";
          } else {
            // the escape sequence will first be this:
            // https://en.wikipedia.org/wiki/Escape_sequences_in_C#Table_of_escape_sequences
            // and if it doesn't exist there, then it takes the letters here:
            // https://flaviocopes.com/non-printable-ascii-characters/
            switch (ch[0]) {
            case L'\0':
              escape_sequence = "\\0";
              break;
            case 1:
              escape_sequence = "SOH";
              break;
            case 2:
              escape_sequence = "STX";
              break;
            case 3:
              escape_sequence = "ETX";
              break;
            case 4:
              escape_sequence = "EOT";
              break;
            case 5:
              escape_sequence = "ENQ";
              break;
            case 6:
              escape_sequence = "ACK";
              break;
            case L'\a':
              escape_sequence = "\\a";
              break;
            case L'\b':
              escape_sequence = "\\b";
              break;
            case L'\t':
              escape_sequence = "\\t";
              break;
            case L'\n':
              escape_sequence = "\\n";
              break;
            case L'\v':
              escape_sequence = "\\v";
              break;
            case L'\f':
              escape_sequence = "\\f";
              break;
            case L'\r':
              escape_sequence = "\\r";
              break;
            case 14:
              escape_sequence = "SO";
              break;
            case 15:
              escape_sequence = "SI";
              break;
            case 16:
              escape_sequence = "DLE";
              break;
            case 17:
              escape_sequence = "DC1";
              break;
            case 18:
              escape_sequence = "DC2";
              break;
            case 19:
              escape_sequence = "DC3";
              break;
            case 20:
              escape_sequence = "DC4";
              break;
            case 21:
              escape_sequence = "NAK";
              break;
            case 22:
              escape_sequence = "SYN";
              break;
            case 23:
              escape_sequence = "ETB";
              break;
            case 24:
              escape_sequence = "CAN";
              break;
            case 25:
              escape_sequence = "EM";
              break;
            case 26:
              escape_sequence = "SUB";
              break;
            case 27:
              escape_sequence = "\\e";
              break;
            case 28:
              escape_sequence = "FS";
              break;
            case 29:
              escape_sequence = "GS";
              break;
            case 30:
              escape_sequence = "RS";
              break;
            case 31:
              escape_sequence = "US";
              break;
            }
          }

          // the printing functions handle bound checking
          if (escape_sequence) {
            int len = strlen(escape_sequence);
            if (x + len <= num_columns) { // check if drawing the char would wrap
              wattron(selection_window, A_DIM);
              mvwaddstr(selection_window, y, x, escape_sequence);
              wattroff(selection_window, A_DIM);
            }
            x += len;
            invisible_only = false;
          } else {
            int len = wcwidth(ch[0]);
            if (x + len <= num_columns) {
              mvwaddwstr(selection_window, y, x, ch);
            }
            x += len;
            switch (ch[0]) {
            // I'm using this list:
            // https://invisible-characters.com/#:~:text=Invisible%20Unicode%20characters%3F,%2B2800%20BRAILLE%20PATTERN%20BLANK).
            // tab is handled as an escape sequence above
            case L' ': // 0020
            case L' ': // 00a0
            case L'­': // 00ad
            case L'͏': // 034f
            case L'؜': // 061c
            case L'ᅟ': // 115f
            case L'ᅠ': // 1160
            case L'឴': // 17b4
            case L'឵': // 17b5
            case L'᠎': // 180e
            case L' ': // 2000
            case L' ': // 2001
            case L' ': // 2002
            case L' ': // 2003
            case L' ': // 2004
            case L' ': // 2005
            case L' ': // 2006
            case L' ': // 2007
            case L' ': // 2008
            case L' ': // 2009
            case L' ': // 200a
            case L'​': // 200b
            case L'‌': // 200c
            case L'‍': // 200d
            case L'‎': // 200e
            case L'‏': // 200f
            case L' ': // 202f
            case L' ': // 205f
            case L'⁠': // 2060
            case L'⁡': // 2061
            case L'⁢': // 2062
            case L'⁣': // 2063
            case L'⁤': // 2064
            case L'⁫': // 206b
            case L'⁬': // 206c
            case L'⁭': // 206d
            case L'⁮': // 206e
            case L'⁯': // 206f
            case L'　': // 3000
            case L'⠀': // 2800
            case L'ㅤ': // 3164
            case L'﻿': // feff
            case L'ﾠ': // ffa0
            case L'𝅙': // 1d159
            case L'𝅳': // 1d173
            case L'𝅴': // 1d174
            case L'𝅵': // 1d175
            case L'𝅶': // 1d176
            case L'𝅷': // 1d177
            case L'𝅸': // 1d178
            case L'𝅹': // 1d179
            case L'𝅺': // 1d17a
              break;
            default:
              invisible_only = false;
              break;
            }
          }
          // draw ... at the right side of the screen if the x exceeds the width for this line
          if (x > num_columns) {
            wattron(selection_window, A_DIM);
            mvwaddstr(selection_window, y, num_columns - 3, "...");
            wattroff(selection_window, A_DIM);
            break; // cancel printing the rest of the token
          }
        }

        if (invisible_only) {
          const Token& token = tokens[y + scroll_position];
          wattron(selection_window, A_DIM);
          mvwprintw(selection_window, y, INITIAL_X, "\\s{%d bytes}", (int)(token.end - token.begin));
          wattroff(selection_window, A_DIM);
        }

        if (row_highlighted || row_selected) {
          wattroff(selection_window, A_BOLD);
          if (row_selected) {
            wattroff(selection_window, COLOR_PAIR(PAIR_SELECTED));
          }
        }
      }
    }

    wrefresh(prompt_window); // fine even if null
    wrefresh(selection_window);
    
    // =========================================================================
    // ============================ user input =================================
    // =========================================================================

    int ch = getch();

    if (sigint_occured != 0 || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
    cleanup_exit:
      if (endwin() == ERR) {
        // send_output_separator (below) might not work,
        // since ncurses is still capturing the output
        fputs("endwin err\n", stderr);
        return 1;
      }
      if (bout_delimit) {
        if (!send_output_separator(bout_sep_null, bout_separator)) {
          return 1;
        }
      }
      const auto* pos = &*queued_output.cbegin();
      while (pos != &*queued_output.cend()) {
        if (putchar(*pos++) == EOF) {
          fputs("stdout err\n", stderr);
          return 1;
        }
      }
      return 0;
    } else
#ifdef BUTTON5_PRESSED
        if (ch == KEY_MOUSE) {
      MEVENT e;
      if (getmouse(&e) != OK)
        continue;
      if (e.bstate & BUTTON4_PRESSED) {
        tenacious_single_select_indicator = 0;
        goto scroll_up;
      } else if (e.bstate & BUTTON5_PRESSED) {
        tenacious_single_select_indicator = 0;
        goto scroll_down;
      }
    } else
#endif
        if (ch == KEY_RESIZE) {
      goto on_resize;
    } else if (ch == 'i' && multiple_selections) {
      std::sort(selections.begin(), selections.end());
      auto selections_position = selections.cbegin();
      decltype(selections) replacement;
      replacement.resize(tokens.size() - selections.size());
      auto replacement_insertion = replacement.begin();

      for (int i = 0; i < (int)tokens.size(); ++i) {
        selections_position =
            std::lower_bound(selections_position, selections.cend(), i);
        if (selections_position == selections.cend() ||
            *selections_position != i) {
          *replacement_insertion++ = i;
        }
      }
      selections = std::move(replacement);
    } else if (ch == 'c') { // && multiple_selections
      selections.clear();
    } else if (ch == ' ' && multiple_selections) {
      auto pos =
          std::find(selections.cbegin(), selections.cend(), selection_position);
      if ((pos == selections.cend())) {
        selections.push_back(selection_position);
      } else {
        selections.erase(pos);
      }
    } else if (ch == '\n' || ch == 'd' || ch == 'f') {
      if (selections.size() == 0) {
        ++tenacious_single_select_indicator;
        selections.push_back(selection_position);
      }
      if (!selection_order) {
        std::sort(selections.begin(), selections.end());
      }

      if (immediate_output) {
        if (endwin() == ERR) {
          // this would be bad since text is going to the window instead of stdout
          fputs("endwin err\n", stderr);
          return 1;
        }
      }
      
      // send the batch separator between groups of selections
      // e.g. a|b|c=a|b|b=a|b|c 
      static bool first_output = true;
      if (!first_output) {
        if (!send_output_separator(bout_sep_null, bout_separator)) {
          endwin();
          fputs("stdout err\n", stderr);
          return 1;
        }
      }
      first_output = false;

      for (const auto& s : selections) {
        const Token& token = tokens[s];
        const char* iter = token.begin;
        while (iter != token.end) {
          if (immediate_output) {
            if (fputc(*iter, stdout) == EOF) {
              endwin();
              fputs("stdout err\n", stderr);
              return 1;
            }
          } else {
            queued_output.push_back(*iter);
          }
          ++iter;
        }
        // send the output separator if between two selections
        // e.g. a|b|c
        if (&s != &*selections.crbegin()) {
          if (!send_output_separator(out_sep_null, out_separator)) {
            endwin();
            fputs("stdout err\n", stderr);
            return 1;
          }
        }
      }

      if (immediate_output) {
        if (fflush(stdout) == EOF) {
          endwin();
          fputs("stdout err\n", stderr);
          return 1;
        }
      }
      if (tenacious) {
        selections.clear();
      } else {
        goto cleanup_exit;
      }
    } else {

      // ========================== movement commands ==========================
      
      tenacious_single_select_indicator = 0;

      if (ch == KEY_UP || ch == 'k') {
        [[maybe_unused]] scroll_up : --selection_position;
      } else if (ch == KEY_DOWN || ch == 'j') {
        [[maybe_unused]] scroll_down : ++selection_position;
      } else if (ch == KEY_HOME) {
        selection_position = 0;
      } else if (ch == KEY_END) {
        selection_position = (int)tokens.size() - 1;
      } else if (ch == KEY_PPAGE || ch == KEY_NPAGE) {
        if (ch == KEY_PPAGE) {
          scroll_position -= selection_rows;
          if (scroll_position < 0) {
            scroll_position = 0;
          }
        } else {
          scroll_position += selection_rows;
          if (scroll_position > (int)tokens.size() - selection_rows) {
            scroll_position = (int)tokens.size() - selection_rows;
          }
        }
        selection_position = scroll_position + selection_rows / 2;
        // skip applying constraints
        continue;
      }

      apply_constraints();
    }
  }
}
