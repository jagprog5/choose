#pragma once

#include <ncursesw/curses.h>
#include <memory>
#include <stdexcept>

namespace choose {

struct file_destroyer {
  void operator()(FILE* f) {
    if (f) {
      fclose(f);
    }
  }
};

using file = std::unique_ptr<FILE, file_destroyer>;

namespace nc {
// choose mostly propagates errors through exceptions
// if a ncurses call fails, instead of checking for ERR, it just throws

struct window_destroyer {
  void operator()(WINDOW* w) {
    if (w) {
      delwin(w);
    }
  }
};

using window = std::unique_ptr<WINDOW, window_destroyer>;

struct screen_destroyer {
  void operator()(SCREEN* s) {
    if (s) {
      delscreen(s);
    }
  }
};

using screen = std::unique_ptr<SCREEN, screen_destroyer>;

void endwin() {
  if (::endwin() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

window newwin(int nlines, int ncols, int begin_y, int begin_x) {
  auto ret = window(::newwin(nlines, ncols, begin_y, begin_x));
  if (!ret) {
    throw std::runtime_error("ncurses error");
  }
  return ret;
}

void wresize(const window& w, int nlines, int ncols) {
  if (::wresize(w.get(), nlines, ncols) == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void mvwin(const window& w, int nlines, int ncols) {
  if (::mvwin(w.get(), nlines, ncols) == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void reset_prog_mode() {
  if (::reset_prog_mode() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

screen newterm(char* type, const file& outfd, const file& infd) {
  auto ret = screen(::newterm(type, outfd.get(), infd.get()));
  if (!ret) {
    throw std::runtime_error("ncurses error");
  }
  return ret;
}

void cbreak() {
  if (::cbreak() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void noecho() {
  if (::noecho() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void mouseinterval(int i) {
  if (::mouseinterval(i) == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

}  // namespace nc

}  // namespace choose