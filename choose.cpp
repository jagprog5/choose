#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <limits>
#include <regex>
#include <vector>

static constexpr int PAIR_SELECTED = 1;

static volatile sig_atomic_t sigint_occured = 0;

static void sig_handler([[maybe_unused]] int sig) {
  sigint_occured = 1;
}

int main(int argc, char** argv) {
  signal(SIGINT, sig_handler);
  // ============================= args ===================================

  if (argc == 2 &&
      (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
        "              .     ╒══════╕                     .    \n"
        "   .. ........;;.   |      |  .. ................;;.  \n"
        "    ..::stdin:;;;;. |choose|   ..::chosen stdout:;;;;.\n"
        "  . . ::::::::;;:'  |  ⇑⇓  | . . ::::::::::::::::;;:' \n"
        "              :'    ╘══════╛                     :'   \n\n"
        "description:\n"
        "\tSplits an input into tokens based on a regex separator,\n"
        "\tand provides a text based ui for selecting which token are sent to "
        "the output.\n"
        "usage:\n"
        "\tchoose (-h|--help)\n"
        "\tchoose <options> [<input regex separator, default \\n>] [-o <output "
        "separator, default \\n>]\n"
        "options:\n"
        "\t-s sort the output based on selection order instead of input order\n"
        "\t-i make the input separator case insensitive\n"
        "\t-t tenacious; don't exit on confirmed selection. for "
        "non-tty output, each selection is flushed\n"
        "examples:\n"
        "\techo -n \"this 1 is 2 a 3 test\" | choose \" [0-9] \"\n"
        "\thist() { SELECTED=`history | grep \"$1\" | sed "
        "'s/^\\s*[0-9*]*\\s*//' | head -n -1 | tac | choose` && history -s "
        "\"$SELECTED\" && eval \"$SELECTED\" ; }\n"
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
        "\texit:\n"
        "\t\t- q\n"
        "\t\t- backspace\n"
        "\t\t- escape\n");
    return 0;
  }

  bool selection_order = false;
  std::regex_constants::syntax_option_type flags = std::regex::ECMAScript;
  bool tenacious = false;

  int in_separator_index = std::numeric_limits<int>::min();
  int out_separator_index = std::numeric_limits<int>::min();

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      char* pos = argv[i] + 1;
      char ch;
      while ((ch = *pos++)) {
        if (ch == 's') {
          selection_order = true;
        } else if (ch == 'i') {
          flags |= std::regex_constants::icase;
        } else if (ch == 't') {
          tenacious = true;
        } else if (ch == 'o') {
          if (i == argc - 1) {
            fprintf(stderr, "-o must be followed by an arg\n");
            return 1;
          }
          out_separator_index = i + 1;
        } else {
          fprintf(stderr, "unknown option: '%c'\n", ch);
          return 1;
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

  std::vector<std::vector<char>> tokens;
  {
    // read input
    std::vector<char> raw_input;
    char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
      raw_input.push_back(ch);
    }

    if (raw_input.size() == 0)
      return 0;

    // parse input
    std::regex re(in_separator_index == std::numeric_limits<int>::min()
                      ? "\n"
                      : argv[in_separator_index],
                  flags);
    std::cmatch match;

    const char* pos = &*raw_input.cbegin();
    while (std::regex_search(pos, match, re)) {
      const char* match_begin = match.cbegin()->first;
      auto length = match.cbegin()->length();

      bool has_length = pos != match_begin;
      if (has_length) {
        tokens.emplace_back();
      }

      while (pos != match_begin) {
        tokens.rbegin()->push_back(*pos++);
      }

      if (has_length) {
        tokens.rbegin()->push_back('\0');
      }

      pos += length;
    }

    // last token

    bool has_length = pos != &*raw_input.cend();

    if (has_length) {
      tokens.emplace_back();
    }

    while (pos != &*raw_input.cend()) {
      tokens.rbegin()->push_back(*pos++);
    }

    if (has_length) {
      tokens.rbegin()->push_back('\0');
    }
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
  
  int num_rows, num_columns;

  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  FILE* f = fopen("/dev/tty", "r+");
  SCREEN* screen = newterm(NULL, f, f);
  set_term(screen);

  keypad(stdscr, true);    // enable arrow keys
  cbreak();                // pass keys directly from input without buffering
  noecho();                // disable echo back of keys entered
  curs_set(0);             // invisible cursor

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

        int x = 2;
        auto pos = tokens[y + scroll_position].cbegin();
        auto end = tokens[y + scroll_position].cend() - 1;  // ignore null char
        while (pos != end) {
          char c = *pos++;
          if (c == '\n') {
            // draw newline chars differently
            attron(A_DIM);
            mvaddch(y, x++, '\\');
            mvaddch(y, x++, 'n');
            attroff(A_DIM);
          } else {
            mvaddch(y, x++, c);
          }
        }

        if (row_highlighted || row_selected) {
          attroff(A_BOLD);
          if (row_selected) {
            attroff(COLOR_PAIR(PAIR_SELECTED));
          }
        }
      }
    }

    // ========================== handle input ================================

    // handle input
    int ch = getch();

    if (sigint_occured || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
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
        if (immediate_output) {
          fprintf(stdout, "%s", &*tokens[s].cbegin());
        } else {
          char c;
          const char* token_iter = &*tokens[s].cbegin();
          while ((c = *token_iter++)) {
            queued_output.push_back(c);
          }
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
      selection_position = tokens.size() - 1;
      if ((int)tokens.size() > num_rows) {
        scroll_position = tokens.size() - num_rows;
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
        scroll_position = tokens.size() - num_rows;
      }
    }

    if (selection_position < 0) {
      selection_position = 0;
    } else if (selection_position >= (int)tokens.size()) {
      selection_position = tokens.size() - 1;
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
