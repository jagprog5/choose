#define OUTPUT_SIZE_BOUND_TESTING
// this ensure that through the lifetime of the program the output vector does
// not exceed a certain amount. this was needed since without this check, only
// the end result could be seen.
#include <optional>
std::optional<size_t> output_size_bound_testing;

#define BOOST_TEST_MODULE choose_test_module
#include <boost/test/unit_test.hpp>
#include "args.hpp"
#include "ncurses_wrapper.hpp"
#include "token.hpp"

/*
valgrind should give a clean bill of health:
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./unit_tests
*/

using namespace choose;

char continuation = (char)0b10000000;
char one = (char)0b00000000;
char two = (char)0b11000000;
char three = (char)0b11100000;
char four = (char)0b11110000;

struct GlobalInit {
  GlobalInit() {
    setlocale(LC_ALL, "");
    // if choose fails in run_choose
    signal(SIGPIPE, SIG_IGN);
  }
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

BOOST_AUTO_TEST_SUITE(numeric_compare_test_suite)

BOOST_AUTO_TEST_CASE(numeric_compare_test) {
  auto comp_str = [](const std::string& lhs, const std::string& rhs) -> bool { return numeric_compare(&*lhs.cbegin(), &*lhs.cend(), &*rhs.cbegin(), &*rhs.cend()); };
  BOOST_REQUIRE(!comp_str("2", "1"));
  BOOST_REQUIRE(comp_str("1", "2"));
  BOOST_REQUIRE(!comp_str(".", "."));
  BOOST_REQUIRE(!comp_str(".", ".00000000000"));
  BOOST_REQUIRE(!comp_str(".00000000001", "."));
  BOOST_REQUIRE(comp_str(".0", ".000123"));
  BOOST_REQUIRE(!comp_str("-.0", "-.000123"));
  BOOST_REQUIRE(comp_str("123.00000000", "123.001"));
  BOOST_REQUIRE(!comp_str(".1112", ".11111111"));
  BOOST_REQUIRE(comp_str("12.", "22."));
  BOOST_REQUIRE(comp_str("12.", "22"));
  BOOST_REQUIRE(comp_str("12", "22."));
  BOOST_REQUIRE(comp_str("22", "22.001"));

  BOOST_REQUIRE(!comp_str("", ""));
  BOOST_REQUIRE(!comp_str("-0.000", "") && !comp_str("", "-0.000"));
  BOOST_REQUIRE(!comp_str(",,.123", "0.123") && !comp_str("0.123", ",,.123"));
  BOOST_REQUIRE(!comp_str("1,,,,23", "123") && !comp_str("123", "1,,,,23"));
  BOOST_REQUIRE(comp_str("123", "234"));
  BOOST_REQUIRE(comp_str("012", "123"));
  BOOST_REQUIRE(!comp_str("123", "012"));
  BOOST_REQUIRE(comp_str(",,,0,,,1,,,2", "123"));
  BOOST_REQUIRE(comp_str("102", "103"));
  BOOST_REQUIRE(comp_str("99", "111"));
  BOOST_REQUIRE(!comp_str("-99", "-111"));

  BOOST_REQUIRE(!comp_str("", "-"));
  BOOST_REQUIRE(comp_str("-9,,,,9", "99"));

  BOOST_REQUIRE(!comp_str("1\xAE", "1"));
  BOOST_REQUIRE(!comp_str("1", "1\xAE"));
}

BOOST_AUTO_TEST_CASE(numeric_hash_test) {
  auto h = [](const std::string& s) -> size_t { return numeric_hash(&*s.cbegin(), &*s.cend()); };
  BOOST_REQUIRE_EQUAL(h("-00,.0000"), h("0.0"));
  BOOST_REQUIRE_EQUAL(h("-."), h("00000"));
  BOOST_REQUIRE_EQUAL(h("123"), h("00001,,,2,,,3"));
  BOOST_REQUIRE_NE(h("+123"), h("-123"));
  BOOST_REQUIRE_EQUAL(h("123"), h("123."));
  BOOST_REQUIRE_EQUAL(h("123"), h("123.00000"));
  BOOST_REQUIRE_NE(h("123"), h("123.000001"));
  BOOST_REQUIRE_NE(h("123.456"), h("123456"));
  BOOST_REQUIRE_EQUAL(h("123.456"), h("123.456000000"));

  BOOST_REQUIRE_EQUAL(h("1"), h("1\xAE"));
}

BOOST_AUTO_TEST_CASE(numeric_equal_test) {
  auto equal_str = [](const std::string& lhs, const std::string& rhs) -> bool { return numeric_equal(&*lhs.cbegin(), &*lhs.cend(), &*rhs.cbegin(), &*rhs.cend()); };
  BOOST_REQUIRE(!equal_str("-1", "0"));
  BOOST_REQUIRE(!equal_str(".001", ".00"));
  BOOST_REQUIRE(equal_str(".", ".0000"));
  BOOST_REQUIRE(equal_str(".", ""));
  BOOST_REQUIRE(!equal_str(".", ".0001"));
  BOOST_REQUIRE(!equal_str(".00", ".0001"));
  BOOST_REQUIRE(equal_str("123", "123"));
  BOOST_REQUIRE(!equal_str("123.001", "123"));
  BOOST_REQUIRE(!equal_str("123", "123.001"));
  BOOST_REQUIRE(equal_str("123", "123.000"));
  BOOST_REQUIRE(equal_str("123.000", "123"));
  BOOST_REQUIRE(!equal_str("123", "1234"));
  BOOST_REQUIRE(!equal_str("1234", "123"));
  BOOST_REQUIRE(!equal_str("123.", "1234"));
  BOOST_REQUIRE(!equal_str("1234", "123."));
  BOOST_REQUIRE(!equal_str(".001", ".002"));
  BOOST_REQUIRE(equal_str(".0000000", "."));
  BOOST_REQUIRE(equal_str(".0000000", "0.000000"));
  BOOST_REQUIRE(equal_str("123.000", "123"));
  BOOST_REQUIRE(!equal_str("345", "123"));

  BOOST_REQUIRE(equal_str("1", "1\xAE"));
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

BOOST_AUTO_TEST_CASE(apply_index_op) {
  auto op = IndexOp(IndexOp::BEFORE);
  op.index = 123;
  std::vector<char> empty;
  op.apply(empty);
  BOOST_REQUIRE((empty == std::vector<char>{'1', '2', '3', ' '}));

  std::vector<char> val_zero;
  op.index = 0; // log edge case
  op.apply(val_zero);
  BOOST_REQUIRE((val_zero == std::vector<char>{'0', ' '}));

  op.index = 456;
  std::vector<char> not_empty{'a', 'b', 'c'};
  op.apply(not_empty);
  BOOST_REQUIRE((not_empty == std::vector<char>{'4', '5', '6', ' ', 'a', 'b', 'c'}));
}

BOOST_AUTO_TEST_CASE(apply_index_op_after) {
  auto op = IndexOp(IndexOp::AFTER);
  std::vector<char> empty;
  op.index = 123;
  op.apply(empty);
  BOOST_REQUIRE((empty == std::vector<char>{' ', '1', '2', '3'}));

  std::vector<char> less_than_10; // after logic edge case
  op.index = 9;
  op.apply(less_than_10);
  BOOST_REQUIRE((less_than_10 == std::vector<char>{' ', '9'}));

  std::vector<char> not_empty{'a', 'b', 'c'};
  op.index = 456;
  op.apply(not_empty);
  BOOST_REQUIRE((not_empty == std::vector<char>{'a', 'b', 'c', ' ', '4', '5', '6'}));
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
      return std::equal(first.begin(), first.end(), second.begin(), second.end(), [](const choose::Token& lhs, const choose::Token& rhs) -> bool { //
        return lhs.buffer == rhs.buffer;
      });
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
  (void)!pipe(input_pipe); // supress unused result
  int output_pipe[2];
  (void)!pipe(output_pipe);
  auto input_writer = choose::file(fdopen(input_pipe[1], "w"));
  auto input_reader = choose::file(fdopen(input_pipe[0], "r"));
  auto output_writer = choose::file(fdopen(output_pipe[1], "w"));
  auto output_reader = choose::file(fdopen(output_pipe[0], "r"));

  using duped_str = std::unique_ptr<char, str_destroyer>;
  std::vector<duped_str> duped_args;
  duped_args.push_back(duped_str(strdup(boost::unit_test::framework::current_test_case().full_name().c_str())));
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

struct OutputSizeBoundFixture {
  OutputSizeBoundFixture(size_t max) { output_size_bound_testing = max; }
  ~OutputSizeBoundFixture() { output_size_bound_testing = std::nullopt; }
};

BOOST_AUTO_TEST_SUITE(create_tokens_test_suite)

BOOST_AUTO_TEST_CASE(simple) {
  choose_output out = run_choose("a\na\nb\nc", {"-t"});
  choose_output correct_output{std::vector<choose::Token>{"a", "a", "b", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(delimiters) {
  OutputSizeBoundFixture o(0);
  choose_output out = run_choose("first\nsecond\nthird", {"--output-delimiter", " ", "--batch-delimiter=\n"});
  choose_output correct_output{to_vec("first second third\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(output_in_limit) {
  choose_output out = run_choose("first\nsecond\nthird", {"--head=2"});
  choose_output correct_output{to_vec("first\nsecond\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(default_head) {
  choose_output out = run_choose("0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10", {"--head"});
  choose_output correct_output{to_vec("0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(head_min_limit) {
  choose_output out = run_choose("0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10", {"--head=3,5"});
  choose_output correct_output{to_vec("3\n4\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(output_rm_filter) {
  choose_output out = run_choose("first\nsecond\nthird\nfourth", {"--rm=second", "--filter=first"});
  choose_output correct_output{to_vec("first\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(zero_with_tui) {
  choose_output out = run_choose("anything", {"--out=0", "-t"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(zero_no_tui) {
  choose_output out = run_choose("anything", {"--out=0"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(output_accumulation) {
  // the output avoids a copy when it can, but it still accumulates the input on no/partial delimiter match.
  // this is needed because an entire token needs to be accumulated before a filter can be applied
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--read=1", "-f", "s"});
  choose_output correct_output{to_vec("first\nsecond\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(output_match) {
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--read=1", "--match"});
  choose_output correct_output{to_vec("aaa\naaa\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort"});
  choose_output correct_output{to_vec("a\nis\ntest\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--unique", "--load-factor=1"});
  choose_output correct_output{to_vec("this\nis\na\ntest\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_unique) {
  choose_output out = run_choose("-0\n0\n0\n.0\n.\n\n1\n1.0\n0001.0", {"--unique-numeric"});
  choose_output correct_output{to_vec("-0\n1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(general_numeric_unique) {
  choose_output out = run_choose("1\n10\n1.0\n1e0\n1e1", {"--unique-general-numeric"});
  choose_output correct_output{to_vec("1\n10\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(general_numeric_unique_with_parse_failure) {
  choose_output out = run_choose("1\n10\n\n \n+\n", {"--unique-general-numeric"});
  choose_output correct_output{to_vec("1\n10\n\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_limit_set) {
  choose_output out = run_choose("1\n1\n2\n2\n3\n3\n1\n2\n3\n4\n1\n1\n4\n4\n3\n3", {"--unique-use-set", "--unique-limit", "3"});
  choose_output correct_output{to_vec("1\n2\n3\n4\n1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_limit) {
  choose_output out = run_choose("1\n1\n2\n2\n3\n3\n1\n2\n3\n4\n1\n1\n4\n4\n3\n3", {"--unique-limit", "3"});
  choose_output correct_output{to_vec("1\n2\n3\n4\n1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_unique_use_set) {
  choose_output out = run_choose("-0\n0\n.0\n1\n1.0\n0001.0", {"--unique-numeric", "--unique-use-set"});
  choose_output correct_output{to_vec("-0\n1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(general_numeric_unique_use_set) {
  choose_output out = run_choose("1\n10\n1e1\n1e0", {"--unique-general-numeric", "--unique-use-set"});
  choose_output correct_output{to_vec("1\n10\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_sort) {
  choose_output out = run_choose("17\n-0\n.0\n1\n0001.0", {"--sort-numeric", "--stable"});
  choose_output correct_output{to_vec("-0\n.0\n1\n0001.0\n17\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(general_numeric_sort) {
  choose_output out = run_choose("1\n10\n1.0\n1e0\n1e1", {"--sort-general-numeric", "--stable"});
  choose_output correct_output{to_vec("1\n1.0\n1e0\n10\n1e1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_sort_2) {
  choose_output out = run_choose("3\n-2.1\n-2\n-1\n2\n1\n3", {"--sort-numeric"});
  choose_output correct_output{to_vec("-2.1\n-2\n-1\n1\n2\n3\n3\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(general_numeric_sort_with_parse_failure) {
  choose_output out = run_choose("4\n1\nfirst\nsecond\n3", {"--sort-general-numeric", "--stable"});
  choose_output correct_output{to_vec("first\nsecond\n1\n3\n4\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_sort_hex) {
  // this is a quirk of numeric comparison, just happens to work if the
  // beginning of the tokens all start with or all without "0x". If that isn't
  // the case then --field can match the desired part to make it so
  choose_output out = run_choose("0xABC\n0x0\n0x9\n0x999\n0xFFF", {"--sort-numeric"});
  choose_output correct_output{to_vec("0x0\n0x9\n0x999\n0xABC\n0xFFF\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(numeric_sort_lex_unique) {
  choose_output out = run_choose("2\n2.0\n2\n1\n1.0\n1\n1", {"--sort-numeric", "--stable", "-u"});
  choose_output correct_output{to_vec("1\n1.0\n2\n2.0\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(lex_sort_numeric_unique) {
  choose_output out = run_choose("10\n2\n1\n10.0\n2.0", {"--sort", "--unique-numeric"});
  choose_output correct_output{to_vec("1\n10\n2\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(stable_partial_sort) {
  choose_output out = run_choose("17\n-0\n.0\n1\n1.0\n0001.0", {"-u", "--sort-numeric", "--stable", "--out=3"});
  choose_output correct_output{to_vec("-0\n.0\n1\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out) {
  OutputSizeBoundFixture f(0);
  choose_output out = run_choose("this\nis\na\ntest", {"--out=3"});
  choose_output correct_output{to_vec("this\nis\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_tui) {
  choose_output out = run_choose("this\nis\na\ntest", {"--out=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"this", "is", "a"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_min) {
  OutputSizeBoundFixture f(0);
  choose_output out = run_choose("this\nis\na\ntest", {"--out=1,3"});
  choose_output correct_output{to_vec("is\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_min_tui) {
  choose_output out = run_choose("this\nis\na\ntest", {"--out=1,3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"is", "a"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(tail) {
  OutputSizeBoundFixture f(3);
  choose_output out = run_choose("here\nthis\nis\na\ntest", {"--tail=3"});
  choose_output correct_output{to_vec("is\na\ntest\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(tail_min) {
  OutputSizeBoundFixture f(4);
  choose_output out = run_choose("blah\nhere\nthis\nis\na\ntest", {"--tail=1,4"});
  choose_output correct_output{to_vec("this\nis\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_unique) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--unique"});
  choose_output correct_output{to_vec("a\nis\ntest\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--uniq"});
  choose_output correct_output{to_vec("a\nis\ntest\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq_numeric) {
  choose_output out = run_choose("3\n2\n1\n3.0\n2.0\n1.0", {"-n", "--sort", "--stable", "--uniq"});
  choose_output correct_output{to_vec("1\n2\n3\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq_general_numeric) {
  choose_output out = run_choose("3\n2\n1\n3e0\n2e0\n1e0", {"-g", "--sort", "--stable", "--uniq"});
  choose_output correct_output{to_vec("1\n2\n3\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq_empty) {
  choose_output out = run_choose("", {"--sort", "--uniq"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq_tui) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--uniq", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"a", "is", "test", "this"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_uniq_flip) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--uniq", "--flip"});
  choose_output correct_output{to_vec("this\ntest\nis\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(truncate_no_bound_sort) {
  // difficult to see different from this option, other than from the benchmarks
  choose_output out = run_choose("this\nis\na\ntest", {"--sort", "--out=2", "--truncate-no-bound"});
  choose_output correct_output{to_vec("a\nis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(truncate_no_bound_tail) {
  choose_output out = run_choose("this\nis\na\ntest", {"--tail=2", "--truncate-no-bound"});
  choose_output correct_output{to_vec("a\ntest\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(truncate_no_bound_sort_tail) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort", "--tail=2", "--truncate-no-bound"});
  choose_output correct_output{to_vec("test\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(truncate_no_bound_out) {
  choose_output out = run_choose("this\nis\na\ntest", {"--out=2", "--truncate-no-bound"});
  choose_output correct_output{to_vec("this\nis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_out) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--out=5"});
  choose_output correct_output{to_vec("a\nb\nc\nd\ne\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_out_tui) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--out=5", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"a", "b", "c", "d", "e"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_out_min) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--out=2,5"});
  choose_output correct_output{to_vec("c\nd\ne\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_tail) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--tail=5"});
  choose_output correct_output{to_vec("e\nf\ng\nh\ni\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_tail_min) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--tail=2,5"});
  choose_output correct_output{to_vec("e\nf\ng\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_reverse_tail) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("a\nb\nc\nd\ne\nf\ng\nh\ni\n", {"--sort-reverse", "--tail=5"});
  choose_output correct_output{to_vec("e\nd\nc\nb\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_flip_tail) {
  OutputSizeBoundFixture f(5);
  choose_output out = run_choose("i\nh\ng\nf\ne\nd\nc\nb\na\n", {"--sort", "--tail=5", "--flip"});
  choose_output correct_output{to_vec("i\nh\ng\nf\ne\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_out) {
  OutputSizeBoundFixture f(2);
  choose_output out = run_choose("a\na\na\na\na\nb\nb\nb\nc\nc\nc", {"--unique", "--out=1,2"});
  choose_output correct_output{to_vec("b\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_tail) {
  // output vector size is NOT bounded in these cases. unique elements must be kept track of
  choose_output out = run_choose("a\na\na\na\na\nb\nb\nb\nc\nc\nc", {"--unique", "--tail=2"});
  choose_output correct_output{to_vec("b\nc\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_tail_min) {
  choose_output out = run_choose("a\na\na\na\na\nb\nb\nb\nc\nc\nc", {"--unique", "--tail=1,2"});
  choose_output correct_output{to_vec("b\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_tail) {
  // tail has higher priority
  OutputSizeBoundFixture f(2);
  choose_output out = run_choose("this\nis\na\ntest", {"--out=1,2", "--tail=2"});
  choose_output correct_output{to_vec("a\ntest\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_unique_out) {
  // entirety of the input is read, but the mem size should not exceed the number of
  // unique elements from the input
  OutputSizeBoundFixture f(4);
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--unique", "--out=2"});
  choose_output correct_output{to_vec("a\nis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_unique_out_min) {
  OutputSizeBoundFixture f(4);
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--unique", "--out=1,2"});
  choose_output correct_output{to_vec("is\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_unique_tail) {
  OutputSizeBoundFixture f(4);
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--unique", "--tail=2"});
  choose_output correct_output{to_vec("test\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_unique_tail_min) {
  OutputSizeBoundFixture f(4);
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--sort", "--unique", "--tail=1,2"});
  choose_output correct_output{to_vec("test\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(unique_with_set) {
  choose_output out = run_choose("this\nis\nis\na\na\ntest", {"--unique", "--unique-use-set", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"this", "is", "a", "test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

#ifndef CHOOSE_DISABLE_FIELD
BOOST_AUTO_TEST_CASE(unique_by_field) {
  choose_output out = run_choose("alpha,tester\nbeta,tester\ngamma,tester,abcde", {"-t", "--unique", "--field", "^[^,]*+.\\K[^,]*+"});
  choose_output correct_output{std::vector<choose::Token>{"alpha,tester"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sort_by_field) {
  choose_output out = run_choose("a,z\nb,y\nc,x", {"-t", "--sort", "--field", "^[^,]*+.\\K[^,]*+"});
  choose_output correct_output{std::vector<choose::Token>{"c,x", "b,y", "a,z"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(field_with_no_matches) {
  choose_output out = run_choose("abc 1245\nzzz 123\nno match!", {"-t", "-s", "--field", "\\d+"});
  choose_output correct_output{std::vector<choose::Token>{"no match!", "zzz 123", "abc 1245"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}
#endif

// ========================

BOOST_AUTO_TEST_CASE(no_delimit) {
  choose_output out = run_choose("a\nb\nc", {"--delimit-not-at-end"});
  choose_output correct_output{to_vec("a\nb\nc")}; // no newline at end
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(delimit_on_empty) {
  choose_output out = run_choose("", {"--delimit-on-empty"});
  choose_output correct_output{to_vec("\n")}; // newline at end
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(no_delimit_delimit_on_empty) {
  // checking precedence
  choose_output out = run_choose("", {"--delimit-not-at-end", "--delimit-on-empty"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(in_limit) {
  choose_output out = run_choose("d\nc\nb\na", {"--head=3", "--sort", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"b", "c", "d"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(flip) {
  choose_output out = run_choose("a\nb\nc\nd", {"--flip"});
  choose_output correct_output{to_vec("d\nc\nb\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(reverse) {
  choose_output out = run_choose("a\nb\nc\nd", {"--sort-reverse"});
  choose_output correct_output{to_vec("d\nc\nb\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(direct_limit) {
  choose_output out = run_choose("a\nb\nc", {"--sub", "b", "e", "--sub", "e", "f", "--out=2"});
  choose_output correct_output{to_vec("a\nf\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit) {
  choose_output out = run_choose("a\nb\nc", {"--sort", "--out=2"});
  choose_output correct_output{to_vec("a\nb\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_start) {
  choose_output out = run_choose("0\n1\n2\n3\n4\n5\n6\n7\n8\n9", {"--out=2,5"});
  choose_output correct_output{to_vec("2\n3\n4\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_with_index) {
  choose_output out = run_choose("0\n1\n2\n3\n4\n5\n6\n7\n8\n9", {"--index=before", "--out=2,5"});
  choose_output correct_output{to_vec("0 2\n1 3\n2 4\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_start_with_sort) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort", "--out=1,3"});
  choose_output correct_output{to_vec("is\ntest\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_with_sort_past_end_stop) {
  choose_output out = run_choose("a\nb\nc", {"--sort", "--out=70"});
  choose_output correct_output{to_vec("a\nb\nc\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_with_sort_past_end_start) {
  choose_output out = run_choose("this\nis\na\ntest", {"--sort", "--out=100000,3"});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_unique_sort_with_limit_past_bound) {
  choose_output out = run_choose("this\nis\na\ntest", {"-t", "--unique", "--sort", "--out=100000"});
  choose_output correct_output{std::vector<choose::Token>{"a", "is", "test", "this"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(tail_unique_with_min_limit_past_bound) {
  choose_output out = run_choose("this\nis\na\ntest", {"-t", "--unique", "--tail=100000,100000"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(out_limit_unique) {
  choose_output out = run_choose("d\nd\nd\nd\nc\nb\na", {"--out=2", "--unique"});
  choose_output correct_output{to_vec("d\nc\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(ordered_ops) {
  choose_output out = run_choose("this\nis\nrra\ntest", {"-r", "--sub", "is", "rr", "--rm", "test", "--filter", "rr$", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"thrr", "rr"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(in_limit_process_token) {
  // niche code coverage
  choose_output out = run_choose("this\nis\na\ntest", {"--head=2"});
  choose_output correct_output{to_vec("this\nis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(ordered_op_in_limit) {
  choose_output out = run_choose("z\nz\nz\nz\nthe\nthis\nthere", {"-r", "-f", "^t", "--head=2"});
  choose_output correct_output{to_vec("the\nthis\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(replace_op_last) {
  choose_output out = run_choose("zzzzabczzzz", {"--sed", "-r", "[^z]", "--replace", "q"});
  choose_output correct_output{to_vec("zzzzqqqzzzz")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(replace_op_no_last) {
  choose_output out = run_choose("zzzzabczzzz", {"--sed", "-r", "[^z]", "--replace", "q", "--sub", "q", "0"});
  choose_output correct_output{to_vec("zzzz000zzzz")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sed_with_limit) {
  // this is a weird combination of args. should be allowed though
  choose_output out = run_choose("aaaa1bbbb2cccc3dddd4", {"--sed", "-r", "[0-9]", "--head=2"});
  choose_output correct_output{to_vec("aaaa1bbbb2cccc")}; // in limit stops at at the 3rd match, but allows everything before that
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sed_buffer_full) {
  const char* ch = "zzzzzzzzaaaaaazzzzzbbbbzzzzzzz";
  choose_output out = run_choose(ch, {"--sed", "-r", "(?:aaaaaa|bbbb)", "--buf-size=4"});
  choose_output correct_output{to_vec(ch)};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sed_beginning_discarded) {
  // niche code coverage check when beginning part is discarded
  choose_output out = run_choose("aaaa1bbbb2cccc3dddd4", {"--sed", "-r", "[0-9]", "--head=2", "--read=2"});
  choose_output correct_output{to_vec("aaaa1bbbb2cccc")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(sed_beginning_discarded_with_lookbehind) {
  choose_output out = run_choose("aaaa1bbbb2cccc3dddd4", {"--sed", "-r", "(?<=[a-z])[0-9]", "--head=2", "--read=2"});
  choose_output correct_output{to_vec("aaaa1bbbb2cccc")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(index_op_last) {
  choose_output out = run_choose("here are some words", {" ", "--index"});
  choose_output correct_output{to_vec("0 here\n1 are\n2 some\n3 words\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(index_op_after_last) {
  choose_output out = run_choose("here are some words", {" ", "--index=after"});
  choose_output correct_output{to_vec("here 0\nare 1\nsome 2\nwords 3\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(literal_sub) {
  choose_output out = run_choose("literal substitution", {"--sub", "literal", "good", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"good substitution"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(delimiter_sub) {
  choose_output out = run_choose("very long line here test other long line test test hello", {"test", "-o", "banana", "-d", "--buf-size=4"});
  choose_output correct_output{to_vec("very long line here banana other long line banana banana hello")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(index_ops) {
  choose_output out = run_choose("every\nother\nword\nis\nremoved\n5\n6\n7\n8\n9\n10", {"-r", "--index=after", "-f", "[02468]$", "--sub", "(.*) [0-9]+", "$1", "--index", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"0 every", "1 word", "2 removed", "3 6", "4 8", "5 10"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_empty_input) {
  choose_output out = run_choose("", {});
  choose_output correct_output{to_vec("")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_match_with_groups) {
  choose_output out = run_choose("abcde", {"-r", "--read=1", "--match", "b(c)(d)", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"bcd", "c", "d"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_match_with_groups_limit) {
  choose_output out = run_choose("abcde", {"-r", "--read=1", "--match", "b(c)(d)", "--head=2", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"bcd", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_no_match_lookbehind_retained) {
  // if there is no match, then the input is discarded but it keep enough for the lookbehind
  choose_output out = run_choose("aaabbbccc", {"--read=3", "-r", "--match", "(?<=aaa)bbb", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"bbb"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_partial_match_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=4", "-r", "--match", "(?<=aaa)bbb", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"bbb"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_no_delimiter_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=3", "-r", "(?<=aaa)bbb", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"aaa", "ccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_partial_delimiter_lookbehind_retained) {
  choose_output out = run_choose("aaabbbccc", {"--read=4", "-r", "(?<=aaa)bbb", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"aaa", "ccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(delimiter_no_match) {
  choose_output out = run_choose("aaabbbccc", {"zzzz", "--read=1", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"aaabbbccc"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(direct_but_tokens_stored) {
  choose_output out = run_choose("this\nis\nis\na\ntest", {"-u", "--out=3"});
  choose_output correct_output{to_vec("this\nis\na\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(flush) {
  // this is difficult to test other than manual, with a script that produces output with delays
  choose_output out = run_choose("here\nis\nsome\ninput", {"--flush"});
  choose_output correct_output{to_vec("here\nis\nsome\ninput\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(empty_match_target) {
  // important since PCRE2_NOTEMPTY_ATSTART is used to prevent infinite loop; ensures progress
  choose_output out = run_choose("1234", {"--match", "", "-t"});
  // one empty match in between each character and the end
  choose_output correct_output{std::vector<choose::Token>{"", "", "", "", ""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(input_is_delimiter) {
  choose_output out = run_choose("\n", {"-t"});
  choose_output correct_output{std::vector<choose::Token>{""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(input_is_delimiter_use_delimit) {
  choose_output out = run_choose("\n", {"--use-delimiter", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"", ""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(check_shrink_excess) {
  // creates a large subject but resizes internal buffer to remove the bytes that weren't written to
  choose_output out = run_choose("12345", {"zzzz", "--read=10000", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"12345"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(no_multiline) {
  choose_output out = run_choose("this\nis\na\ntest", {"-r", "--match", "^t", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(yes_multiline) {
  choose_output out = run_choose("this\nis\na\ntest", {"-r", "--multiline", "--match", "^t", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"t", "t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(begin_of_string) {
  // I found it surprising that \\A gives a lookbehind of 1. given the
  // preexisting logic, I expected that the buffer would be removed up to but
  // not including the t, making it the beginning of the line again. but the
  // lookbehind of 1 allows characters to be retained, so it is correctly
  // recognized as not the beginning of the string.
  choose_output out = run_choose("uaaat", {"-r", "--match", "--read=1", "\\A[ut]", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"u"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(begin_of_line) {
  choose_output out = run_choose("abcd\nefgh", {"-r", "--multiline", "--match", "^.", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"a", "e"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(end_of_string) {
  choose_output out = run_choose("uaaat", {"-r", "--match", "--read=6", "[ut]\\Z", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"t"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(end_of_line) {
  choose_output out = run_choose("abcd\nefgh", {"-r", "--multiline", ".$", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"abc", "\nefg"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_less_than_read) {
  // read takes the minimum of the available space in buffer left and the read amount
  choose_output out = run_choose("aaa1234aaa", {"--match", "1234", "--read=1000000", "--buf-size=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_match) {
  choose_output out = run_choose("aaa1234aaa", {"--match", "1234", "--read=1", "--buf-size=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_match_enough) {
  choose_output out = run_choose("aaaa1234aaaa", {"--match", "1234", "--read=1", "--buf-size=4", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"1234"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_trailing_incomplete_multibyte) {
  const char subject[] = {'z', 'z', 'z', (char)0xEF, (char)0xBB, (char)0xBF, 'a', '\0'};
  const char match_target[] = {'(', '?', ':', 'z', 'z', 'z', (char)0xEF, (char)0xBB, (char)0xBF, '|', 'a', ')', '\0'};
  choose_output out = run_choose(subject, {"--utf", "--match", "-r", match_target, "--buf-size=5"});
  choose_output correct_output{to_vec("a\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_sed_trailing_incomplete_multibyte) {
  const char subject[] = {'z', 'z', 'z', (char)0xEF, (char)0xBB, (char)0xBF, 'a', '\0'};
  const char match_target[] = {'(', '?', ':', 'z', 'z', 'z', (char)0xEF, (char)0xBB, (char)0xBF, '|', 'a', ')', '\0'};
  choose_output out = run_choose(subject, {"--utf", "--sed", "-r", match_target, "--buf-size=5"});
  choose_output correct_output{to_vec(subject)};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_entirely_composed_incomplete_multibyte) {
  const char ch[] = {(char)0xEF, (char)0xBB, (char)0xBF, 0};
  // this checks two things:
  // 1. no spin lock from being unable to clear the buffer
  // 2. the third byte causes a UTF-8 error on next iteration.
  BOOST_REQUIRE_THROW(choose_output out = run_choose(ch, {ch, "--utf", "--buf-size=2"}), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(buf_size_partial_match_enough) {
  choose_output out = run_choose("aaa1234aaaa1234aaaa", {"--match", "1234", "--read=4", "--buf-size=4", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"1234", "1234"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(frag_flush_in_process_token) {
  // notice the useless filter is needed so it doesn't do an optimization where the fragments are sent straight to the output
  choose_output out = run_choose("hereisaline123aaaa", {"123", "--read=1", "--buf-size=3", "-r", "-f", ".*"});
  choose_output correct_output{to_vec("hereisaline\naaaa\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(frag_prev_sep_offset_not_zero) {
  // when the buffer is filled because of lookbehind bytes, not from the previous delimiter end
  choose_output out = run_choose("123123", {"(?<=123)?123", "-r", "--read=1", "--buf-size=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"", ""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(frag_prev_sep_offset_not_zero_2) {
  choose_output out = run_choose("123123", {"(?<=123)123", "-r", "--read=1", "--buf-size=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"123123"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(frag_buffer_too_small_appending) {
  choose_output out = run_choose("123123123abc", {"abc", "--read=1", "--buf-size=3", "--buf-size-frag=3", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"123"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(frag_buffer_too_small_process_token) {
  // checks that process_token discards the fragment if it would exceed the fragment size
  choose_output out = run_choose("12341abc", {"abc", "--read=4", "--buf-size=4", "--buf-size-frag=4", "-t"});
  choose_output correct_output{std::vector<choose::Token>{""}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(process_fragments) {
  choose_output out = run_choose("hereisaline123aaaa", {"123", "--read=1", "--buf-size=3"});
  choose_output correct_output{to_vec("hereisaline\naaaa\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(process_fragment_in_count) {
  // ensure that fragments are counted correctly. only on completion is the in count incremented
  choose_output out = run_choose("zzzzzzzzz123hereisaline123aaaa", {"123", "--read=1", "--buf-size=3", "--head=2"});
  choose_output correct_output{to_vec("zzzzzzzzz\nhereisaline\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_delimiter_limit) {
  // ensure match failure behaviour on buffer full
  choose_output out = run_choose("qwerty123testerabqwerty123tester", {"-r", "(?:123)|(?:ab)", "--read=1", "--buf-size=2", "--buf-size-frag=1000", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"qwerty123tester", "qwerty123tester"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_delimiter_limit_from_lookbehind_enough) {
  // same as above, but because the lookbehind is too big
  choose_output out = run_choose("abcd12abcd12abcd", {"-r", "(?<=cd)12", "--read=1", "--buf-size=4", "--buf-size-frag=1000", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"abcd", "abcd", "abcd"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(buf_size_delimiter_limit_from_lookbehind) {
  // same as above, but because the lookbehind is too big
  choose_output out = run_choose("abcd12abcd12abcd", {"-r", "(?<=cd)12", "--read=1", "--buf-size=3", "--buf-size-frag=1000", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"abcd12abcd12abcd"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(complete_utf8) {
  // checks that the last utf8 char is completed before sending it to pcre2
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 0};
  choose_output out = run_choose(ch, {"--read=1", "--utf", "-t"});
  choose_output correct_output{std::vector<choose::Token>{ch}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(utf8_lookback_separates_multibyte) {
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
  // lookbehind of 4 bytes, reading 1 character at a time
  // the lookbehind must be correctly decremented to include the 0xE6 byte
  const char pattern[] = {'(', '?', '<', '=', (char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', ')', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--max-lookbehind=1", "--read=1", "--utf", "--match", pattern, "-t"});
  choose_output correct_output{std::vector<choose::Token>{"st"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8) {
  const char ch[] = {(char)0xFF, (char)0b11000000, (char)0b10000000, (char)0b10000000, 't', 'e', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", "test", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8_separating_multibyte_not_ok) {
  // sanity check for this: https://github.com/PCRE2Project/pcre2/issues/239. an
  // incomplete byte does not give a partial match. so allow invalid still needs
  // the subject_effective_end logic
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", ch, "-t"});
  choose_output correct_output{std::vector<choose::Token>{ch}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(invalid_utf8_overlong_byte) {
  const char ch[] = {continuation, continuation, continuation, continuation, 0};
  choose_output out = run_choose(ch, {"-r", "--read=1", "--utf-allow-invalid", "--match", "anything", "-t"});
  choose_output correct_output{std::vector<choose::Token>{}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(ensure_completed_utf8_multibytes_gives_err) {
  const char ch[] = {(char)0b10000000, 0};
  // abc required since single byte delimiter optimization turns off utf implicitly
  // inside matching logic
  BOOST_REQUIRE_THROW(run_choose(ch, {"--utf", "--read=1", "abc"}), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(misc_args)

BOOST_AUTO_TEST_CASE(null_output_delimiters) {
  choose_output out = run_choose("a\nb\nc", {"-zy"});
  choose_output correct_output{std::vector<char>{'a', '\0', 'b', '\0', 'c', '\0'}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(null_input_delimiter) {
  choose_output out = run_choose(std::vector<char>{'a', '\0', 'b', '\0', 'c'}, {"--read0", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"a", "b", "c"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(in_index_before) {
  choose_output out = run_choose("this\nis\na\ntest", {"--index=before", "-t"});
  choose_output correct_output{std::vector<choose::Token>{"0 this", "1 is", "2 a", "3 test"}};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(numeric_utils)

BOOST_AUTO_TEST_CASE(parse_number_unsigned) {
  BOOST_REQUIRE_EQUAL(*num::mul_overflow(7u, 15u), 105u);
  BOOST_REQUIRE(num::mul_overflow<uint16_t>(0xFFFF, 0xFFFF) == std::nullopt);
  BOOST_REQUIRE_EQUAL(*num::add_overflow(7u, 15u), 22u);
  BOOST_REQUIRE(num::add_overflow<uint16_t>(0xFFFF, 0xFFFF) == std::nullopt);

  auto should_not_be_called = []() { BOOST_REQUIRE(false); };

  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(should_not_be_called, "+0"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(should_not_be_called, "4294967295"), 0xFFFFFFFF);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(should_not_be_called, "16"), 16);

  int err_count = 0;
  auto must_be_called = [&]() { ++err_count; };

  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "-17"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "   123"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, NULL), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "4294967296"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "42949672950"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "4294967295", true, false), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<uint32_t>(must_be_called, "0", false, true), 0);
  BOOST_REQUIRE_EQUAL(err_count, 7);
}

BOOST_AUTO_TEST_CASE(parse_number_signed) {
  auto should_not_be_called = []() { BOOST_REQUIRE(false); };

  BOOST_REQUIRE_EQUAL(num::parse_number<char>(should_not_be_called, "-128"), -128);
  BOOST_REQUIRE_EQUAL(num::parse_number<char>(should_not_be_called, "+127"), +127);
  BOOST_REQUIRE_EQUAL(num::parse_number<char>(should_not_be_called, "72"), 72);
  BOOST_REQUIRE_EQUAL(num::parse_number<char>(should_not_be_called, "-72"), -72);

  int err_count = 0;
  auto must_be_called = [&]() { ++err_count; };

  BOOST_REQUIRE_EQUAL(num::parse_number<char>(must_be_called, "-129"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<char>(must_be_called, "+128"), 0);
  BOOST_REQUIRE_EQUAL(num::parse_number<char>(must_be_called, NULL), 0);
  BOOST_REQUIRE_EQUAL(err_count, 3);
}

BOOST_AUTO_TEST_CASE(parse_number_pair) {
  auto should_not_be_called = []() { BOOST_REQUIRE(false); };

  using T = std::tuple<char, std::optional<char>>;

  BOOST_REQUIRE(num::parse_number_pair<char>(should_not_be_called, "15,51") == (T{15, 51}));
  BOOST_REQUIRE(num::parse_number_pair<char>(should_not_be_called, "+15,-51") == (T{15, -51}));
  BOOST_REQUIRE(num::parse_number_pair<char>(should_not_be_called, "15") == (T{15, std::nullopt}));

  int err_count = 0;
  auto must_be_called = [&]() { ++err_count; };
  BOOST_REQUIRE(num::parse_number_pair<char>(must_be_called, "abcdef") == (T{0, 0}));
  BOOST_REQUIRE(num::parse_number_pair<char>(must_be_called, "15a51") == (T{0, 0}));
  BOOST_REQUIRE(num::parse_number_pair<char>(must_be_called, "15,bcd51") == (T{0, 0}));
  BOOST_REQUIRE_EQUAL(err_count, 3);
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
  // abc delimiter to not have single byte optimization.
  // required to actually put the error causing text through pcre2
  BOOST_REQUIRE_THROW(run_choose(ch, {"--utf", "abc"}), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(sub_failure) {
  BOOST_REQUIRE_THROW(run_choose("test", {"-r", "--sub", "test", "${"}), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
