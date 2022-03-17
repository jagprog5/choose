#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <vector>

char parse_cmd_args(int argc, char** argv) {
  // returns the delimiter arg or exits
  char delimiter = ' ';

  if (argc == 2 &&
      (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
        "              .     ╒══════╕                     .    \n"
        "   .. ........;;.   |      |  .. ................;;.  \n"
        "    ..::stdin:;;;;. |choose|   ..::chosen output:;;;;.\n"
        "  . . ::::::::;;:'  |  ↑↓  | . . ::::::::::::::::;;:' \n"
        "              :'    ╘══════╛                     :'   \n\n"
        "examples:\n"
        "\techo \"choose between these words\" | choose\n"
        "\thist() { history | grep \"$1\" | uniq | awk '{$1= ""; print $0}' | tac | choose -n | bash ; }\n"
        "controls:\n"
        "\tArrow/page up/down, mouse scroll, or jk to scroll.\n"
        "\tEnter or middle mouse button or d or f for a single selection\n"
        "\tSpace or left click for multiple selections.\n"
        "delimiter:\n"
        "\tDefaults to space. Alternatively:\n"
        "\t\t-d <delimiter character>\n"
        "\tOr for a newline delimiter:\n"
        "\t\t-n\n\n");
    exit(0);
  }

  if (argc == 3 && strcmp("-d", argv[1]) == 0) {
    delimiter = argv[2][0];
  } else if (argc == 2 && strcmp("-n", argv[1]) == 0) {
    delimiter = '\n';
  }
  return delimiter;
}

std::vector<std::vector<char>> parse_stdin(char delimiter) {
  std::vector<std::vector<char>> inputs;
  bool next_available = false;
  char ch;
  while (read(STDIN_FILENO, &ch, 1) > 0) {
    if (ch == delimiter) {
      next_available = false;
    } else {
      if (!next_available) {
        inputs.emplace_back();
        next_available = true;
      }
      inputs.rbegin()->push_back(ch);
    }
  }

  return inputs;
}

static const int PAIR_SELECTED = 1;

void init_tui() {
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
}

int main(int argc, char** argv) {
  char delimiter = parse_cmd_args(argc, argv);
  std::vector<std::vector<char>> inputs = parse_stdin(delimiter);

  int num_rows, num_columns;
  init_tui();
  // getmaxyx(stdscr, num_rows, num_columns);

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
    for (int y = 0; y < num_rows; ++y) {
      for (int x = 0; x < num_columns; ++x) {
        // clear the entire terminal
        mvprintw(y, x, " ");
      }

      // draw the text
      int current_row = y + scroll_position;
      if (current_row >= 0 && current_row < (int)inputs.size()) {
        bool row_highlighted = current_row == selection_position;
        bool row_selected = std::find(selections.cbegin(), selections.cend(),
                                      current_row) != selections.cend();
        if (row_highlighted || row_selected) {
          attron(A_BOLD);
          if (row_highlighted) {
            mvprintw(y, 0, ">");
          }
          if (row_selected) {
            attron(COLOR_PAIR(PAIR_SELECTED));
          }
        }

        mvprintw(y, 2, &*inputs[y + scroll_position].begin());

        if (row_highlighted || row_selected) {
          attroff(A_BOLD);
          if (row_selected) {
            attroff(COLOR_PAIR(PAIR_SELECTED));
          }
        }
      }
    }

    // handle input
    int ch = getch();

    MEVENT e;
    if (ch == KEY_MOUSE) {
      if (getmouse(&e) != OK)
        continue;
      if (e.bstate & BUTTON4_PRESSED) {
        --selection_position;
      } else if (e.bstate & BUTTON5_PRESSED) {
        ++selection_position;
      } else if (e.bstate & BUTTON1_PRESSED) {
        goto user_selection;
      } else if (e.bstate & BUTTON2_RELEASED) {
        goto user_finished;
      }
    } else if (ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
      endwin();
      exit(0);
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
          fputc(delimiter, stdout);
        }
        fprintf(stdout, "%s", &*inputs[s].cbegin());
        first_output = false;
      }
      exit(0);
    } else if (ch == KEY_UP || ch == 'k') {
      --selection_position;
    } else if (ch == KEY_DOWN || ch == 'j') {
      ++selection_position;
    } else if (ch == KEY_HOME) {
      selection_position = 0;
      scroll_position = 0;
    } else if (ch == KEY_END) {
      selection_position = inputs.size() - 1;
      scroll_position = inputs.size() - num_rows;
    } else if (ch == KEY_PPAGE) {
      selection_position -= num_rows;
      if (selection_position < scroll_border) {
        scroll_position = 0;
      }
    } else if (ch == KEY_NPAGE) {
      selection_position += num_rows;
      if (selection_position >= (int)inputs.size() - scroll_border) {
        scroll_position = inputs.size() - num_rows;
      }
    }

    if (selection_position < 0) {
      selection_position = 0;
    } else if (selection_position >= (int)inputs.size()) {
      selection_position = inputs.size() - 1;
    }

    // scroll to keep the selection in view
    if (selection_position >= scroll_border) {
      if (selection_position - scroll_border < scroll_position) {
        scroll_position = selection_position - scroll_border;
      }
    }

    if (selection_position < (int)inputs.size() - scroll_border) {
      if (selection_position + scroll_border - scroll_position >=
          num_rows - 1) {
        scroll_position = selection_position + scroll_border - num_rows + 1;
      }
    }
  }

  return 0;
}
