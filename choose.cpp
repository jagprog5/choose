#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cmath>
#include <vector>
#include <stdlib.h>

char parse_cmd_args(int argc, char** argv) {
  // returns the delimiter arg or exits
  char delimiter = ' ';

  if (argc == 2 && (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts(
    "              .     ╒══════╕                    .    \n"
		"   .. ........;;.   |      |  .. ...............;;.  \n"
		"    ..::stdin:;;;;. |choose|   ..::chosen stdin:;;;;.\n"
		"  . . ::::::::;;:'  |      | . . :::::::::::::::;;:' \n"
		"              :'    ╘══════╛                    :'   \n\n"
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
  inputs.emplace_back();
  char ch;
  while (read(STDIN_FILENO, &ch, 1) > 0) {
    if (ch == delimiter) {
      if (inputs.rbegin()->size() != 0) {
        inputs.rbegin()->push_back('\0');
        inputs.emplace_back();
      }
    } else {
      inputs.rbegin()->push_back(ch);
    }
  }
  inputs.pop_back();
  fflush(stdin);
  return inputs;
}

void init_tui(int& num_rows, int& num_columns) {
  // returns (num rows, num columns)

  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  FILE *f = fopen("/dev/tty", "r+");
  SCREEN *screen = newterm(NULL, f, f);
  set_term(screen);

  keypad(stdscr, TRUE); // enable arrow keys
  cbreak();	// pass keys directly from input without waiting for
	noecho();	// disable echo back of keys entered
  nodelay(stdscr, false); // make getch block

  // start_color();
  // init_pair(NORMAL, COLOR_WHITE, COLOR_BLACK);
  // init_pair(SELECTED, COLOR_WHITE, COLOR_WHITE);

  getmaxyx(stdscr, num_rows, num_columns);
}

int main(int argc, char** argv) {
  char delimiter = parse_cmd_args(argc, argv);
  std::vector<std::vector<char>> inputs = parse_stdin(delimiter);
  int numbering_width = (trunc(log10(inputs.size())) + 1) + 1; // + 1 for a space

  int num_rows, num_columns;
  init_tui(num_rows, num_columns);

  // user interface loop

  int start_index = 0; // for scrolling

  while (true) {
    for (int y = 0; y < num_rows; ++y) {
      if (y + start_index < inputs.size()) {
        mvprintw(y, 0, "%d", y + 1 + start_index);
        mvprintw(y, numbering_width, &*inputs[y + start_index].begin());
      }
    }

    int ch = getch();
    if (ch == KEY_UP) {
      --start_index;
    } else if (ch == KEY_DOWN) {
      ++start_index;
    }
  }

  endwin();

  return 0;
}
