#define BOOST_TEST_MODULE choose_test_module
#include <boost/test/included/unit_test.hpp>
#include "args.hpp"
#include "token.hpp"

using namespace choose;

struct GlobalInit {
  GlobalInit() { setlocale(LC_ALL, ""); }
};

BOOST_GLOBAL_FIXTURE(GlobalInit);

BOOST_AUTO_TEST_SUITE(create_prompt_lines_test_suite)

BOOST_AUTO_TEST_CASE(empty_prompt) {
  auto ret = choose::str::create_prompt_lines("", 80);
  BOOST_REQUIRE_EQUAL(ret.size(), 1);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"") == 0);
}

BOOST_AUTO_TEST_CASE(zero_width) {
  // zero or negative width is handled correctly
  auto ret = choose::str::create_prompt_lines("abc", -1000);
  BOOST_REQUIRE_EQUAL(ret.size(), 3);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
}

BOOST_AUTO_TEST_CASE(only_whitespace) {
  auto ret = choose::str::create_prompt_lines("         ", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 1);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"") == 0);
}

BOOST_AUTO_TEST_CASE(small_width) {
  auto ret = choose::str::create_prompt_lines("abcd", 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 4);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[3].data(), L"d") == 0);
}

BOOST_AUTO_TEST_CASE(excess_spaces) {
  auto ret = choose::str::create_prompt_lines("    ab   cd  ", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 2);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"ab") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"cd") == 0);
}

BOOST_AUTO_TEST_CASE(full_empty_one) {
  auto ret = choose::str::create_prompt_lines("     a b c    ", 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 3);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"a") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"b") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"c") == 0);
}

BOOST_AUTO_TEST_CASE(full_empty_many) {
  auto ret = choose::str::create_prompt_lines("    111 222  333   444    555", 3);
  BOOST_REQUIRE_EQUAL(ret.size(), 5);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"111") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"222") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[2].data(), L"333") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[3].data(), L"444") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[4].data(), L"555") == 0);
}

BOOST_AUTO_TEST_CASE(invalid_utf8) {
  const char ch[] = {(char)0b11100000, '\0'};
  BOOST_CHECK_THROW(choose::str::create_prompt_lines(ch, 80), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(wide_utf8) {
  // this checks for a special case in which a single character would wrap
  // https://www.compart.com/en/unicode/U+6F22
  // 0xE6 0xBC 0xA2 encodes for a character which has a display width of 2
  const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, ' ', (char)0xE6, (char)0xBC, (char)0xA2, '\0'};
  auto ret = choose::str::create_prompt_lines(ch, 1);
  BOOST_REQUIRE_EQUAL(ret.size(), 2);
  BOOST_REQUIRE(std::wcscmp(ret[0].data(), L"漢") == 0);
  BOOST_REQUIRE(std::wcscmp(ret[1].data(), L"漢") == 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(other_string_utils_test_suite)

char continuation = 0b10000000;
char one = 0b00000000;
char two = 0b11000000;
char three = 0b11100000;
char four = 0b11110000;

BOOST_AUTO_TEST_CASE(test_find_last_non_continuation) {
  const char empty[] = {};
  BOOST_REQUIRE(choose::str::utf8::find_last_non_continuation(empty, empty) == 0);
  const char simple[] = {'a'};
  BOOST_REQUIRE(choose::str::utf8::find_last_non_continuation(simple, std::end(simple)) == simple);
  const char limit[] = {'a', continuation, continuation, continuation, continuation};
  BOOST_REQUIRE(choose::str::utf8::find_last_non_continuation(limit, std::end(limit)) == 0);
  const char not_limit[] = {'a', continuation, continuation, continuation};
  BOOST_REQUIRE(choose::str::utf8::find_last_non_continuation(not_limit, std::end(not_limit)) == not_limit);
}

BOOST_AUTO_TEST_CASE(test_bytes_required) {
  const char empty[] = {};
  BOOST_REQUIRE(choose::str::utf8::bytes_required(empty, empty) < 0);
  const char simple[] = {'a'};
  BOOST_REQUIRE(choose::str::utf8::bytes_required(simple, std::end(simple)) == 0);
  const char limit[] = {'a', continuation, continuation, continuation, continuation};
  BOOST_REQUIRE(choose::str::utf8::bytes_required(limit, std::end(limit)) < 0);
  const char not_limit[] = {four, continuation, continuation, continuation};
  BOOST_REQUIRE(choose::str::utf8::bytes_required(not_limit, std::end(not_limit)) == 0);

  char vals[] = {four, three, two, one};
  char* pos = vals;
  while (pos != std::end(vals)) {
    std::vector<char> tester{*pos};
    for (int i = 0; i < 3; ++i) {
      tester.push_back(continuation);
      int required = 4 - tester.size() - (pos - vals);
      BOOST_REQUIRE_EQUAL(choose::str::utf8::bytes_required(&*tester.cbegin(), &*tester.cend()), required);
    }
    ++pos;
  }
}

BOOST_AUTO_TEST_CASE(test_decrement_until_not_separating_multibyte) {
  const char none[] = {continuation, continuation};
  {
    const char* pos = &*std::crbegin(none);
    bool r = choose::str::utf8::decrement_until_not_separating_multibyte(pos, none, &*std::cend(none)) == pos;
    BOOST_REQUIRE(r);
  }
  {
    const char* end = &*std::cend(none);
    bool r = choose::str::utf8::decrement_until_not_separating_multibyte(end, none, &*std::cend(none)) == end;
    BOOST_REQUIRE(r);
  }
  const char vals[] = {continuation, one, continuation};
  const char* on_it = vals + 1;
  BOOST_REQUIRE_EQUAL(choose::str::utf8::decrement_until_not_separating_multibyte(on_it, vals, &*std::cend(vals)), on_it);
  const char* off_it = vals + 2;
  BOOST_REQUIRE_EQUAL(choose::str::utf8::decrement_until_not_separating_multibyte(off_it, vals, &*std::cend(vals)), on_it);
}

BOOST_AUTO_TEST_SUITE_END()

// choose either sends to stdout, or creates an interface that displays tokens
struct choose_output {
  std::variant<std::vector<char>, std::vector<choose::Token>> o;

  bool operator==(const choose_output& other) const {
    if (o.index() != other.o.index())
      return false;
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
    os << "stdout:\n";
    bool first = true;
    for (char ch : *out_str) {
      if (!first) {
        os << ',';
      }
      first = false;
      const char* escape_sequence = choose::str::get_escape_sequence(ch);
      if (escape_sequence) {
        os << escape_sequence;
      } else {
        os << ch;
      }
    }
  } else {
    const std::vector<choose::Token>& out_tokens = std::get<std::vector<choose::Token>>(out.o);
    os << "tokens:\n";
    bool first_token = true;
    for (const Token& t : out_tokens) {
      if (!first_token) {
        os << '\n';
      }
      first_token = false;
      bool first_in_token = true;
      for (char ch : t.buffer) {
        if (!first_in_token) {
          os << ',';
        }
        first_in_token = false;
        const char* escape_sequence = choose::str::get_escape_sequence(ch);
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

// runs choose with the given stdin and arguments
choose_output run_choose(const std::vector<char> input, const std::vector<const char*>& argv) {
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
  FILE* input_writer = fdopen(input_pipe[1], "w");
  FILE* input_reader = fdopen(input_pipe[0], "r");
  FILE* output_writer = fdopen(output_pipe[1], "w");
  FILE* output_reader = fdopen(output_pipe[0], "r");

  std::vector<char*> argv_non_const;
  argv_non_const.push_back(strdup("/tester/path/to/choose"));
  for (const char* p : argv) {
    // getopt might?? modify argv, making a copy just in case.
    argv_non_const.push_back(strdup(p));
  }

  str::write_f(input_writer, input);
  fclose(input_writer);

  auto args = choose::handle_args(argv_non_const.size(), argv_non_const.data(), input_reader, output_writer);
  choose_output ret;
  try {
    ret.o = choose::create_tokens(args);
    fclose(output_writer);
  } catch (const choose::termination_request&) {
    fclose(output_writer);
    std::vector<char> out;
    static constexpr size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    size_t bytes_read;
    do {
      bytes_read = fread(buf, sizeof(char), BUF_SIZE, output_reader);
      str::append_to_buffer(out, buf, std::begin(buf) + bytes_read);
    } while (bytes_read == BUF_SIZE);
    ret.o = std::move(out);
  }

  for (char* p : argv_non_const) {
    free(p);
  }

  fclose(input_reader);
  fclose(output_reader);
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

BOOST_AUTO_TEST_CASE(basic_output_accumulation) {
  // basic output avoids a copy when it can, but it still accumulates the input on no/partial separator match.
  // this is needed because an entire token needs to be accumulated before a filter can be applied
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--min-read=1", "-f", "s", "-t"});
  choose_output correct_output{to_vec("first\nsecond\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

BOOST_AUTO_TEST_CASE(basic_output_match) {
  choose_output out = run_choose("firstaaasecondaaathird", {"aaa", "--min-read=1", "--match", "-t"});
  choose_output correct_output{to_vec("aaa\naaa\n")};
  BOOST_REQUIRE_EQUAL(out, correct_output);
}

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

// BOOST_FIXTURE_TEST_CASE(unique, WriterFixture) {
//     fputs("this\nis\nis\na\na\ntest", writer);
//     close_fixture();
//     auto tokens = create_tokens({"choose", "--unique"});
//     BOOST_REQUIRE(tokens_equal(tokens, {"this", "is", "a", "test"}));
// }

// BOOST_FIXTURE_TEST_CASE(sort_reverse_and_unique, WriterFixture) {
//     fputs("this\nis\nis\na\na\ntest", writer);
//     close_fixture();
//     auto tokens = create_tokens({"choose", "--unique", "--sort-reverse"});
//     BOOST_REQUIRE(tokens_equal(tokens, {"this", "test", "is", "a"}));
// }

// BOOST_FIXTURE_TEST_CASE(in_limit, WriterFixture) {
//     fputs("d\nc\nb\na", writer);
//     close_fixture();
//     auto tokens = create_tokens({"choose", "--in=3", "--sort"});
//     BOOST_REQUIRE(tokens_equal(tokens, {"b", "c", "d"}));
// }

// BOOST_FIXTURE_TEST_CASE(ordered_ops, WriterFixture) {
//   fputs("this\nis\nrra\ntest", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--sub", "is", "rr", "--rm", "test", "--filter", "rr$"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"thrr", "rr"}));
// }

// BOOST_FIXTURE_TEST_CASE(index_ops, WriterFixture) {
//   fputs("every\nother\nword\nis\nremoved\n5\n6\n7\n8\n9\n10", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--in-index=after", "-f", "[02468]$", "--sub", "(.*) [0-9]+", "$1", "--out-index"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"0 every", "1 word", "2 removed", "3 6", "4 8", "5 10"}));
// }

// // these next tests are fine grained checks on create_tokens logic

// BOOST_FIXTURE_TEST_CASE(check_empty_input, WriterFixture) {
//   close_fixture();
//   auto tokens = create_tokens({"choose"});
//   BOOST_REQUIRE(tokens_equal(tokens, {}));
// }

// BOOST_FIXTURE_TEST_CASE(check_match_with_groups, WriterFixture) {
//   fputs("abcde", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--min-read=1", "--match", "b(c)(d)"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"bcd", "c", "d"}));
// }

// // if there is no match, then the input is discarded but it keep enough for the lookbehind
// BOOST_FIXTURE_TEST_CASE(check_no_match_lookbehind_retained, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=3", "-r", "--match", "(?<=aaa)bbb"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"bbb"}));
// }

// BOOST_FIXTURE_TEST_CASE(check_partial_match_lookbehind_retained, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=4", "-r", "--match", "(?<=aaa)bbb"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"bbb"}));
// }

// BOOST_FIXTURE_TEST_CASE(check_no_separator_lookbehind_retained, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=3", "-r", "(?<=aaa)bbb"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"aaa", "ccc"}));
// }

// BOOST_FIXTURE_TEST_CASE(check_partial_separator_lookbehind_retained, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=4", "-r", "(?<=aaa)bbb"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"aaa", "ccc"}));
// }

// BOOST_FIXTURE_TEST_CASE(separator_no_match, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "zzzz"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"aaabbbccc"}));
// }

// BOOST_FIXTURE_TEST_CASE(empty_separator, WriterFixture) {
//   // important since PCRE2_NOTEMPTY is used to prevent infinite loop
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", ""});
//   BOOST_REQUIRE(tokens_equal(tokens, {"aaabbbccc"}));
// }

// BOOST_FIXTURE_TEST_CASE(empty_match_target, WriterFixture) {
//   fputs("aaabbbccc", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--match", ""});
//   BOOST_REQUIRE(tokens_equal(tokens, {}));
// }

// BOOST_FIXTURE_TEST_CASE(input_is_separator, WriterFixture) {
//   fputs("\n", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose"});
//   BOOST_REQUIRE(tokens_equal(tokens, {""}));
// }

// BOOST_FIXTURE_TEST_CASE(input_is_separator_use_delimit, WriterFixture) {
//   fputs("\n", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--use-delimiter"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"", ""}));
// }

// BOOST_FIXTURE_TEST_CASE(check_shrink_excess, WriterFixture) {
//   // creates a large subject but resizes to remove the bytes that weren't written to.
//   fputs("12345", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "", "--min-read=10000"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"12345"}));
// }

// BOOST_FIXTURE_TEST_CASE(no_multiline, WriterFixture) {
//   fputs("this\nis\na\ntest", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--match", "^t"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"t"}));
// }

// BOOST_FIXTURE_TEST_CASE(yes_multiline, WriterFixture) {
//   fputs("this\nis\na\ntest", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--multiline", "--match", "^t"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"t", "t"}));
// }

// BOOST_FIXTURE_TEST_CASE(begin_of_string, WriterFixture) {
//   // I found it surprising that \\A gives a lookbehind of 1. given the
//   // preexisting logic, I expected that the buffer would be removed up to but
//   // not including the t, making it the beginning of the line again. but the
//   // lookbehind of 1 allows characters to be retained, so it is correctly
//   // recognized as not the beginning of the string.
//   fputs("uaaat", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--match", "--min-read=1", "\\A[ut]"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"u"}));
// }

// BOOST_FIXTURE_TEST_CASE(end_of_string, WriterFixture) {
//   fputs("uaaat\n", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "-r", "--match", "--min-read=6", "[ut]\\Z"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"t"}));
// }

// BOOST_FIXTURE_TEST_CASE(retain_limit_match, WriterFixture) {
//   fputs("1234", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--match", "1234", "--min-read=1", "--retain-limit=3"});
//   BOOST_REQUIRE(tokens_equal(tokens, {}));
// }

// BOOST_FIXTURE_TEST_CASE(retain_limit_separator, WriterFixture) {
//   // leading aaaa is to check that the offset is used correctly when the retain limit is hit
//   fputs("aaaa1234567", writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "1234567", "--min-read=3", "--retain-limit=3"});
//   BOOST_REQUIRE(tokens_equal(tokens, { "aaaa1234567" }));
// }

// BOOST_FIXTURE_TEST_CASE(complete_utf8, WriterFixture) {
//   // checks that the last utf8 byte is completed before sending it to pcre2
//   const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 0};
//   fputs(ch, writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=1", "--utf"});
//   BOOST_REQUIRE(tokens_equal(tokens, {ch}));
// }

// BOOST_FIXTURE_TEST_CASE(utf8_lookback_separates_multibyte, WriterFixture) {
//   const char ch[] = {(char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
//   fputs(ch, writer);
//   close_fixture();
//   // lookbehind of 4 bytes, reading >=1 character at a time
//   // the lookbehind must be correctly decremented to include the 0xE6 byte
//   const char pattern[] = {'(', '?', '<', '=', (char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', ')', 's', 't', 0};
//   auto tokens = create_tokens({"choose", "-r", "--max-lookbehind=1", "--min-read=1", "--utf", "--match", pattern});
//   BOOST_REQUIRE(tokens_equal(tokens, {"st"}));
// }

// BOOST_FIXTURE_TEST_CASE(invalid_utf8, WriterFixture) {
//   const char ch[] = {(char)0xFF, (char)0b11000000, (char)0b10000000, (char)0b10000000, 't', 'e', 's', 't', 0};
//   fputs(ch, writer);
//   close_fixture();
//   auto tokens = create_tokens({"choose", "--min-read=1", "--utf-allow-invalid", "--match", "test"});
//   BOOST_REQUIRE(tokens_equal(tokens, {"test"}));
// }

// BOOST_FIXTURE_TEST_CASE(invalid_utf8_bytes_still_retained, WriterFixture) {
//   const char ch[] = {'t', 'e', 's', 't', (char)0xE6, (char)0xBC, (char)0xA2, 't', 'e', 's', 't', 0};
//   fputs(ch, writer);
//   close_fixture();
//   const char result[] = {(char)0xE6, (char)0xBC, (char)0xA2, 0};
//   auto tokens = create_tokens({"choose", "--min-read=1", "--utf-allow-invalid", result, "--match"});
//   BOOST_REQUIRE(tokens_equal(tokens, {result}));
// }

BOOST_AUTO_TEST_SUITE_END()
