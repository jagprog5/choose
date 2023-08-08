#pragma once

#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <vector>

namespace choose {

namespace str {

// gives a method for displaying non-printing ascii characters in the interface
// returns null if there isn't a sequence
template <typename charT>
const char* get_escape_sequence(charT ch) {
  // the escape sequence will first be this:
  // https://en.wikipedia.org/wiki/Escape_sequences_in_C#Table_of_escape_sequences
  // and if it doesn't exist there, then it takes the letters here:
  // https://flaviocopes.com/non-printable-ascii-characters/
  switch (ch) {
    case 0:
      return "\\0";
      break;
    case 1:
      return "SOH";
      break;
    case 2:
      return "STX";
      break;
    case 3:
      return "ETX";
      break;
    case 4:
      return "EOT";
      break;
    case 5:
      return "ENQ";
      break;
    case 6:
      return "ACK";
      break;
    case 7:
      return "\\a";
      break;
    case 8:
      return "\\b";
      break;
    case 9:
      return "\\t";
      break;
    case 10:
      return "\\n";
      break;
    case 11:
      return "\\v";
      break;
    case 12:
      return "\\f";
      break;
    case 13:
      return "\\r";
      break;
    case 14:
      return "SO";
      break;
    case 15:
      return "SI";
      break;
    case 16:
      return "DLE";
      break;
    case 17:
      return "DC1";
      break;
    case 18:
      return "DC2";
      break;
    case 19:
      return "DC3";
      break;
    case 20:
      return "DC4";
      break;
    case 21:
      return "NAK";
      break;
    case 22:
      return "SYN";
      break;
    case 23:
      return "ETB";
      break;
    case 24:
      return "CAN";
      break;
    case 25:
      return "EM";
      break;
    case 26:
      return "SUB";
      break;
    case 27:
      return "\\e";
      break;
    case 28:
      return "FS";
      break;
    case 29:
      return "GS";
      break;
    case 30:
      return "RS";
      break;
    case 31:
      return "US";
      break;
    default:
      return NULL;
      break;
  }
}

template <typename T>
void append_to_buffer(std::vector<T>& buf, const T* begin, const T* end) {
  buf.resize(buf.size() + (end - begin));
  std::copy(begin, end, &*buf.end() - (end - begin));
}

template <typename T>
void append_to_buffer(std::vector<T>& buf, const std::vector<T>& from) {
  append_to_buffer(buf, &*from.cbegin(), &*from.cend());
}

// applies word wrapping on a string
// convert the prompt to a vector of wide char null terminating strings
std::vector<std::vector<wchar_t>> create_prompt_lines(const char* prompt, int num_columns) {
  std::vector<std::vector<wchar_t>> ret;
  std::mbstate_t ps = std::mbstate_t();

  const char* prompt_terminator = prompt;
  // prompt_terminator points to the position of the null terminator in the prompt
  // it is needed for the "n" arg in mbrtowc
  while (*prompt_terminator != 0) {
    ++prompt_terminator;
  }
  ret.emplace_back();

  const char* pos = prompt;
  const int INITIAL_AVAILABLE_WIDTH = num_columns;
  int available_width = INITIAL_AVAILABLE_WIDTH;
  while (pos < prompt_terminator) {
    wchar_t ch; // NOLINT initialized by get_ch()

    // read from the prompt to produce a wide char, placed in ch. returns false on decode err
    auto get_ch = [&]() -> bool {
      size_t num_bytes = std::mbrtowc(&ch, pos, prompt_terminator - pos, &ps);
      if (num_bytes == 0 || num_bytes == (size_t)-1 || num_bytes == (size_t)-2) {
        return false;
      }
      pos += num_bytes;
      return true;
    };

    auto remove_trailing_invisible = [](std::vector<wchar_t>& str) {
      while (!str.empty() && std::iswspace(*str.rbegin())) {
        str.pop_back();
      }
    };

    if (!get_ch()) {
      throw std::runtime_error("decode err");
    }

    if (ch == '\n') {
      remove_trailing_invisible(*ret.rbegin());
      available_width = INITIAL_AVAILABLE_WIDTH;
      ret.rbegin()->push_back(L'\0');
      ret.emplace_back();
    } else {
      int ch_width = wcwidth(ch);
      if (ch_width <= 0) {
        continue; // do not use 0 width or error width characters (EOF, etc)
      }
      available_width -= ch_width;

      // there is not enough space to insert ch into this line
      // not empty check so a single character should never wrap,
      // also precondition required for previous_character_visible
      if (available_width < 0 && !ret.rbegin()->empty()) {
        available_width = INITIAL_AVAILABLE_WIDTH - ch_width;
        bool next_character_visible = !std::iswspace(ch);
        bool previous_character_visible = !std::iswspace(*ret.rbegin()->rbegin());
        bool wrap_separates_word = next_character_visible && previous_character_visible;

        // remove the leading white space by reading and dropping the input
        // if there was nothing except whitespace then abort the wrap
        while (std::iswspace(ch)) {
          if (pos >= prompt_terminator) {
            remove_trailing_invisible(*ret.rbegin());
            goto get_out;
          }
          if (!get_ch()) {
            throw std::runtime_error("decode err");
          }
        }

        // if the line entirely contained whitespace then clear and reuse line
        bool has_visible = false;
        for (auto elem : *ret.rbegin()) {
          if (!std::iswspace(elem)) {
            has_visible = true;
            break;
          }
        }

        if (!has_visible) {
          ret.rbegin()->clear();
        } else {
          ret.emplace_back();
          auto& last_line = ret.rbegin()[1];
          remove_trailing_invisible(last_line);

          // if the line wrap occurred on a word boundary, then the last
          // characters from the latest line must be moved to the next line
          if (wrap_separates_word) {
            // find the last visible character in the string
            auto pos = last_line.end() - 1;
            while (pos >= last_line.begin() && !std::iswspace(*pos)) {
              --pos;
            }

            // if it is found (otherwise the entire row is a word without spaces)
            if (pos >= last_line.begin()) {
              ++pos;
              auto pos_cp = pos;
              while (pos_cp < last_line.end()) {
                available_width -= wcwidth(*pos_cp);
                ret.rbegin()->push_back(*pos_cp++);
              }
              last_line.resize(pos - last_line.begin());
            }
          }
          remove_trailing_invisible(last_line);
          last_line.push_back(L'\0');
        }
      }
      ret.rbegin()->push_back(ch);
    }
  }
get_out:
  ret.rbegin()->push_back(L'\0');

  return ret;
}

void write_f(FILE* f, const char* begin, const char* end) {
  size_t size = end - begin;
  if (fwrite(begin, sizeof(char), size, f) != size) {
    throw std::runtime_error("output err");
  }
}

void write_f(FILE* f, const std::vector<char>& v) {
  write_f(f, &*v.cbegin(), &*v.cend());
}

void flush_f(FILE* f) {
  if (fflush(f) == EOF) {
    throw std::runtime_error("output err");
  }
}

struct QueuedOutput {
  // if the output from the tui interface is going to that same terminal that
  // the interface is running in then it interferes. this is only an issue if
  // tenacious is enabled, since otherwise the program exits on any output
  // selected. in this case, queue up the output, and sends it on exit.
  std::optional<std::vector<char>> queued;

  void write_output(FILE* f, const char* begin, const char* end) {
    if (this->queued) {
      append_to_buffer(*this->queued, begin, end);
    } else {
      write_f(f, begin, end);
    }
  }

  void write_output(FILE* f, const std::vector<char>& v) {
    this->write_output(f, &*v.cbegin(), &*v.cend());
  }

  void flush_output(FILE* f) {
    if (this->queued) {
      write_f(f, *this->queued);
      this->queued->clear();
    }
  }
};

size_t get_bytes(FILE* f, size_t n, char* out) {
  size_t read_ret = fread(out, sizeof(char), n, f);
  if (read_ret == 0) {
    if (feof(f)) {
      return read_ret;
    } else if (ferror(f)) {
      const char* err_string = strerror(errno);
      throw std::runtime_error(err_string);
    }
  }
  return read_ret;
}

size_t get_bytes_unbuffered(int fileno, size_t n, char* out) {
  ssize_t read_ret = read(fileno, out, n);
  if (read_ret == -1) {
    const char* err_string = strerror(errno);
    throw std::runtime_error(err_string);
  }
  return (size_t)read_ret;
}

namespace utf8 {

static constexpr int MAX_BYTES_PER_CHARACTER = 4;

// c is the first byte of a utf8 multibyte sequence. returns the length of the multibyte
// returns -1 on error. e.g. this is a continuation byte
int length(unsigned char c) {
  if (c < 0b10000000) {
    return 1;
  } else if ((c & 0b11100000) == 0b11000000) {
    return 2;
  } else if ((c & 0b11110000) == 0b11100000) {
    return 3;
  } else if ((c & 0b11111000) == 0b11110000) {
    return 4;
  } else {
    return -1;
  }
}

bool is_continuation(unsigned char c) {
  return (c & 0b11000000) == 0b10000000;
}

// returns NULL on error
const char* last_character_start(const char* begin, const char* end) {
  // find the first non continuation byte in the string
  const char* pos = end - 1;
  // the limit is of concern here for when invalid utf is enabled.
  int left = MAX_BYTES_PER_CHARACTER;
  while (1) {
    if (pos < begin) {
      return NULL;
    }

    if (!is_continuation(*pos)) {
      return pos;
    }

    if (--left == 0) {
      return NULL;
    }
    --pos;
  }
}

// returns NULL on error
const char* last_completed_character_end(const char* begin, const char* end) {
  const char* pos = last_character_start(begin, end);
  if (pos == NULL) {
    return NULL;
  }
  int len = length(*pos);
  if (len == -1 || pos + len > end) {
    return pos;
  }
  return pos + len;
}

// pos is in range [begin,end].
// it is assumed that end is a character start -> if pos is end, then end is returned.
// if an error occurs, then pos is returned
const char* decrement_until_character_start(const char* pos, const char* begin, const char* end) {
  if (pos == end) {
    return pos;
  }

  const char* ret = last_character_start(begin, pos + 1);
  if (ret == NULL) {
    return pos;
  }
  return ret;
}

} // namespace utf8

} // namespace str

} // namespace choose
