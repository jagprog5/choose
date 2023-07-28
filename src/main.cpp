#include "tui/tui.hpp"
#include "pipeline/terminal.hpp"

// NOLINTNEXTLINE exceptions are correctly handled
int main(int argc, char* const* argv) {
  std::vector<choose::pipeline::SimplePacket> tokens;
  choose::Arguments args;
  try {
    choose::Arguments::populate_args(args, argc, argv);
    setlocale(LC_ALL, args.locale);
    args.create_packets();
  } catch (choose::pipeline::pipeline_complete& e) {
    tokens = std::move(e.packets);
    return EXIT_SUCCESS;
  } catch (const choose::pipeline::output_finished&) {
    return EXIT_SUCCESS;
  }

  // https://stackoverflow.com/a/44884859/15534181
  // required for ncurses to work after using stdin
  choose::file f = choose::file(fopen("/dev/tty", "r+"));
  if (!f) {
    perror(NULL);
    throw std::runtime_error("");
  }
  try {
    choose::nc::screen screen = choose::nc::newterm(NULL, f, f);
    set_term(screen.get());
    choose::nc::cbreak(); // pass keys directly from input without buffering
    choose::nc::noecho();
    curs_set(0); // invisible cursor

    // not handling ERR for color, attribute, or printing. in general
    start_color();
    use_default_colors();
    init_pair(choose::tui::UIState::PAIR_SELECTED, COLOR_GREEN, -1);
    choose::tui::UIState(std::ref(args), std::move(tokens));
  } catch (const std::exception& e) {
    // a note on ncurses:
    //  - endwin() must be called before program exit. it must not be called twice
    //    or else the terminal prompt gets put at the bottom)
    //  - either endwin() or reset_prog_mode() must be called before putting to
    //    stdout or stderr (or else it goes to the ncurses screen instead)
    if (!isendwin()) {
      endwin(); // safety measure to restore terminal
    }
    throw e;
  }

  return choose::tui::sigint_occurred ? 128 + 2 : EXIT_SUCCESS;
}
