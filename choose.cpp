#include <ncursesw/curses.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <errno.h>
#include <vector>
#include <locale.h>
#include <cwchar>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

constexpr int PAIR_SELECTED = 1;

volatile sig_atomic_t sigint_occured = 0;

// read_done is used to handle an edge case:
// sigint_occured writes the buffered output to stdout upon ctrl-c, however
// sigint_occured is only evaluated in the tui loop
// the "read" function blocks until there is input, meaning, if ctrl-c is
// pressed with no input to the program, it will hang.
// read_done allows exit on ctrl-c with no input
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

  if (argc == 2 && (strcmp("-v", argv[1]) == 0 || strcmp("--version", argv[1]) == 0)) {
    return puts("1.0.2") < 0;
  }
  if (argc == 2 && (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    // respects 80 char width, and pipes the text to less to accomodate terminal height

    const char* const help_text = ""
"                             .     /======\\                                .    \n"
"   .. .......................;;.   |      |  .. ...........................;;.  \n"
"    ..::::::::::stdin::::::::;;;;. |choose|   ..::::::::chosen stdout::::::;;;;.\n"
"  . . :::::::::::::::::::::::;;:'  | >    | . . :::::::::::::::::::::::::::;;:' \n"
"                             :'    \\======/                                :'   \n"
"description:\n"
"        Splits the input into tokens based on a separator, and provides a text\n"
"        based ui for selecting which tokens are sent to the output.\n"
"terminology:\n"
"               \"input separator\": describes how to split the input into tokens.\n"
"                                  each token is displayed for selection in the\n"
"                                  user interface.\n"
"              \"output separator\": if multiple tokens are selected (which is\n"
"                                  enabled via -m), then an output separator is\n"
"                                  placed between each token in the output.\n"
"               \"batch separator\": selecting multiple tokens and sending them to\n"
"                                  the output together is a \"batch\". if multiple\n"
"                                  batches are sent to the output (which is\n"
"                                  enabled via -t), then a batch separator is\n"
"usage:                            used between batches, instead of an output\n"
"        choose (-h|--help)        separator.\n"
"        choose (-v|--version)\n"
"        choose <options> [<input separator>]\n"
"                [-o <output separator, default: '\\n'>]\n"
"                [-b <batch separator, default: <output separator>>]\n"
"                [-p <prompt>]\n"
"options:\n"
"        -d delimit; add a batch separator at the end of the output\n"
"        -f flip the received token order\n"
"        -i make the input separator case-insensitive\n"
"        -m allow the selection of multiple tokens\n"
"        -r use (PCRE2) regex for the input separator\n"
"                If disabled, the default input separator is a newline character.\n"
"                If enabled, the default input separator is a regex which matches\n"
"                newline characters not contained in single or double quotes,\n"
"                excluding escaped quotes. regex101.com/r/RHyz6D/\n"
"        -s sort the token output based on selection order instead of input order\n"
"        -t tenacious; don't exit on confirmed selection\n"
"        -u enable regex UTF-8\n"
"        -y use null as the batch separator\n"
"        -z use null as the output separator\n"
"        -0 use null as the input separator\n"
"examples:\n"
"        echo -n \"this 1 is 2 a 3 test\" | choose -r \" [0-9] \"\n"
"        echo -n \"1A2a3\" | choose -i \"a\"\n"
"        echo -n \"a b c\" | choose -o \",\" -b $'\\n' \" \" -dmst\n\n"
"        hist() { # copy paste this into ~/.bashrc\n"
"          HISTTIMEFORMATSAVE=\"$HISTTIMEFORMAT\"\n"
"          trap 'HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\"' err\n"
"          unset HISTTIMEFORMAT\n"
"          SELECTED=`history | grep -i \"\\`echo \"$@\"\\`\" | \\\n"
"          sed 's/^ *[0-9]*[ *] //' | head -n-1 | choose -f -p \"Select a line to run.\"` && \\\n"
"          history -s \"$SELECTED\" && HISTTIMEFORMAT=\"$HISTTIMEFORMATSAVE\" && \\\n"
"          eval \"$SELECTED\" ; \n"
"        }\n"
"controls:\n"
"         confirm selections: enter, d, or f\n"
"         multiple selection: space   <-}\n"
"          invert selections: i       <-} enabled with -m\n"
"           clear selections: c       <-}\n"
"                       exit: q, backspace, or escape\n"
"                  scrolling: arrow/page up/down, home/end, "
#ifdef BUTTON5_PRESSED
"mouse scroll, "
#endif
"j/k\n\n"
"to view the license, or report an issue, visit:\n"
"        github.com/jagprog5/choose\n";

    FILE *fp = popen("less", "w");
    if (fp != NULL) {
      // opening up less succeeded
      bool fputs_failed = fputs(help_text, fp) < 0;
      bool pclose_failed = pclose(fp) == -1;
      if (fputs_failed) {
        puts(help_text);
      }
      if (pclose_failed) {
        fprintf(stderr, "%s\n", strerror(errno));
      }
      return pclose_failed || fputs_failed;
    } else {
      // opening up less failed
      puts(help_text);
      fprintf(stderr, "%s\n", strerror(errno));
      return 1;
    }
  }

  if (signal(SIGINT, sig_handler) == SIG_IGN) {
    // for SIG_IGN: https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html
    // also, I don't check for SIG_ERR here since SIGINT as an arg guarantees this can't happen
    signal(SIGINT, SIG_IGN);
  }

  // ===========================================================================
  // ========================= cli arg parsing =================================
  // ===========================================================================

  // FLAGS
  uint32_t flags = PCRE2_LITERAL;
  bool selection_order = false;
  bool tenacious = false;
  bool flip = false;
  bool multiple_selections = false;

  // these options are made available since null can't be typed as a command line arg
  // there's precedent elsewhere, e.g. find -print0 -> xargs -0
  bool in_sep_null = false;
  bool out_sep_null = false;
  bool bout_sep_null = false;
  bool bout_delimit = false;

  // these pointers point inside one of the argv elements
  const char* in_separator = 0;
  const char* out_separator = "\n";
  const char* bout_separator = 0;
  const char* prompt = 0;

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
          return 1;
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
            case 'm':
              multiple_selections = true;
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
            case 'u':
              flags |= PCRE2_UTF;
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
            case 'p':
              if (pos != argv[i] + 1) {
                // checking that the flag is just after the dash. e.g. -o, not -io
                fprintf(stderr, "-%c can't be specified with other flags "
                    "in the same arg, but it was in arg %d: \"%s\"\n", ch, i, argv[i]);
                return 1;
              }
              // if it is only -o, then the next arg is the separator
              if (*(pos + 1) == '\0') {
                if (i == argc - 1) {
                  fprintf(stderr, "-%c must be followed by an arg\n", ch);
                  return 1;
                }
                next_arg_reserved = true;
                if (ch == 'o') {
                  out_separator = argv[i + 1];
                } else if (ch == 'b') {
                  bout_separator = argv[i + 1];
                } else { // 'p'
                  prompt = argv[i + 1];
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
          fprintf(stderr, "only one positional argument is allowed. a second one was found at position %d\n", i);
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
          fprintf(stderr, "%s\n", strerror(errno));
          return read_ret;
        case 0:
          read_flag = false;
          break;
      }
    }
    read_done = 1;

    if (raw_input.empty()) {
      return 0;
    }

    // ============================= make tokens ===============================

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
    } else if (!in_separator) {
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

  if (tokens.empty()) {
    return 0;
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
    fprintf(stderr, "%s\n", strerror(errno));
    return 1;
  }
  SCREEN* screen = newterm(NULL, f, f);
  if (screen == NULL) {
    fputs("ncurses err\n", stderr);
    return 1;
  }
  if (set_term(screen) == NULL) {
    fputs("ncurses err\n", stderr);
    return 1;
  }
  // enable arrow keys
  if (keypad(stdscr, true) == ERR) {
    // shouldn't be handled
  }
  // pass keys directly from input without buffering
  if (cbreak() == ERR) {
    fputs("ncurses err\n", stderr);
    return 1;
  }
  // disable echo back of keys entered
  if (noecho() == ERR) {
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
    fputs("ncurses err\n", stderr);
    return 1;
  }

  WINDOW* prompt_window = 0;
  WINDOW* selection_window = 0;

  // this should be called before exiting after this point
  // (to clean up resources and restore terminal settings)
  // returns false if the output is still going to the screen and not stdout
  auto ncurses_deinit = [&]() {
    auto ret = endwin(); // it's ok to call endwin after endwin. it's idempotent
    delwin(prompt_window); // calling delwin is fine even if null
    delwin(selection_window);
    delscreen(screen);
    fclose(f);
    return ret != ERR;
  };

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
  int selection_position = 0;

  int tenacious_single_select_indicator = 0;

  std::vector<int> selections;

  int num_rows; // of the entire screen
  int num_columns;

  const int INITIAL_PROMPT_ROWS = prompt ? 1 + 2 : 0; // todo obtain height
  int prompt_rows; 
  int selection_rows;

  // ============================= resize handling =============================
on_resize:
  getmaxyx(stdscr, num_rows, num_columns);

  prompt_rows = INITIAL_PROMPT_ROWS;
  selection_rows = num_rows - prompt_rows;

  if (selection_rows <= 0) {
    // the prompt has a fixed size, and the selection fills the remaining space
    // unless the selection would have 0 height, in which case it eats into the prompt to stay visible
    prompt_rows = INITIAL_PROMPT_ROWS + selection_rows - 1;
    selection_rows = 1;
  }

  if (prompt_window) {
    if (delwin(prompt_window) == ERR) {
      ncurses_deinit();
      fputs("ncurses err\n", stderr);
      return 1;
    }
  }
  if (selection_window) {
    if (delwin(selection_window) == ERR) {
      ncurses_deinit();
      fputs("ncurses err\n", stderr);
      return 1;
    }
  }

  if (prompt) {
    prompt_window = newwin(prompt_rows, num_columns, 0, 0);
    if (!prompt_window) {
      ncurses_deinit();
      fputs("ncurses err\n", stderr);
      return 1;
    }
    box(prompt_window, 0, 0);
    mvwaddstr(prompt_window, 1, 1, prompt);
  }

  selection_window = newwin(selection_rows, num_columns, prompt_rows, 0);
  if (!selection_window) {
    ncurses_deinit();
    fputs("ncurses err\n", stderr);
    return 1;
  }

  // how close is the selection to the top or bottom while scrolling
  int scroll_border = 5;
  // disable border if the terminal is very small
  if (selection_rows <= scroll_border * 2) {
    scroll_border = 0;
  }

  // scroll to keep the selection on screen when a resize occured
  static bool previous_scroll_on_resize = false;
  bool scroll_on_resize = selection_position >= selection_rows - 1;

  if (scroll_on_resize) {
    scroll_position = selection_position - (selection_rows - 1);
  } else if (!scroll_on_resize && previous_scroll_on_resize) {
    // handle if the screen is resized fast
    scroll_position = 0;
  }
  previous_scroll_on_resize = scroll_on_resize;

  refresh();

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
          mvwprintw(selection_window, y, 0, "%d", 1 + it - selections.begin());
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
            invisible_only = false;
            wattron(selection_window, A_DIM);
            mvwaddstr(selection_window, y, x, escape_sequence);
            x += len;
            wattroff(selection_window, A_DIM);
          } else {
            mvwaddwstr(selection_window, y, x, ch);
            int display_width = wcwidth(ch[0]);
            x += display_width;
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

        if (row_highlighted || row_selected) {
          if (invisible_only) {
            const Token& token = tokens[y + scroll_position];
            wattron(selection_window, A_DIM);
            move(y, INITIAL_X);
            wprintw(selection_window, "\\s{%d bytes}", token.end - token.begin);
            wattroff(selection_window, A_DIM);
          }
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

    if (sigint_occured != 0 || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
    cleanup_exit:
      if (!ncurses_deinit()) {
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
          ncurses_deinit();
          fputs("endwin err\n", stderr);
          return 1;
        }
      }
      
      // send the batch separator between groups of selections
      // e.g. a|b|c=a|b|b=a|b|c 
      static bool first_output = true;
      if (!first_output) {
        if (!send_output_separator(bout_sep_null, bout_separator)) {
          ncurses_deinit();
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
              ncurses_deinit();
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
            ncurses_deinit();
            fputs("stdout err\n", stderr);
            return 1;
          }
        }
      }

      if (immediate_output) {
        if (fflush(stdout) == EOF) {
          ncurses_deinit();
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
        scroll_position = 0;
      } else if (ch == KEY_END) {
        selection_position = (int)tokens.size() - 1;
        if ((int)tokens.size() > selection_rows) {
          scroll_position = (int)tokens.size() - selection_rows;
        } else {
          scroll_position = 0;
        }
      } else if (ch == KEY_PPAGE) {
        selection_position -= selection_rows;
        if (selection_position < scroll_border) {
          scroll_position = 0;
        }
      } else if (ch == KEY_NPAGE) {
        if ((int)tokens.size() < selection_rows) {
          // handle an edge case where there is few tokens
          selection_position = (int)tokens.size() - 1;
        } else {
          selection_position += selection_rows;
          if (selection_position >= (int)tokens.size() - scroll_border) {
            scroll_position = (int)tokens.size() - selection_rows;
          }
        }
      }

      // ========================== scroll adjustments ===========================

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
            selection_rows - 1) {
          scroll_position = selection_position + scroll_border - selection_rows + 1;
        }
      }
    }
  }
}
