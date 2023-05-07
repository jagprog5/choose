#include <errno.h>
#include <locale>
#include <unistd.h>
#include <cmath>
#include <csignal>
#include "args.hpp"
#include "ncurses_wrapper.hpp"
#include "string_utils.hpp"
#include "token.hpp"

// signal flag set from SIGINT. used in ncurses input
volatile sig_atomic_t sigint_occurred = 0;
void sigint_handler(int) {
  sigint_occurred = 1;
}

// a single instance of UIState is used, with a static lifetime
struct UIState {
  choose::Arguments args;
  std::vector<choose::Token> tokens;
  bool output_is_tty;
  choose::BatchOutputStream os;

  // ncurses
  static constexpr int PAIR_SELECTED = 1;
  WINDOW* prompt_window = 0;
  WINDOW* selection_window = 0;

  // the line offset of elements in the tui
  int scroll_position = 0;
  // the cursor position in the tui
  int selection_position = 0;
  // if args.tenacious is true, this provides feedback to the user on selection confirmation
  int tenacious_single_select_indicator = 0;

  int num_rows = 0;        // the screen height
  int num_columns = 0;     // the screen width
  int prompt_rows = 0;     // the screen height available to the prompt
  int selection_rows = 0;  // the screen height available to the selection

  // the indices of the tokens selected by the user
  std::vector<int> selections = {};
  // word wrapped lines which display the prompt text
  std::vector<std::vector<wchar_t>> prompt_lines = {};

  UIState(const UIState& o) = delete;
  UIState& operator=(const UIState&) = delete;
  UIState(UIState&& o) = delete;
  UIState& operator=(UIState&&) = delete;
  ~UIState() {}

  // this modifies selection_position and scroll_position appropriately
  void apply_constraints() {
// how close is the selection to the top or bottom while scrolling
#ifdef CHOOSE_NO_SCROLL_BORDER
    static constexpr int scroll_border = 0;
#else
    int scroll_border = selection_rows / 3;
#endif

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
  }

  void on_resize() {
  again:
    getmaxyx(stdscr, num_rows, num_columns);
    if (num_rows < (args.prompt ? 2 : 1) || num_columns < 1) {
      // too small to be functional. lock out everything until it's big enough
      if (num_rows > 0 || num_columns > 0) {
        mvprintw(0, 0, "too small!");
      }
      int ch;
      do {
        ch = getch();
      } while (ch != KEY_RESIZE);
      goto again;
    }

    refresh();

    if (args.prompt) {
      prompt_lines = choose::str::create_prompt_lines(args.prompt, num_columns - 2);
    }

    prompt_rows = args.prompt ? prompt_lines.size() + 2 : 0;
    selection_rows = num_rows - prompt_rows;

    if (selection_rows <= 0) {
      // the prompt has a fixed size, and the selection fills the remaining space
      // unless the selection would have 0 height, in which case it eats into the prompt to stay visible
      prompt_rows += selection_rows - 1;
      selection_rows = 1;
    }

    bool first_time = selection_window == 0;

    int selection_window_y = args.end ? 0 : prompt_rows;
    int prompt_window_y = args.end ? selection_rows : 0;

    if (first_time) {
      selection_window = choose::nc::newwin(selection_rows, num_columns, selection_window_y, 0);
      if (args.prompt) {
        prompt_window = choose::nc::newwin(prompt_rows, num_columns, prompt_window_y, 0);
        box(prompt_window, 0, 0);
        for (size_t i = 0; i < prompt_lines.size(); ++i) {
          mvwaddwstr(prompt_window, 1 + i, 1, &*prompt_lines[i].begin());
        }
      }
    } else {
      choose::nc::wresize(selection_window, selection_rows, num_columns);
      choose::nc::mvwin(selection_window, selection_window_y, 0);
      if (prompt_window) {
        choose::nc::wresize(prompt_window, prompt_rows, num_columns);
        choose::nc::mvwin(prompt_window, prompt_window_y, 0);
      }
    }

    if (prompt_window) {
      werase(prompt_window);
      box(prompt_window, 0, 0);
      for (size_t i = 0; i < prompt_lines.size(); ++i) {
        mvwaddwstr(prompt_window, 1 + i, 1, &*prompt_lines[i].begin());
      }
    }

    // this scroll constraint only comes into effect when resizing:
    // when the window height is increased at the end of the tokens,
    // if there are available token above then pull the entire scroll down
    if (scroll_position + selection_rows > (int)tokens.size() && (int)tokens.size() >= selection_rows) {
      scroll_position = (int)tokens.size() - selection_rows;
    }

    apply_constraints();
  }

  void handle_confirmation_input() {
    if (args.tenacious) {
      if (output_is_tty) {
        // output is being queued up, not being sent right now
        // no need to stop ncurses from capturing output
      } else {
        // stop capturing the input to ncurses, but without refreshing the screen
        choose::nc::reset_prog_mode();
      }
    } else {
      choose::nc::endwin();
    }

    if (selections.size() == 0) {
      ++tenacious_single_select_indicator;
      selections.push_back(selection_position);
    }
    if (!args.selection_order) {
      std::sort(selections.begin(), selections.end());
    }

    for (const auto& s : selections) {
      const choose::Token& token = tokens[s];
      os.write_output(token);
    }
    os.finish_batch();

    if (args.tenacious) {
      selections.clear();
      if (!os.uses_buffer()) {
        if (fflush(args.output) == EOF) {
          throw std::runtime_error("output err");
        }
      }
    } else {
      os.finish_output();
    }
  }

  // returns true if the tui loop should continue, false if it should break
  bool handle_user_input() {
    int ch = getch();
    if (sigint_occurred != 0 || ch == KEY_BACKSPACE || ch == 'q' || ch == 27) {
      choose::nc::endwin();
      os.finish_output();
      return false;
    } else
#ifdef BUTTON5_PRESSED
        if (ch == KEY_MOUSE) {
      MEVENT e;
      if (getmouse(&e) != OK)
        return true;
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
      on_resize();
    } else if (ch == 'i' && args.multiple_selections) {
      std::sort(selections.begin(), selections.end());
      auto selections_position = selections.cbegin();
      decltype(selections) replacement;
      replacement.resize(tokens.size() - selections.size());
      auto replacement_insertion = replacement.begin();

      for (int i = 0; i < (int)tokens.size(); ++i) {
        selections_position = std::find(selections_position, selections.cend(), i);
        if (selections_position == selections.cend() || *selections_position != i) {
          *replacement_insertion++ = i;
        }
      }
      selections = std::move(replacement);
    } else if (ch == 'c') {  // && multiple_selections
      selections.clear();
    } else if (ch == ' ' && args.multiple_selections) {
      auto pos = std::find(selections.cbegin(), selections.cend(), selection_position);
      if ((pos == selections.cend())) {
        selections.push_back(selection_position);
      } else {
        selections.erase(pos);
      }
    } else if (ch == '\n' || ch == 'd' || ch == 'f') {
      handle_confirmation_input();
      return args.tenacious;
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
            selection_position = 0;
            return true;
          }
        } else {
          scroll_position += selection_rows;
          if (scroll_position > (int)tokens.size() - selection_rows) {
            if ((int)tokens.size() > selection_rows) {
              scroll_position = (int)tokens.size() - selection_rows;
            } else {
              scroll_position = 0;
            }
            selection_position = (int)tokens.size() - 1;
            return true;
          }
        }

        // special constraints. constrain the selection_position to the scroll
        selection_position = scroll_position + selection_rows / 2;
        // don't do the normal constraints
        return true;
      }
      apply_constraints();
    }
    return true;
  }

  void draw_tui() {
    werase(selection_window);
    int selection_text_space = selections.size() == 0 || !args.selection_order ? 0 : int(std::log10(selections.size())) + 1;

    for (int y = 0; y < selection_rows; ++y) {
      // =============================== draw line =============================

      int current_row = y + scroll_position;
      if (current_row >= 0 && current_row < (int)tokens.size()) {
        bool row_highlighted = current_row == selection_position;
        auto it = std::find(selections.cbegin(), selections.cend(), current_row);
        bool row_selected = it != selections.cend();

        if (args.selection_order && row_selected) {
          wattron(selection_window, A_DIM);
          mvwprintw(selection_window, y, 0, "%d", (int)(1 + it - selections.begin()));
          wattroff(selection_window, A_DIM);
        }

        bool line_is_highlighted = row_highlighted || row_selected;
        if (line_is_highlighted) {
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
        auto pos = &*tokens[y + scroll_position].buffer.cbegin();
        auto end = &*tokens[y + scroll_position].buffer.cend();

        // ============================ draw token =============================

        // if the token only contains chars which are not drawn visibly
        bool invisible_only = true;
        std::mbstate_t ps = std::mbstate_t();  // text decode state gets reset per token
        while (pos < end) {
          // a wchar_t string of length 1 for ncurses drawing
          // (only at [0] is set)
          wchar_t ch[2];
          ch[1] = L'\0';

          const char* escape_sequence = 0;  // draw non printing ascii via escape sequence
          bool char_is_invalid = false;     // decode error is represented by ?

          size_t num_bytes = std::mbrtowc(&ch[0], pos, end - pos, &ps);
          if (num_bytes == 0) {
            // null char was decoded. this is perfectly valid
            num_bytes = 1;  // keep going
          } else if (num_bytes == (size_t)-1) {
            // this sets errno, but we can keep going
            num_bytes = 1;
            char_is_invalid = true;
          } else if (num_bytes == (size_t)-2) {
            // the remaining bytes in the token do not complete a character
            num_bytes = end - pos;  // go to the end
            char_is_invalid = true;
          }

          pos += num_bytes;

          if (char_is_invalid) {
            escape_sequence = "?";
          } else {
            escape_sequence = choose::str::get_escape_sequence(ch[0]);
          }

          // the printing functions handle bound checking
          if (escape_sequence) {
            int len = strlen(escape_sequence);
            if (x + len <= num_columns) {  // check if drawing the char would wrap
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
            if (!std::iswspace(ch[0])) {
              invisible_only = false;
            }
          }
          // draw ... at the right side of the screen if the x exceeds the width for this line
          if (x > num_columns) {
            wattron(selection_window, A_DIM);
            mvwaddstr(selection_window, y, num_columns - 3, "...");
            wattroff(selection_window, A_DIM);
            break;  // cancel printing the rest of the token
          }
        }

        if (invisible_only) {
          const choose::Token& token = tokens[y + scroll_position];
          wattron(selection_window, A_DIM);
          mvwprintw(selection_window, y, INITIAL_X, "\\s{%d bytes}", (int)(token.buffer.end() - token.buffer.begin()));
          wattroff(selection_window, A_DIM);
        }

        if (line_is_highlighted) {
          wattroff(selection_window, A_BOLD);
          if (row_selected) {
            wattroff(selection_window, COLOR_PAIR(PAIR_SELECTED));
          }
        }
      }
    }

    wnoutrefresh(prompt_window);  // fine even if null
    wnoutrefresh(selection_window);
    doupdate();
  }

  void loop() {
    if (tokens.size() == 0) {
      mvprintw(0, 0, "No tokens.");
      int ch;
      do {
        ch = getch();  // wait for any exit or confirmation input
      } while (ch != '\n' && ch != 'd' && ch != 'f' && ch != KEY_BACKSPACE && ch != 'q' && ch != 27 && !sigint_occurred);
      choose::nc::endwin();
      return;
    }
    on_resize();
    while (true) {
      draw_tui();
      if (!handle_user_input()) {
        break;
      }
    }
  }
};

int main(int argc, char* const* argv) {
  setlocale(LC_ALL, "");  // for utf8
  choose::Arguments args = choose::handle_args(argc, argv);
  std::vector<choose::Token> tokens;
  try {
    tokens = choose::create_tokens(args);
  } catch (const choose::termination_request&) {
    exit(EXIT_SUCCESS);
  }

  if (signal(SIGINT, sigint_handler) == SIG_IGN) {
    signal(SIGINT, SIG_IGN);
  }

  UIState state{
      std::move(args),                    //
      std::move(tokens),                  //
      (bool)isatty(fileno(args.output)),  //
      choose::BatchOutputStream(          //
          state.args,                     //
          // queue up output and send it at the end if the output if to the shell
          state.args.tenacious && state.output_is_tty ? std::optional(std::vector<char>()) : std::nullopt  //
          ),
  };

  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  FILE* f = fopen("/dev/tty", "r+");
  if (!f) {
    perror(NULL);
    return EXIT_FAILURE;
  }

  try {
    SCREEN* screen = choose::nc::newterm(NULL, f, f);
    set_term(screen);
    keypad(stdscr, true);
    choose::nc::cbreak();  // pass keys directly from input without buffering
    choose::nc::noecho();
    curs_set(0);  // invisible cursor

    // as opposed to: nodelay(stdscr, false) // make getch block
    // a very large timeout still allows sigint to be effective immediately
    wtimeout(stdscr, std::numeric_limits<int>::max());
    // get mouse events right away
    mouseinterval(0);

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
    use_default_colors();
    init_pair(UIState::PAIR_SELECTED, COLOR_GREEN, -1);

    state.scroll_position = 0;
    state.selection_position = state.args.end ? (int)state.tokens.size() - 1 : 0;
    state.tenacious_single_select_indicator = 0;

    state.loop();
  } catch (...) {
  // a note on ncurses:
  //  - endwin() must be called before program exit. it must not be called twice
  //    or else the terminal prompt gets put at the bottom)
  //  - either endwin() or reset_prog_mode() must be called before putting to
  //    stdout or stderr (or else it goes to the ncurses screen instead)
  if (!isendwin()) {
      endwin();
    }
    throw;
  }
  return sigint_occurred ? 128 + 2 : EXIT_SUCCESS;
}