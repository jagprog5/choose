#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <limits>
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

  // ============================= help message ================================

  if (argc == 2 &&
      (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
        "              .     ╒══════╕                     .    \n"
        "   .. ........;;.   |      |  .. ................;;.  \n"
        "    ..::stdin:;;;;. |choose|   ..::chosen stdout:;;;;.\n"
        "  . . ::::::::;;:'  |  ⇑⇓  | . . ::::::::::::::::;;:' \n"
        "              :'    ╘══════╛                     :'   \n\n"
        "description:\n"
        "\tSplits an input into tokens based on a separator, "
        "and provides a text based ui for selecting which token are sent to "
        "the output.\n"
        "usage:\n"
        "\tchoose (-h|--help)\n"
        "\tchoose <options> [<input separator>] [-o <output separator>]\n"
        "options:\n"
        "\t-f flip the token order\n"
        "\t-i make the match case-insensitive\n"
        "\t-r use (PCRE2) regex for the input separator\n"
        "\t\tIf disabled, the default input separator is a newline character\n"
        "\t\tIf enabled, the default input separator is a regex which matches newline characters not contained in single or double quotes, excluding escaped quotes\n"
        "\t-s sort the output based on selection order instead of input order\n"
        "\t-t tenacious; don't exit on confirmed selection. in "
        "non-tty outputs, each selection is flushed individually\n"
        "\t-0 use null as the input separator\n"
        "examples:\n"
        "\techo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
        "\techo -n \"1A2a3\" | choose -i \"a\"\n"
        "\thist() {\n"
        "\t\tHISTTIMEFORMATSAVE=\"$HISTTIMEFORMAT\"\n"
        "\t\ttrap 'HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\"' err\n"
        "\t\tunset HISTTIMEFORMAT\n"
        "\t\tSELECTED=`history | grep -i \"\\`echo \"$@\"\\`\" | sed 's/^ *[0-9]*[ *] //' | head -n -1 | choose -f` && \\\n"
        "\t\thistory -s \"$SELECTED\" && HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\" && "
        "eval \"$SELECTED\" ; \n"
        "\t}\n"
        "controls:\n"
        "\tscrolling:\n"
        "\t\t- arrow up/down\n"
        "\t\t- page up/down\n"
        "\t\t- home/end\n"
#ifdef BUTTON5_PRESSED
        "\t\t- mouse scroll\n"
#endif
        "\t\t- j/k\n"
        "\tconfirm selection:\n"
        "\t\t- enter\n"
        "\t\t- d or f\n"
        "\tmultiple selections:\n"
        "\t\t- space\n"
        "\tinvert selections:\n"
        "\t\t- t\n"
        "\tclear selections:\n"
        "\t\t- c\n"
        "\texit:\n"
        "\t\t- q\n"
        "\t\t- backspace\n"
        "\t\t- escape\n");
    return 0;
  }

  // ============================= args ===================================

  uint32_t flags = PCRE2_LITERAL;
  bool selection_order = false;
  bool tenacious = false;
  bool flip = false;
  bool null = false;

  // indices are initialized to invalid positions
  // if it is not set by the args, then it will use default values instead
  int in_separator_index = std::numeric_limits<int>::min();
  int out_separator_index = std::numeric_limits<int>::min();

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      char* pos = argv[i] + 1;
      char ch;
      while ((ch = *pos++)) {
        switch (ch) {
          case 'r':
            flags &= ~PCRE2_LITERAL;
            break;
          case 'i':
            flags |= PCRE2_CASELESS;
            break;
          case 's':
            selection_order = true;
            break;
          case 't':
            tenacious = true;
            break;
          case 'f':
            flip = true;
            break;
          case '0':
            null = true;
            break;
          case 'o':
            if (i == argc - 1) {
              fprintf(stderr, "-o must be followed by an arg\n");
              return 1;
            }
            out_separator_index = i + 1;
            break;
          default:
            fprintf(stderr, "unknown option: '%c'\n", ch);
            return 1;
            break;
        }
      }
    } else if (i != out_separator_index) {
      // the last non option argument is the separator
      in_separator_index = i;
    }
  }

  const char* out_separator =
      out_separator_index == std::numeric_limits<int>::min()
          ? "\n"
          : argv[out_separator_index];

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

    const char* input_separator;
    if (null) {
      input_separator = ""; // points to a single null character
    } else if (in_separator_index == std::numeric_limits<int>::min()) {
      // default sep
      if (flags & PCRE2_LITERAL) {
        input_separator = "\n";
      } else {
        // https://regex101.com/r/RHyz6D/
        // the above link, but with "pattern" replaced with a newline char
        input_separator =
            R"(\G((?:(?:\\\\)*+\\["']|(?:\\\\)*(?!\\+['"])[^"'])*?\K(?:
|(?:\\\\)*'(?:\\\\)*+(?:(?:\\')|(?!\\')[^'])*(?:\\\\)*'(?1)|(?:\\\\)*"(?:\\\\)*+(?:(?:\\")|(?!\\")[^"])*(?:\\\\)*"(?1))))";
      }
    } else {
      input_separator = argv[in_separator_index];
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
      PCRE2_SPTR pattern = (PCRE2_SPTR)input_separator;
      int errornumber;
      PCRE2_SIZE erroroffset;
      
      pcre2_code* re = pcre2_compile(pattern, null ? 1 : PCRE2_ZERO_TERMINATED, flags,
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
    for (int y = 0; y < num_rows; ++y) {
      // draw the tokens
      int current_row = y + scroll_position;
      if (current_row >= 0 && current_row < (int)tokens.size()) {
        bool row_highlighted = current_row == selection_position;
        bool row_selected = std::find(selections.cbegin(), selections.cend(),
                                      current_row) != selections.cend();
        if (row_highlighted || row_selected) {
          attron(A_BOLD);
          if (row_highlighted) {
            mvaddch(y, 0, tenacious_single_select_indicator & 0b1 ? '}' : '>');
          }
          if (row_selected) {
            attron(COLOR_PAIR(PAIR_SELECTED));
          }
        }

        static constexpr int INITIAL_X = 2;
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

    // ========================== user input ================================

    int ch = getch();

    if (sigint_occured != 0 || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
    cleanup_exit:
      endwin();
      queued_output.push_back('\0');
      fprintf(stdout, "%s", &*queued_output.cbegin());
      delscreen(screen);
      fclose(f);
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
      bool immediate_output = tenacious && !is_tty;
      if (immediate_output) {
        endwin();
      }
      static bool first_output = true;
      for (const auto& s : selections) {
        if (!first_output) {
          if (immediate_output) {
            fprintf(stdout, "%s", out_separator);
          } else {
            char c;
            const char* delim_iter = out_separator;
            while ((c = *delim_iter++)) {
              queued_output.push_back(c);
            }
          }
        }
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

        first_output = false;
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
