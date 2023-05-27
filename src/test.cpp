#define BOOST_TEST_MODULE choose_test_module
#include <boost/test/unit_test.hpp>
#include "args.hpp"
#include "ncurses_wrapper.hpp"
#include "token.hpp"

using namespace choose;

char continuation = (char)0b10000000;
char one = (char)0b00000000;
char two = (char)0b11000000;
char three = (char)0b11100000;
char four = (char)0b11110000;

struct GlobalInit {
  GlobalInit() { setlocale(LC_ALL, ""); }
};

BOOST_GLOBAL_FIXTURE(GlobalInit);

BOOST_AUTO_TEST_SUITE(create_prompt_lines_test_suite)

BOOST_AUTO_TEST_CASE(empty_prompt) {
  auto ret = str::create_prompt_lines("", 80);
  BOOST_REQUIRE_EQUAL(ret.size(), 1);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"") == 0);
}

BOOST_AUTO_TEST_CASE(zero_width) {
  // zero or negative width is handled correctly
  auto ret = str::create_prompt_lines("abc", -1000);
  BOOST_REQUIRE_EQUAL(ret.size(), 3);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
}

BOOST_AUTO_TEST_CASE(only_whitespace) {
  auto ret = str::create_prompt_lines("         ", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 1);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"") == 0);
}

BOOST_AUTO_TEST_CASE(small_width) {
  auto ret = str::create_prompt_lines("abcd", 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 4);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[3].data(), L"d") == 0);
}

BOOST_AUTO_TEST_CASE(excess_spaces) {
  auto ret = str::create_prompt_lines("    ab   cd  ", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 2);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"ab") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"cd") == 0);
}

BOOST_AUTO_TEST_CASE(full_empty_one) {
  auto ret = str::create_prompt_lines("     a b c    ", 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 3);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
}

BOOST_AUTO_TEST_CASE(full_empty_many) {
  auto ret = str::create_prompt_lines("    111 222  333   444    555", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 5);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"111") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"222") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"333") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[3].data(), L"444") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[4].data(), L"555") == 0);
}

BOOST_AUTO_TEST_CASE(newlines) {
  auto ret = str::create_prompt_lines("this\nis\n\na\ntest", 80);
  BOOST_REQUIRE_EQUAL(ret.size(), 5);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"this") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"is") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[3].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[4].data(), L"test") == 0);
}

BOOST_AUTO_TEST_CASE(zero_width_char_ignored) {
  const char ch[] = {'h', (char)0xEF, (char)0xBB, (char)0xBF, 'i', 0};
  auto ret = str::create_prompt_lines(ch, 80);
  BOOST_REQUIRE_EQUAL(ret.size(), 1);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"hi") == 0);
}

BOOST_AUTO_TEST_CASE(word_boundary_with_leading_spaces) {
  auto ret = str::create_prompt_lines("  word", 4);
  BOOST_REQUIRE_EQUAL(ret.size(), 2);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"word") == 0);
}

BOOST_AUTO_TEST_CASE(invalid_utf8) {
  const char ch[] = {(char)0b11100000, '\0'};
  BOOST_CHECK_THROW(str::create_prompt_lines(ch, 80), std::runtime_error);

  // a second place this happens, when consuming leading whitespace during wrap
  const char ch2[] = {'t', ' ', (char)0b11100000, '\0'};
  BOOST_CHECK_THROW(str::create_prompt_lines(ch2, 1), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(wide_utf8) {
  // this checks for a special case in which a single character would wrap
  // https://www.compart.com/en/unicode/U+6F22
  // 0xE6 0xBC 0xA2 encodes for a character which has a display width of 2
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, ' ', (char)0xE6, (char)0xBC, (char)0xA2, '\0'};
  auto ret = str::create_prompt_lines(ch, 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 2);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"漢") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"漢") == 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(other_string_utils_test_suite)

BOOST_AUTO_TEST_CASE(test_last_character_start) {
  const char empty[] = {};
  BOOST_REQUIRE(str::utf8::last_character_start(empty, empty) == 0);
  const char simple[] = {'a'};
  BOOST_REQUIRE(str::utf8::last_character_start(simple, std::end(simple)) == simple);
  const char limit[] = {'a', continuation, continuation, continuation, continuation};
  BOOST_REQUIRE(str::utf8::last_character_start(limit, std::end(limit)) == 0);
  const char not_limit[] = {'a', continuation, continuation, continuation};
  BOOST_REQUIRE(str::utf8::last_character_start(not_limit, std::end(not_limit)) == not_limit);
  const char more[] = {'a', 'b', 'c', continuation, continuation, continuation};
  BOOST_REQUIRE(str::utf8::last_character_start(more, std::end(more)) == more + 2);
}

BOOST_AUTO_TEST_CASE(test_decrement_until_character_start) {
  const char none[] = {continuation, continuation};
  {
    const char* pos = &*std::crbegin(none);
    bool r = str::utf8::decrement_until_character_start(pos, none, &*std::cend(none)) == pos;
    BOOST_REQUIRE(r);
  }
  {
    const char* end = &*std::cend(none);
    bool r = str::utf8::decrement_until_character_start(end, none, &*std::cend(none)) == end;
    BOOST_REQUIRE(r);
  }
  const char vals[] = {continuation, one, continuation};
  const char* on_it = vals + 1;
  BOOST_REQUIRE_EQUAL(str::utf8::decrement_until_character_start(on_it, vals, &*std::cend(vals)), on_it);
  const char* off_it = vals + 2;
  BOOST_REQUIRE_EQUAL(str::utf8::decrement_until_character_start(off_it, vals, &*std::cend(vals)), on_it);
}

BOOST_AUTO_TEST_CASE(test_end_of_last_complete_character) {
  const char none[] = {continuation, continuation};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(none, std::end(none)) == NULL);

  const char complete[] = {three, continuation, continuation};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(complete, std::end(complete)) == std::end(complete));

  const char single[] = {one};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(single, std::end(single)) == std::end(single));

  const char err[] = {(char)0xFF, continuation};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(err, std::end(err)) == err);

  const char incomplete[] = {three, continuation};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(incomplete, std::end(incomplete)) == incomplete);

  // adding a continuation byte does not decrease the effective size of the string
  const char gotchya[] = {(char)0b11000000, (char)0b10000000, (char)0b10000000};
  BOOST_REQUIRE(str::utf8::last_completed_character_end(gotchya, std::end(gotchya)) == std::end(gotchya) - 1);
}

BOOST_AUTO_TEST_CASE(apply_index_op_before) {
  std::vector<char> empty;
  str::apply_index_op(empty, 123, true);
  BOOST_REQUIRE((empty == std::vector<char>{'1', '2', '3', ' '}));

  std::vector<char> val_zero;
  str::apply_index_op(val_zero, 0, true); // log edge case
  BOOST_REQUIRE((val_zero == std::vector<char>{'0', ' '}));

  std::vector<char> not_empty{'a', 'b', 'c'};
  str::apply_index_op(not_empty, 123, true);
  BOOST_REQUIRE((not_empty == std::vector<char>{'1', '2', '3', ' ', 'a', 'b', 'c'}));
}

BOOST_AUTO_TEST_CASE(apply_index_op_after) {
  std::vector<char> empty;
  str::apply_index_op(empty, 123, false);
  BOOST_REQUIRE((empty == std::vector<char>{' ', '1', '2', '3'}));

  std::vector<char> less_than_10;
  str::apply_index_op(less_than_10, 9, false);
  BOOST_REQUIRE((less_than_10 == std::vector<char>{' ', '9'}));

  std::vector<char> not_empty{'a', 'b', 'c'};
  str::apply_index_op(not_empty, 123, false);
  BOOST_REQUIRE((not_empty == std::vector<char>{'a', 'b', 'c', ' ', '1', '2', '3'}));
}

BOOST_AUTO_TEST_SUITE_END()

// choose either sends to stdout, or creates an interface that displays tokens
struct choose_output {
  std::variant<std::vector<char>, std::vector<choose::Token>> o;

  bool operator==(const choose_output& other) const {
    if (o.index() != other.o.index()) {
      return false;
    }
    if (const std::vector<char>* first_intermediate = std::get_if<std::vector<char>>(&o)) {
      const std::vector<char>& first = *first_intermediate;
      const std::vector<char>& second = std::get<std::vector<char>>(other.o);
      return first == second;
    } else {
      const std::vector<choose::Token>& first = std::get<std::vector<choose::Token>>(o);
      const std::vector<choose::Token>& second = std::get<std::vector<choose::Token>>(other.o);
      return first == second;
    }
  }

  friend std::ostream& operator<<(std::ostream&, const choose_output&);
};

std::ostream& operator<<(std::ostream& os, const choose_output& out) {
  if (const std::vector<char>* out_str = std::get_if<std::vector<char>>(&out.o)) {
    os << "\nstdout: ";
    bool first = true;
    for (char ch : *out_str) {
      if (!first) {
        os << ',';
      }
      first = false;
      const char* escape_sequence = str::get_escape_sequence(ch);
      if (escape_sequence) {
        os << escape_sequence;
      } else {
        os << ch;
      }
    }
  } else {
    const std::vector<choose::Token>& out_tokens = std::get<std::vector<choose::Token>>(out.o);
    os << "\ntokens: ";
    bool first_token = true;
    for (const Token& t : out_tokens) {
      if (!first_token) {
        os << '|';
        os << '|';
      }
      first_token = false;
      bool first_in_token = true;
      for (char ch : t.buffer) {
        if (!first_in_token) {
          os << ',';
        }
        first_in_token = false;
        const char* escape_sequence = str::get_escape_sequence(ch);
        if (escape_sequence) {
          os << escape_sequence;
        } else {
          os << ch;
        }
      }
    }
  }
  return os;
}

std::vector<char> to_vec(const char* s) {
  return {s, s + strlen(s)};
}

extern int optreset;

struct str_destroyer {
  void operator()(char* s) { free(s); } // NOLINT
};

// runs choose with the given stdin and arguments
choose_output run_choose(const std::vector<char>& input, const std::vector<const char*>& argv) {
  // resetting getopt global state
  // https://github.com/dnsdb/dnsdbq/commit/efa68c0499c3b5b4a1238318345e5e466a7fd99f
#ifdef linux
  optind = 0;
#else
  optind = 1;
  optreset = 1;
#endif

  int input_pipe[2];
  pipe(input_pipe);
  int output_pipe[2];
  pipe(output_pipe);
  auto input_writer = choose::file(fdopen(input_pipe[1], "w"));
  auto input_reader = choose::file(fdopen(input_pipe[0], "r"));
  auto output_writer = choose::file(fdopen(output_pipe[1], "w"));
  auto output_reader = choose::file(fdopen(output_pipe[0], "r"));

  using duped_str = std::unique_ptr<char, str_destroyer>;
  std::vector<duped_str> duped_args;
  duped_args.push_back(duped_str(strdup("/tester/path/to/choose")));
  for (const char* p : argv) {
    // getopt might?? modify argv, making a copy just in case.
    duped_args.push_back(duped_str(strdup(p)));
  }

  // duped_args owns a copy of the arguments. argv_non_const just views them
  // this copy is done since get_opt might? modify the args. the arg is non const
  std::vector<char*> argv_non_const;
  argv_non_const.resize(duped_args.size());
  typename std::vector<duped_str>::const_iterator from_pos = duped_args.cbegin();
  typename std::vector<char*>::iterator to_pos = argv_non_const.begin();
  while (from_pos != duped_args.end()) {
    *to_pos++ = from_pos++->get();
  }

  str::write_f(input_writer.get(), input);
  input_writer.reset();

  auto args = choose::handle_args((int)argv_non_const.size(), argv_non_const.data(), input_reader.get(), output_writer.get());
  choose_output ret;
  try {
    ret.o = choose::create_tokens(args);
    output_writer.reset();
  } catch (const choose::termination_request&) {
    output_writer.reset();
    std::vector<char> out;
    static constexpr size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    size_t bytes_read; // NOLINT
    do {
      bytes_read = fread(buf, sizeof(char), BUF_SIZE, output_reader.get());
      str::append_to_buffer(out, buf, std::begin(buf) + bytes_read);
    } while (bytes_read == BUF_SIZE);
    ret.o = std::move(out);
  }

  return ret;
}

choose_output run_choose(const char* null_terminating_input, const std::vector<const char*>& argv) {
  return run_choose(to_vec(null_terminating_input), argv);
}

BOOST_AUTO_TEST_SUITE(create_tokens_test_suite)

BOOST_AUTO_TEST_CASE(simple) {
  choose_output out = run_choose("a\nb\nc", {});
  choose_output correct_output{std::vector<choose::Token>{"a", "b", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(simple_basic_output) {
  // see args.is_basic(); also tests separators
  choose_output out = run_choose("first\nsecond\nthird", {"--output-separator", " ", "--batch-separator=\n", "-t"});
  choose_output correct_output{to_vec("first second third\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(basic_output_rm_filter) {
  choose_output out = run_choose("first\nsecond\nthird", {"--in=2", "-t"});
  choose_output correct_output{to_vec("first\nsecond\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(basic_output_in_limit) {
  choose_output out = run_choose("first\nsecond\nthird\nfourth", {"--rm=second", "--filter=first", "-t"});
  choose_output correct_output{to_vec("first\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(zero_in_limit) {
  choose_output out = run_choose("anything", {"--in=0", "-t"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(zero_in_out_limit) {
  choose_output out = run_choose("anything", {"--in=0", "--out=0"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(basic_output_accumulation) {
  // basic output avoids a copy when it can, but it still accumulates the input on no/partial separator match.
  // this is needed because an entire token needs to be accumulated before a filter can be applied
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--read=1", "-f", "s", "-t"});
  choose_output correct_output{to_vec("first\nsecond\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(basic_output_match) {
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--read=1", "--match", "-t"});
  choose_output correct_output{to_vec("aaa\naaa\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

// lexicographical sorting and uniqueness

BOOST_AUTO_TEST_CASE(sort) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort"});
  choose_output correct_output{std::vector<choose::Token>{"a", "is", "test", "this"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_reverse) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort-reverse"});
  choose_output correct_output{std::vector<choose::Token>{"this", "test", "is", "a"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--unique"});
  choose_output correct_output{std::vector<choose::Token>{"this", "is", "a", "test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_reverse_and_unique) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--unique", "--sort-reverse"});
  choose_output correct_output{std::vector<choose::Token>{"this", "test", "is", "a"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

// user defined sorting and uniqueness

BOOST_AUTO_TEST_CASE(defined_sort) {
  // this also checks the separator and sort stability
  choose_output out = run_choose("John Doe\nApple\nJohn Doe\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--comp-sort"});
  choose_output correct_output{std::vector<choose::Token>{"John Doe", "John Doe", "John Smith", "Apple", "Banana"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(defined_sort_reverse) {
  // notice that this isn't just the reverse of defined_sort test, since the sort is stable
  choose_output out = run_choose("John Doe\nApple\nJohn Doe\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--comp-sort", "--sort-reverse"});
  choose_output correct_output{std::vector<choose::Token>{"Apple", "Banana", "John Doe", "John Doe", "John Smith"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(defined_unique) {
  // the comparison treats all John's as the same. so there's one John and one non John in the output.
  choose_output out = run_choose("John Doe\nApple\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--comp-unique"});
  choose_output correct_output{std::vector<choose::Token>{"John Doe", "Apple"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(defined_sort_reverse_and_unique) {
  choose_output out = run_choose("John Doe\nApple\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--comp-sort", "--sort-reverse", "--comp-unique"});
  choose_output correct_output{std::vector<choose::Token>{"Apple", "John Doe"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(comp_z) {
  choose_output out = run_choose("John Doe\nApple\nJohn Doe\nBanana\nJohn Smith", {"-r", "--comp-z", "^John[ a-zA-Z]*\\0(?!John)", "--comp-sort"});
  choose_output correct_output{std::vector<choose::Token>{"John Doe", "John Doe", "John Smith", "Apple", "Banana"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

// mix of both lex and user defined

BOOST_AUTO_TEST_CASE(lex_unique_defined_sort) {
  choose_output out = run_choose("John Doe\nApple\nJohn Doe\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--unique", "--comp-sort"});
  choose_output correct_output{std::vector<choose::Token>{"John Doe", "John Smith", "Apple", "Banana"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(defined_unique_lex_sort) {
  choose_output out = run_choose("John Doe\nApple\nJohn Doe\nBanana\nJohn Smith", {"-r", "--comp", "---", "^John[ a-zA-Z]*---(?!John)", "--comp-unique", "--sort"});
  choose_output correct_output{std::vector<choose::Token>{"Apple", "John Doe"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

// ========================

BOOST_AUTO_TEST_CASE(no_delimit) {
  choose_output out = run_choose("a\nb\nc", {"--no-delimit", "-t"});
  choose_output correct_output{to_vec("a\nb\nc")}; // no newline at end
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(delimit_on_empty) {
  choose_output out = run_choose("", {"--delimit-on-empty", "-t"});
  choose_output correct_output{to_vec("\n")}; // newline at end
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(no_delimit_delimit_on_empty) {
  // checking precedence
  choose_output out = run_choose("", {"--no-delimit", "--delimit-on-empty", "-t"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(in_limit) {
  choose_output out = run_choose("d\nc\nb\na", {"--in=3", "--sort"});
  choose_output correct_output{std::vector<choose::Token>{"b", "c", "d"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(flip) {
  choose_output out = run_choose("a\nb\nc\nd", {"--flip", "-t"});
  choose_output correct_output{to_vec("d\nc\nb\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(direct_but_not_basic_limit) {
  choose_output out = run_choose("a\nb\nc", {"--sub", "c", "d", "-t=2"});
  choose_output correct_output{to_vec("a\nb\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(ordered_ops) {
  choose_output out = run_choose("this\nis\nrra\ntest", {"-r", "--sub", "is", "rr", "--rm", "test", "--filter", "rr$"});
  choose_output correct_output{std::vector<choose::Token>{"thrr", "rr"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(literal_sub) {
  choose_output out = run_choose("literal substitution", {"--sub", "literal", "good"});
  choose_output correct_output{std::vector<choose::Token>{"good substitution"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(index_ops) {
  choose_output out = run_choose("every\nother\nword\nis\nremoved\n5\n6\n7\n8\n9\n10", {"-r", "--in-index=after", "-f", "[02468]$", "--sub", "(.*) [0-9]+", "$1", "--out-index"});
  choose_output correct_output{std::vector<choose::Token>{"0 every", "1 word", "2 removed", "3 6", "4 8", "5 10"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_empty_input) {
  choose_output out = run_choose("", {"-t"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_match_with_groups) {
  choose_output out = run_choose("abcde", {"-r", "--read=1", "--match", "b(c)(d)"});
  choose_output correct_output{std::vector<choose::Token>{"bcd", "c", "d"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_match_with_groups_limit) {
  choose_output out = run_choose("abcde", {"-r", "--read=1", "--match", "b(c)(d)", "--in=2"});
  choose_output correct_output{std::vector<choose::Token>{"bcd", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_no_match_lookbehind_retained) {
  // if there is no match, then the input is discarded but it keep enough for the lookbehind
  choose_output out = run_choose("aaabbbccc", {"--read=3", "-r", "--match", "(?<=aaa)bbb"});
  choose_output correct_output{std::vector<choose::Token>{"bbb"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_partial_match_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=4", "-r", "--match", "(?<=aaa)bbb"});
  choose_output correct_output{std::vector<choose::Token>{"bbb"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_no_separator_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=3", "-r", "(?<=aaa)bbb"});
  choose_output correct_output{std::vector<choose::Token>{"aaa", "ccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_partial_separator_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=4", "-r", "(?<=aaa)bbb"});
  choose_output correct_output{std::vector<choose::Token>{"aaa", "ccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(separator_no_match) {
  choose_output out = run_choose("aaabbbccc", {"zzzz", "--read=1"});
  choose_output correct_output{std::vector<choose::Token>{"aaabbbccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(empty_match_target) {
  // important since PCRE2_NOTEMPTY is used to prevent infinite loop; ensures progress
  choose_output out = run_choose("aaabbbccc", {"--match", ""});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(input_is_separator) {
  choose_output out = run_choose("\n", {});
  choose_output correct_output{std::vector<choose::Token>{""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(input_is_separator_use_delimit) {
  choose_output out = run_choose("\n", {"--use-delimiter"});
  choose_output correct_output{std::vector<choose::Token>{"", ""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_shrink_excess) {
  // creates a large subject but resizes internal buffer to remove the bytes that weren't written to
  choose_output out = run_choose("12345", {"zzzz", "--read=10000"});
  choose_output correct_output{std::vector<choose::Token>{"12345"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(no_multiline) {
  choose_output out = run_choose("this\nis\na\ntest", {"-r", "--match", "^t"});
  choose_output correct_output{std::vector<choose::Token>{"t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(yes_multiline) {
  choose_output out = run_choose("this\nis\na\ntest", {"-r", "--multiline", "--match", "^t"});
  choose_output correct_output{std::vector<choose::Token>{"t", "t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(begin_of_string) {
  // I found it surprising that \\A gives a lookbehind of 1. given the
  // preexisting logic, I expected that the buffer would be removed up to but
  // not including the t, making it the beginning of the line again. but the
  // lookbehind of 1 allows characters to be retained, so it is correctly
  // recognized as not the beginning of the string.
  choose_output out = run_choose("uaaat", {"-r", "--match", "--read=1", "\\A[ut]"});
  choose_output correct_output{std::vector<choose::Token>{"u"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(begin_of_line) {
  choose_output out = run_choose("abcd\nefgh", {"-r", "--multiline", "--match", "^."});
  choose_output correct_output{std::vector<choose::Token>{"a", "e"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(end_of_string) {
  choose_output out = run_choose("uaaat", {"-r", "--match", "--read=6", "[ut]\\Z"});
  choose_output correct_output{std::vector<choose::Token>{"t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(end_of_line) {
  choose_output out = run_choose("abcd\nefgh", {"-r", "--multiline", ".$"});
  choose_output correct_output{std::vector<choose::Token>{"abc", "\nefg"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_less_than_read) {
  // read takes the minimum of the available space in buffer left and the read amount
  choose_output out = run_choose("aaa1234aaa", {"--match", "1234", "--read=1000000", "--retain-limit=3"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_match) {
  choose_output out = run_choose("aaa1234aaa", {"--match", "1234", "--read=1", "--retain-limit=3"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_match_enough) {
  choose_output out = run_choose("aaaa1234aaaa", {"--match", "1234", "--read=1", "--retain-limit=4"});
  choose_output correct_output{std::vector<choose::Token>{"1234"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_partial_match_enough) {
  choose_output out = run_choose("aaa1234aaaa1234aaaa", {"--match", "1234", "--read=4", "--retain-limit=4"});
  choose_output correct_output{std::vector<choose::Token>{"1234", "1234"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_separator) {
  choose_output out = run_choose("this1234test", {"1234", "--read=1", "--retain-limit=7"});
  // the retain limit came into effect part way through matching the separator
  choose_output correct_output{std::vector<choose::Token>{"4test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_separator_enough) {
  choose_output out = run_choose("this1234test", {"1234", "--read=1", "--retain-limit=8"});
  choose_output correct_output{std::vector<choose::Token>{"this", "test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(retain_limit_partial_separator_enough) {
  choose_output out = run_choose("this1234test", {"1234", "--read=6", "--retain-limit=8"});
  choose_output correct_output{std::vector<choose::Token>{"this", "test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(complete_utf8) {
  // checks that the last utf8 char is completed before sending it to pcre2
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 0};
  choose_output out = run_choose(ch, {"--read=1", "--utf"});
  choose_output correct_output{std::vector<choose::Token>{ch}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(utf8_lookback_separates_multibyte) {
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
  // lookbehind of 4 bytes, reading >=1 character at a time
  // the lookbehind must be correctly decremented to include the 0xE6 byte
  const char pattern[] = {'(', '?', '<', '=', (char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', ')', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--max-lookbehind=1", "--read=1", "--utf", "--match", pattern});
  choose_output correct_output{std::vector<choose::Token>{"st"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8) {
  const char ch[] = {(char)0xFF, (char)0b11000000, (char)0b10000000, (char)0b10000000, 't', 'e', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", "test"});
  choose_output correct_output{std::vector<choose::Token>{"test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8_separating_multibyte_not_ok) {
  // sanity check for this: https://github.com/PCRE2Project/pcre2/issues/239. an
  // incomplete byte does not give a partial match. so allow invalid still needs
  // the subject_effective_end logic
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", ch});
  choose_output correct_output{std::vector<choose::Token>{ch}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8_overlong_byte) {
  const char ch[] = {continuation, continuation, continuation, continuation, 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", "anything"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(ensure_completed_utf8_multibytes_gives_err) {
  const char ch[] = {(char)0b10000000, 0};
  BOOST_REQUIRE_THROW(run_choose(ch, {"--utf", "--read=1"}), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(misc_args)

BOOST_AUTO_TEST_CASE(null_output_separators) {
  choose_output out = run_choose("a\nb\nc", {"-zyt"});
  choose_output correct_output{std::vector<char>{'a', '\0', 'b', '\0', 'c', '\0'}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(null_input_separator) {
  choose_output out = run_choose(std::vector<char>{'a', '\0', 'b', '\0', 'c'}, {"-0"});
  choose_output correct_output{std::vector<choose::Token>{"a", "b", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(parse_ul) {
  int argc = 1;
  const char* const argv[] = {"/tester/path/to/parse_ul"};
  long out; // NOLINT
  bool arg_has_errors = false;
  choose::parse_ul("123", &out, 0, 1000, &arg_has_errors, "simple", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, false);
  BOOST_REQUIRE_EQUAL(out, 123);
  choose::parse_ul("banana", &out, 0, 1000, &arg_has_errors, "simple parse error", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, true);
  arg_has_errors = false;
  choose::parse_ul("-999999999999999999999999999999999999999999999999999999999999999999", &out, 0, 1000, &arg_has_errors, "-range parse err", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, true);
  arg_has_errors = false;
  choose::parse_ul("999999999999999999999999999999999999999999999999999999999999999999", &out, 0, 1000, &arg_has_errors, "+range parse err", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, true);
  arg_has_errors = false;
  choose::parse_ul("3", &out, 3, 1000, &arg_has_errors, "-range inclusive", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, false);
  BOOST_REQUIRE_EQUAL(out, 3);
  choose::parse_ul("1000", &out, 3, 1000, &arg_has_errors, "+range inclusive", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, false);
  BOOST_REQUIRE_EQUAL(out, 1000);
  choose::parse_ul("2", &out, 3, 1000, &arg_has_errors, "-range exclusive err", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, true);
  arg_has_errors = false;
  choose::parse_ul("1001", &out, 3, 1000, &arg_has_errors, "+range exclusive err", argc, argv, 0);
  BOOST_REQUIRE_EQUAL(arg_has_errors, true);
  arg_has_errors = false;
}

BOOST_AUTO_TEST_CASE(in_index_before) {
  choose_output out = run_choose("this\nis\na\ntest", {"--in-index=before"});
  choose_output correct_output{std::vector<choose::Token>{"0 this", "1 is", "2 a", "3 test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(misc_failures)

BOOST_AUTO_TEST_CASE(match_start_after_end) {
  // \K can be used to set a match beginning after its end. this is guarded for
  BOOST_REQUIRE_THROW(run_choose("This is a test string.", {"-r", "--match", "test(?=...\\K)"}), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(compilation_failure) {
  BOOST_REQUIRE_THROW(run_choose("", {"-r", "["}), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(match_failure) {
  // in this case some utf8 failure
  const char ch[] = {(char)0xFF, '\0'};
  BOOST_REQUIRE_THROW(run_choose(ch, {"--utf"}), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(sub_failure) {
  BOOST_REQUIRE_THROW(run_choose("test", {"-r", "--sub", "test", "${"}), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
