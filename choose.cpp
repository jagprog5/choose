#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <regex>
#include <vector>

static constexpr int PAIR_SELECTED = 1;

int main(int argc, char** argv) {

  // ============================= args ===================================

  if (argc > 2) {
    fprintf(stderr, "At most 1 arg!\n");
    return 1;
  } else if (argc == 2 &&
             (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
        "              .     ╒══════╕                     .    \n"
        "   .. ........;;.   |      |  .. ................;;.  \n"
        "    ..::stdin:;;;;. |choose|   ..::chosen output:;;;;.\n"
        "  . . ::::::::;;:'  |  ⇑⇓  | . . ::::::::::::::::;;:' \n"
        "              :'    ╘══════╛                     :'   \n\n"
        "usage:\n"
        "\tchoose (-h|--help)\n"
        "\tchoose <short options> [<regex delimiter, default newline>]\n"
        "description:\n"
        "\tSplits an input into tokens based on a regex delimiter,\n"
        "\tand provides a text based ui for selecting which token are sent to "
        "the output.\n"
        "optionsTODO:\n"
        "\t-s sort the output based on selection order instead of input order\n"
        "\t-i make regex case insensitive\n"
        "examples:\n"
        "\techo -n \"this 1 is 2 a 3 test\" | ./choose \" [0-9] \"\n"
        "\thist() { history | grep \"$1\" | uniq | sed 's/^ *[0-9]*//' | tac | "
        "choose | bash ; }\n"
        "controls:\n"
        "\tArrow/page up/down,"
#ifdef BUTTON5_PRESSED
        " mouse scroll,"
#endif
        " or jk to scroll.\n"
        "\tEnter or middle mouse button or d or f for a single selection\n"
        "\tSpace or left click for multiple selections.\n");
    return 0;
  }

  // ============================= stdin ===================================

  std::vector<std::vector<char>> tokens;
  {
    // read input
    std::vector<char> raw_input;
    char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
      raw_input.push_back(ch);
    }

    if (raw_input.size() == 0) return 0;

    // parse input
    std::regex re(argc == 1 ? "\n" : argv[argc - 1]);
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

  int num_rows, num_columns;
  
  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  FILE* f = fopen("/dev/tty", "r+");
  SCREEN* screen = newterm(NULL, f, f);
  set_term(screen);

  keypad(stdscr, TRUE);    // enable arrow keys
  cbreak();                // pass keys directly from input without buffering
  noecho();                // disable echo back of keys entered
  nodelay(stdscr, false);  // make getch block
  curs_set(0);             // invisible cursor

  mouseinterval(0);  // get mouse events right away
  mousemask(ALL_MOUSE_EVENTS, NULL);

  start_color();
  init_pair(PAIR_SELECTED, COLOR_GREEN, COLOR_BLACK);

  int scroll_position = 0;
  int selection_position = 0;

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

    // ============================= draw tui ===================================

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
            mvaddch(y, 0, '>');
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

    // ============================= handle input ===================================

    // handle input
    int ch = getch();

    MEVENT e;
    if (ch == KEY_MOUSE) {
      if (getmouse(&e) != OK)
        continue;
      if (e.bstate & BUTTON1_PRESSED) {
        goto user_selection;
#ifdef BUTTON5_PRESSED
      } else if (e.bstate & BUTTON4_PRESSED) {
        --selection_position;
      } else if (e.bstate & BUTTON5_PRESSED) {
        ++selection_position;
#endif
      } else if (e.bstate & BUTTON2_RELEASED) {
        goto user_finished;
      }
    } else if (ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
      endwin();
      return 0;
    } else if (ch == KEY_RESIZE) {
      goto on_resize;
    } else if (ch == ' ') {
    user_selection:
      auto pos =
          std::find(selections.cbegin(), selections.cend(), selection_position);
      if ((pos == selections.cend())) {
        selections.push_back(selection_position);
      } else {
        selections.erase(pos);
      }
    } else if (ch == '\n' || ch == 'd' || ch == 'f') {
    user_finished:
      if (selections.size() == 0) {
        selections.push_back(selection_position);
      }
      endwin();
      bool first_output = true;
      for (const auto& s : selections) {
        if (!first_output) {
          putchar('\n');  // output delimiter
        }
        printf("%s", &*tokens[s].cbegin());
        first_output = false;
      }
      return 0;
    } else if (ch == KEY_UP || ch == 'k') {
      --selection_position;
    } else if (ch == KEY_DOWN || ch == 'j') {
      ++selection_position;
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
