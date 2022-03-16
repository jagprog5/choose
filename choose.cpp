#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

int main(int argc, char** argv) {
  char delimiter = ' ';

  if (argc == 2 && (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)) {
    puts("              .     ╒══════╕                    .    \n"
		 "   .. ........;;.   |      |  .. ...............;;.  \n"
		 "    ..::stdin:;;;;. |choose|   ..::chosen stdin:;;;;.\n"
		 "  . . ::::::::;;:'  |      | . . :::::::::::::::;;:' \n"
		 "              :'    ╘══════╛                    :'   \n\n"
		 "delimiter:\n"
		 "\tDefaults to space. Alternatively:\n"
		 "\t\t-d <delimiter character>\n"
		 "\tOr for the newline character:\n"
		 "\t\t-n\n\n"
		 "\tThe delimiter will NOT be used if contained in single or double quotes. Quotes in quotes should be escaped with \\\n");
	return 0;
  }

  if (argc == 3 && (strcmp("-d", argv[1]) == 0)) {
	  delimiter = argv[2][0];
  }

  // read stdin into inputs
  std::vector<std::vector<char>> inputs;

  {
  	inputs.emplace_back();
	
	bool in_single_quote = false;
	bool in_double_quote = false;

    char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
      if (ch == delimiter && !in_single_quote && !in_double_quote) {
        inputs.emplace_back();
      } else {
        inputs.rbegin()->push_back(ch);
      }
    }
  }

  for (auto v : inputs) {
	  puts(&*v.cbegin());
  }



//   char mesg[] = "Enter a string: "; /* message to be appeared on the screen */
//   char str[80];
//   int row, col;               /* to store the number of rows and *
//                                * the number of colums of the screen */
//   initscr();                  /* start the curses mode */
//   getmaxyx(stdscr, row, col); /* get the number of rows and columns */
//   mvprintw(row / 2, (col - strlen(mesg)) / 2, "%s", mesg);
//   /* print the message at the center of the screen */
//   getstr(str);
//   mvprintw(LINES - 2, 0, "You Entered: %s", str);
//   getch();
//   endwin();

  return 0;
}

// #include <curses.h>
// #include <iostream>
// #include <string>
// #include <vector>

// enum colors { NORMAL, SELECTED };
// unsigned int NUM_COLUMNS;
// unsigned int NUM_ROWS;

// void init_tui() {
//   if (initscr() == NULL) {
//     std::cerr << "ncurses init error\n";
//     exit(1);
//   }
//   // keypad(stdscr, TRUE); // enable arrow keys
//   // cbreak();	// pass keys directly from input without waiting for
//   newline
// 	// noecho();	// disable echo back of keys entered

//   if (nodelay(stdscr, false) == ERR) {
//     endwin();
//     std::cerr << "could not init no_delay\n";
//   }

//   start_color();
//   init_pair(NORMAL, COLOR_WHITE, COLOR_BLACK);
//   init_pair(SELECTED, COLOR_WHITE, COLOR_WHITE);

//   getmaxyx(stdscr, NUM_ROWS, NUM_COLUMNS);
// }

// std::vector<std::string> load_input() {
//   std::vector<std::string> ret;
//   std::string line;
//   while (std::getline(std::cin, line)) {
//     ret.push_back(std::move(line));
//   }
//   return ret;  // nrvo
// }

// void display(const std::vector<std::string>& strings,
//              std::vector<std::string>::size_type index) {

//   addstr(strings[index].c_str());
//   refresh();
// }

// int main() {
//   std::vector<std::string> lines = load_input();
//   init_tui();
//   display(lines, 0);
//   printw("Press any key to continue.");

//   while (getch() == ERR) {}

//   // for (auto line : lines) {
//   // std::cout << line << std::endl;
//   // }

//   endwin();  // deinit tui
//   // std::cout << (input == ERR);
//   return 0;
// }