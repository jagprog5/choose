#pragma once

#include <ncursesw/curses.h>
#include <stdexcept>

namespace choose {

namespace nc {

// choose mostly propagates errors through exceptions
// if a ncurses call fails, instead of checking for ERR, it just throws

void endwin() {
  if (::endwin() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

WINDOW* newwin(int nlines, int ncols, int begin_y, int begin_x) {
  WINDOW* ret = ::newwin(nlines, ncols, begin_y, begin_x);
  if (!ret) {
    throw std::runtime_error("ncurses error");
  }
  return ret;
}

void wresize(WINDOW* w, int nlines, int ncols) {
  if (::wresize(w, nlines, ncols) == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void mvwin(WINDOW* w, int nlines, int ncols) {
  if (::mvwin(w, nlines, ncols) == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

void reset_prog_mode() {
  if (::reset_prog_mode() == ERR) {
    throw std::runtime_error("ncurses error");
  }
}

SCREEN* newterm(char* type, FILE* outfd, FILE* infd) {
  SCREEN* ret = ::newterm(type, outfd, infd);
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