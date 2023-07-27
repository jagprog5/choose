#include "tui/tui.hpp"

// NOLINTNEXTLINE exceptions are correctly handled
int main(int argc, char* const* argv) {
  choose::Arguments args = choose::Arguments::create_args(argc, argv);
  setlocale(LC_ALL, args.locale);
  std::vector<choose::pipeline::SimplePacket> tokens;
  try {
    tokens = args.create_packets();
  } catch (const choose::pipeline::output_finished&) {
    return EXIT_SUCCESS;
  }

  try {
    choose::tui::UIState(std::move(args), std::move(tokens));
  } catch (...) {
    // a note on ncurses:
    //  - endwin() must be called before program exit. it must not be called twice
    //    or else the terminal prompt gets put at the bottom)
    //  - either endwin() or reset_prog_mode() must be called before putting to
    //    stdout or stderr (or else it goes to the ncurses screen instead)
    if (!isendwin()) {
      endwin(); // safety measure to restore terminal
    }
    return EXIT_FAILURE;
  }
  return choose::tui::sigint_occurred ? 128 + 2 : EXIT_SUCCESS;
}
