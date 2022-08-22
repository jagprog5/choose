#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <vector>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

static constexpr int PAIR_SELECTED = 1;

static volatile sig_atomic_t sigint_occured = 0;

// read_done is used to handle an edge case:
// sigint_occured writes the buffered output to stdout upon ctrl-c, however
// sigint_occured is only evaluated in the tui loop
// the "read" function blocks until there is input, meaning, if ctrl-c is
// pressed with no input to the program, it will hang.
// read_done allows exit on ctrl-c with no input
static volatile sig_atomic_t read_done = 0;

static void sig_handler([[maybe_unused]] int sig) {
  if (read_done == 0) {
    exit(0);
  }
  sigint_occured = 1;
}

int main(int argc, char** argv) {
  signal(SIGINT, sig_handler);

  // ============================= messages ================================
  if (argc == 2 &&
      (strcmp("-v", argv[1]) == 0 || strcmp("--version", argv[1]) == 0)) {
    puts("1.0.0");
    return 0;
  }
  if (argc == 2 &&
      (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
        "              .     ╒══════╕                     .    \n"
        "   .. ........;;.   |      |  .. ................;;.  \n"
        "    ..::stdin:;;;;. |choose|   ..::chosen stdout:;;;;.\n"
        "  . . ::::::::;;:'  |  ⇑⇓  | . . ::::::::::::::::;;:' \n"
        "              :'    ╘══════╛                     :'   \n"
        "description:\n"
        "        Splits the input into tokens based on a separator,\n"
        "        and provides a text based ui for selecting which tokens are sent to "
        "the output.\n"
        "terminology:\n"
        "               \"input separator\": describes where to split the input to create tokens\n"
        "              \"output separator\": placed between each token in the output\n"
        "        \"batch output separator\": selecting multiple tokens and sending them to the output together is a \"batch\"\n"
        // spaces here in case of inconsistent space vs tab length
        "                                  if multiple batches are sent to the output (by using tenacious mode),\n"
        "usage:                            then a batch separator is placed between each batch in the output,\n"
        "        choose (-h|--help)        instead of a normal output separator\n"
        "        choose (-v|--version)\n"
        "        choose <options> [<input separator>]\n"
        "                [-o <output separator, default: \\n>]\n"
        "                [-b <batch output separator, default: output separator>]\n"
        "options:\n"
        "        -d delimit; adds a trailing batch output separator at the end of the output\n"
        "        -f flip the token order\n"
        "        -i make the input separator case-insensitive\n"
        "        -r use (PCRE2) regex for the input separator\n"
        "                If disabled, the default input separator is a newline character.\n"
        "                If enabled, the default input separator is a regex which matches\n"
        "                newline characters not contained in single or double quotes, excluding escaped quotes\n"
        "        -s sort the output based on selection order instead of input order                    ^\n"
        "        -t tenacious; don't exit on confirmed selection                                       |\n"
        "        -y use null as the batch output separator                                   regex101.com/r/RHyz6D/\n"
        "        -z use null as the output separator\n"
        "        -0 use null as the input separator\n"
        "examples:\n"
        "        echo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
        "        echo -n \"1A2a3\" | choose -i \"a\"\n"
        "        echo -n \"1 2 3\" | choose -o \",\" -b $'\\n' \" \" -dst\n"
        "        hist() {\n"
        "          HISTTIMEFORMATSAVE=\"$HISTTIMEFORMAT\"\n"
        "          trap 'HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\"' err\n"
        "          unset HISTTIMEFORMAT\n"
        "          SELECTED=`history | grep -i \"\\`echo \"$@\"\\`\" | "
        "sed 's/^ *[0-9]*[ *] //' | head -n -1 | choose -f` && \\\n"
        "          history -s \"$SELECTED\" && HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\" && "
        "eval \"$SELECTED\" ; \n"
        "        }\n"
        "controls:\n"
        "         confirm selections: enter, d, or f        clear selections: c\n"
        "            batch selection: space                             exit: q, backspace, or escape\n"
        "          invert selections: i                            scrolling: arrow/page up/down, home/end, "
#ifdef BUTTON5_PRESSED
        "mouse scroll, "
#endif
        "j/k\n"
        "source:\n\tgithub.com/jagprog5/choose\n");
    return 0;
  }

  // ============================= args ===================================

  // FLAGS
  uint32_t flags = PCRE2_LITERAL;
  bool selection_order = false;
  bool tenacious = false;
  bool flip = false;

  // these options are made available since null can't be typed as a command line arg
  // there's precedent elsewhere, e.g. find -print0 -> xargs -0
  bool in_sep_null = false;
  bool out_sep_null = false;
  bool bout_sep_null = false;
  bool bout_delimit = false;

  // these pointers point inside one of the argv elements
  const char* in_separator = (char*)-1;
  const char* out_separator = "\n";
  const char* bout_separator = (char*)-1;

  {
    // e.g. in -o stuff_here, the arg after -o should not be parsed.
    bool next_arg_reserved = false;

    for (int i = 1; i < argc; ++i) {
      if (next_arg_reserved) {
        next_arg_reserved = false;
        continue;
      }

      if (argv[i][0] == '-') {
        if (argv[i][1] == '\0') {
          fprintf(stderr, "dash specified without anything after it, in arg %d\n", i);
          return -1;
        }
        char* pos = argv[i] + 1;
        char ch;
        while ((ch = *pos)) {
          switch (ch) {
            // flags
            default:
              fprintf(stderr, "unknown flag -%c in arg %d\n", ch, i);
              return 1;
              break;
            case 'd':
              bout_delimit = true;
              break;
            case 'f':
              flip = true;
              break;
            case 'i':
              flags |= PCRE2_CASELESS;
              break;
            case 'r':
              flags &= ~PCRE2_LITERAL;
              break;
            case 's':
              selection_order = true;
              break;
            case 't':
              tenacious = true;
              break;
            case 'y':
              bout_sep_null = true;
              break;
            case 'z':
              out_sep_null = true;
              break;
            case '0':
              in_sep_null = true;
              break;
            // optional args
            case 'o':
            case 'b':
              if (pos != argv[i] + 1) {
                // checking that the flag is just after the dash. e.g. -o, not -io
                fprintf(stderr, "-%c can't be specified with other flags "
                    "in the same arg, but it was in arg %d: \"%s\"\n", ch, i, argv[i]);
                return 1;
              }
              // if it is only -o, then the next arg is the seperator
              if (*(pos + 1) == '\0') {
                if (i == argc - 1) {
                  fprintf(stderr, "-%c must be followed by an arg\n", ch);
                  return 1;
                }
                next_arg_reserved = true;
                if (ch == 'o') {
                  out_separator = argv[i + 1];
                } else {
                  bout_separator = argv[i + 1];
                }
              } else {
                // if not, then it is specified without a space, like -ostuff
                if (ch == 'o') {
                  out_separator = argv[i] + 2;
                } else {
                  bout_separator = argv[i] + 2;
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
        if (in_separator != (char*)-1) {
          fprintf(stderr, "only one positional argument is allowed. a second one was found at position %d\n", i);
          return 1;
        }
        in_separator = argv[i];
      }
    }

    if (bout_separator == (char*)-1) {
      bout_separator = out_separator;
    }
  }

  // ============================= stdin ===================================

  std::vector<char> raw_input;  // hold stdin

  // points to sections of raw_input
  struct Token {
    const char* begin;
    const char* end;
  };
  std::vector<Token> tokens;

  {
    // =========================== read input ==============================

    char ch;
    while (read(STDIN_FILENO, &ch, sizeof(char)) > 0) {
      raw_input.push_back(ch);
    }
    read_done = 1;

    if (raw_input.empty()) {
      return 0;
    }

    // =========================== make tokens ==============================

    const char* pos = &*raw_input.cbegin();

    auto insert_token_and_advance = [&tokens, flip](const char*& begin,
                                                       const char* end) {
      if (begin == end)
        return;
      tokens.insert(flip ? tokens.begin() : tokens.end(), {begin, end});
      begin = end;
    };

    if (in_sep_null) {
      in_separator = ""; // points to a single null character
    } else if (in_separator == (char*)-1) {
      // default sep
      if (flags & PCRE2_LITERAL) {
        in_separator = "\n";
      } else {
        // https://regex101.com/r/RHyz6D/
        // the above link, but with "pattern" replaced with a newline char
        in_separator =
            R"(\G((?:(?:\\\\)*+\\["']|(?:\\\\)*(?!\\+['"])[^"'])*?\K(?:
|(?:\\\\)*'(?:\\\\)*+(?:(?:\\')|(?!\\')[^'])*(?:\\\\)*'(?1)|(?:\\\\)*"(?:\\\\)*+(?:(?:\\")|(?!\\")[^"])*(?:\\\\)*"(?1))))";
      }
    }

    /*
     * Using the PCRE2 c library is absolutely abhorrent.
     * It's just sooooo outdated.
     *   - Modern practices would use templates instead of preprocessor macros,
     *   - ovector is an implementation detail that should be abstracted away by
     * an object, instead of the user having to manually move everything around
     * and just KNOW what each index of the vector means
     *
     * I am using PCRE2 because std::regex and boost::regex both can't handle
     * the above regex I crafted online (regex flavour not supported).
     * Another alternative is jpcre2 (a c++ wrapper), but that one...
     * ... actually I probably should have used that instead. anyways:
     *
     * I'm following the example here:
     * https://www.pcre.org/current/doc/html/pcre2demo.html
     */

    {
      // compile the regex
      PCRE2_SPTR pattern = (PCRE2_SPTR)in_separator;
      int errornumber;
      PCRE2_SIZE erroroffset;
      
      pcre2_code* re = pcre2_compile(pattern, in_sep_null ? 1 : PCRE2_ZERO_TERMINATED, flags,
                                     &errornumber, &erroroffset, NULL);
      if (re == NULL) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        fprintf(stderr, "PCRE2 compilation failed at offset %d: %s\n",
                (int)erroroffset, buffer);
        return 1;
      }

      // match
      pcre2_match_data* match_data =
          pcre2_match_data_create_from_pattern(re, NULL);
      PCRE2_SPTR subject = (PCRE2_SPTR) & *raw_input.cbegin();
      PCRE2_SIZE subject_length = (PCRE2_SIZE)raw_input.size();
      int rc = pcre2_match(re, subject, subject_length, 0, 0, match_data, NULL);

      // check the result
      if (rc == PCRE2_ERROR_NOMATCH) {
        goto regex_done;
      } else if (rc <= 0) {
        // < 0 is a regex error
        // = 0 means the match_data ovector wasn't big enough
        // should never happen
        fprintf(stderr, "First match, matching error %d\n", rc);
        // not bothering to call free since program terminates
        return 1;
      }

      // access the result
      PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
      if (ovector[0] > ovector[1]) {
        fprintf(stderr,
                "\\K was used in an assertion to set the match start after its "
                "end.\n"
                "From end to start the match was: %.*s\n",
                (int)(ovector[0] - ovector[1]), (char*)(subject + ovector[1]));
        fprintf(stderr, "Run abandoned\n");
        return 1;
      }

      // just take the entire match, [0]
      PCRE2_SPTR substring_start = subject + ovector[0];
      PCRE2_SIZE substring_length = ovector[1] - ovector[0];
      insert_token_and_advance(pos, (char*)substring_start);
      pos += substring_length;

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
        // some weird implementation details I don't quite understand
        // again, I'm copying pasting from the example linked above
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
          fprintf(stderr, "Matching error %d\n", rc);
          return 1;
        }

        if (ovector[0] > ovector[1]) {
          fprintf(
              stderr,
              "\\K was used in an assertion to set the match start after its "
              "end.\n"
              "From end to start the match was: %.*s\n",
              (int)(ovector[0] - ovector[1]), (char*)(subject + ovector[1]));
          fprintf(stderr, "Run abandoned\n");
          return 1;
        }

        PCRE2_SPTR substring_start = subject + ovector[0];
        PCRE2_SIZE substring_length = ovector[1] - ovector[0];
        insert_token_and_advance(pos, (char*)substring_start);
        pos += substring_length;
      }
    }

  regex_done:
    // last token (anchored to end of input)
    insert_token_and_advance(pos, &*raw_input.cend());
  }

  // ============================ END OF REGEX STUFF ========================

  if (tokens.empty()) {
    return 0;
  }

  // ============================= init tui ===================================

  int is_tty = isatty(fileno(stdout));
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
  SCREEN* screen = newterm(NULL, f, f);
  set_term(screen);

  keypad(stdscr, true);  // enable arrow keys
  cbreak();              // pass keys directly from input without buffering
  noecho();              // disable echo back of keys entered
  curs_set(0);           // invisible cursor

  // as opposed to: nodelay(stdscr, false) // make getch block
  // a very large timeout still allows sigint to be effective immediately
  wtimeout(stdscr, std::numeric_limits<int>::max());

  mouseinterval(0);  // get mouse events right away

  /*
  the doc says that the mousemask must be set to enable mouse control,
  however, it seems to work even without calling the function

  calling the function makes the left mouse button captured, which prevents a
  user from selecting and copying text

  so with no benefit and a small downside, I leave this commented out
  */

  // #ifdef BUTTON5_PRESSED
  //   mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
  // #endif

  start_color();
  init_pair(PAIR_SELECTED, COLOR_GREEN, COLOR_BLACK);

  int scroll_position = 0;
  int selection_position = 0;

  int tenacious_single_select_indicator = 0;

  std::vector<int> selections;

  int num_rows, num_columns;
on_resize:
  getmaxyx(stdscr, num_rows, num_columns);

  // how close is the selection to the top or bottom before it scrolls
  int scroll_border = 5;
  // disable border if the terminal is very small
  if (num_rows < scroll_border * 2) {
    scroll_border = 0;
  }

  while (true) {
    // ============================= draw tui =================================

    erase();

    const int selection_text_space = selections.size() == 0 || !selection_order ?
                                     0 : int(std::log10(selections.size())) + 1;

    for (int y = 0; y < num_rows; ++y) {
      // draw the tokens
      int current_row = y + scroll_position;
      if (current_row >= 0 && current_row < (int)tokens.size()) {
        bool row_highlighted = current_row == selection_position;
        auto it = std::find(selections.cbegin(), selections.cend(), current_row);
        bool row_selected = it != selections.cend();

        if (selection_order && row_selected) {
          attron(A_DIM);
          mvprintw(y, 0, "%d", 1 + it - selections.begin());
          attroff(A_DIM);
        }

        if (row_highlighted || row_selected) {
          attron(A_BOLD);
          if (row_highlighted) {
            mvaddch(y, selection_text_space, tenacious_single_select_indicator & 0b1 ? '}' : '>');
          }
          if (row_selected) {
            attron(COLOR_PAIR(PAIR_SELECTED));
          }
        }

        // 2 leaves a space for the indicator '>' and a single space
        const int INITIAL_X = selection_text_space + 2;
        int x = INITIAL_X;
        auto pos = tokens[y + scroll_position].begin;
        auto end = tokens[y + scroll_position].end;
        // if the line only contains spaces, draw it differently
        bool only_spaces = true;
        while (pos != end) {
          char c = *pos++;

          if (c != ' ') {
            only_spaces = false;
          }

          // draw some characters differently
          char special_char;
          switch (c) {
            case '\a':
              special_char = 'a';
              break;
            case '\b':
              special_char = 'b';
              break;
            case (char)27:
              special_char = 'e';
              break;
            case '\f':
              special_char = 'f';
              break;
            case '\n':
              special_char = 'n';
              break;
            case '\r':
              special_char = 'r';
              break;
            case '\t':
              special_char = 't';
              break;
            case '\v':
              special_char = 'v';
              break;
            case '\0':
              special_char = '0';
              break;
            default:
              // default case. print char normally
              goto print_normal_char;
              break;
          }

          // if char is special {
            attron(A_DIM);
            mvaddch(y, x++, '\\');
            mvaddch(y, x++, special_char);
            attroff(A_DIM);
          // }
            goto after_print_normal_char;
          print_normal_char:
          // else {
            mvaddch(y, x++, c);
          // }
          after_print_normal_char:
            (void)0;
        }

        if (row_highlighted || row_selected) {
          if (only_spaces) {
            const Token& token = tokens[y + scroll_position];
            unsigned int size = token.end - token.begin;

            for (unsigned int i = INITIAL_X; i < INITIAL_X + size * 2; i += 2) {
              attron(A_DIM);
              mvaddch(y, i, '\\');
              mvaddch(y, i + 1, 's');
              attroff(A_DIM);
            }
          }
          attroff(A_BOLD);
          if (row_selected) {
            attroff(COLOR_PAIR(PAIR_SELECTED));
          }
        }
      }
    }

    auto send_output_seperator = [&](bool sep_null, const char* const sep) {
      if (immediate_output) {
        if (sep_null) {
          putchar('\0');
        } else {
          fprintf(stdout, "%s", sep);
        }
      } else {
        if (sep_null) {
          putchar('\0');
        } else {
          char c;
          const char* delim_iter = sep;
          while ((c = *delim_iter++)) {
            queued_output.push_back(c);
          }
        }
      }
    };

    // ========================== user input ================================

    int ch = getch();

    if (sigint_occured != 0 || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
    cleanup_exit:
      if (bout_delimit) {
        send_output_seperator(bout_sep_null, bout_separator);
      }
      endwin();
      delscreen(screen);
      fclose(f);
      const auto* pos = &*queued_output.cbegin();
      while (pos != &*queued_output.cend()) {
        putchar(*pos++);
      }
      return 0;
    } else
#ifdef BUTTON5_PRESSED
        if (ch == KEY_MOUSE) {
      MEVENT e;
      if (getmouse(&e) != OK)
        continue;
      if (e.bstate & BUTTON4_PRESSED) {
        goto scroll_up;
      } else if (e.bstate & BUTTON5_PRESSED) {
        goto scroll_down;
      }
    } else
#endif
        if (ch == KEY_RESIZE) {
      goto on_resize;
    } else if (ch == 'i') {
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
    } else if (ch == 'c') {
      selections.clear();
    } else if (ch == ' ') {
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
        endwin();
      }
      
      // send the batch output separator between groups of selections
      // e.g. a|b|c=a|b|b=a|b|c 
      static bool first_output = true;
      if (!first_output) {
        send_output_seperator(bout_sep_null, bout_separator);
      }
      first_output = false;

      for (const auto& s : selections) {
        const Token& token = tokens[s];
        const char* iter = token.begin;
        while (iter != token.end) {
          if (immediate_output) {
            fputc(*iter, stdout);
          } else {
            queued_output.push_back(*iter);
          }
          ++iter;
        }
        // send the output separator if between two selections
        // e.g. a|b|c
        if (&s != &*selections.crbegin()) {
          send_output_seperator(out_sep_null, out_separator);
        }
      }

      if (immediate_output) {
        fflush(stdout);
      }
      if (tenacious) {
        selections.clear();
      } else {
        goto cleanup_exit;
      }
    } else if (ch == KEY_UP || ch == 'k') {
      [[maybe_unused]] scroll_up : --selection_position;
      tenacious_single_select_indicator = 0;
    } else if (ch == KEY_DOWN || ch == 'j') {
      [[maybe_unused]] scroll_down : ++selection_position;
      tenacious_single_select_indicator = 0;
    } else if (ch == KEY_HOME) {
      selection_position = 0;
      scroll_position = 0;
    } else if (ch == KEY_END) {
      selection_position = (int)tokens.size() - 1;
      if ((int)tokens.size() > num_rows) {
        scroll_position = (int)tokens.size() - num_rows;
      } else {
        scroll_position = 0;
      }
    } else if (ch == KEY_PPAGE) {
      selection_position -= num_rows;
      if (selection_position < scroll_border) {
        scroll_position = 0;
      }
    } else if (ch == KEY_NPAGE) {
      selection_position += num_rows;
      if (selection_position >= (int)tokens.size() - scroll_border) {
        scroll_position = (int)tokens.size() - num_rows;
      }
    }

    if (selection_position < 0) {
      selection_position = 0;
    } else if (selection_position >= (int)tokens.size()) {
      selection_position = (int)tokens.size() - 1;
    }

    // scroll to keep the selection in view
    if (selection_position >= scroll_border) {
      if (selection_position - scroll_border < scroll_position) {
        scroll_position = selection_position - scroll_border;
      }
    }

    if (selection_position < (int)tokens.size() - scroll_border) {
      if (selection_position + scroll_border - scroll_position >=
          num_rows - 1) {
        scroll_position = selection_position + scroll_border - num_rows + 1;
      }
    }
  }

  return 0;
}
