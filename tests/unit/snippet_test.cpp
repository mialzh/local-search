#include "search/snippet.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using local_search::MakeSnippet;
using local_search::Occurrence;

namespace {

std::string Repeat(std::string_view value, std::size_t count) {
  std::string result;
  for (std::size_t i = 0; i < count; ++i) {
    result.append(value);
  }
  return result;
}

Occurrence At(std::string_view text, std::string_view word,
              std::size_t token_position) {
  const auto begin = text.find(word);
  REQUIRE(begin != std::string_view::npos);
  return {token_position, begin, begin + word.size()};
}

// Fixture helper: these texts have words separated by whitespace.
// It does not normalize words or choose snippet windows.
std::vector<Occurrence> WordMatches(
    std::string_view text,
    std::initializer_list<std::string_view> query_words) {
  const std::unordered_set<std::string_view> words(query_words);
  std::vector<Occurrence> result;
  std::size_t position = 0;
  std::size_t cursor = 0;

  while (cursor < text.size()) {
    const auto begin = text.find_first_not_of(" \t\r\n", cursor);
    if (begin == std::string_view::npos) {
      break;
    }
    auto end = text.find_first_of(" \t\r\n", begin);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    if (words.find(text.substr(begin, end - begin)) != words.end()) {
      result.push_back({position, begin, end});
    }
    ++position;
    cursor = end;
  }
  return result;
}

} // namespace

TEST_CASE("Snippet returns empty text unchanged", "[snippet]") {
  CHECK(MakeSnippet("", {}).empty());
}

TEST_CASE("Snippet zero limit returns an empty string", "[snippet]") {
  const std::string text = "cat dog";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 0).empty());
  CHECK(MakeSnippet(text, {}, 0).empty());
}

TEST_CASE("Snippet without matches preserves a short document", "[snippet]") {
  CHECK(MakeSnippet("  cat\nDOG!  ", {}, 100) == "  cat\nDOG!  ");
}

TEST_CASE("Snippet without matches truncates the document prefix", "[snippet]") {
  CHECK(MakeSnippet("0123456789", {}, 4) == "0123…");
}

TEST_CASE("Snippet preserves original case punctuation and whitespace", "[snippet]") {
  const std::string text = "  Кот,\nДОМ!  ";
  const std::vector<Occurrence> matches{
      At(text, "Кот", 0), At(text, "ДОМ", 1)};
  CHECK(MakeSnippet(text, matches, 100) == text);
}

TEST_CASE("Snippet adds an ellipsis only on the right", "[snippet]") {
  const std::string text = "cat tail";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 3) == "cat…");
}

TEST_CASE("Snippet adds an ellipsis only on the left", "[snippet]") {
  const std::string text = "head cat";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 3) == "…cat");
}

TEST_CASE("Snippet adds ellipses on both truncated sides", "[snippet]") {
  const std::string text = "head cat tail";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 3) == "…cat…");
}

TEST_CASE("Snippet adds no ellipsis to a document exactly at the limit", "[snippet]") {
  const std::string text = "cat dog";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 7) == text);
  CHECK(MakeSnippet(text, {}, 7) == text);
}

TEST_CASE("Snippet default limit counts 200 source code points", "[snippet]") {
  const std::string text = Repeat("я", 250);
  CHECK(MakeSnippet(text, {}) == Repeat("я", 200) + "…");
}

TEST_CASE("Snippet default middle fragment has 200 code points plus markers",
          "[snippet]") {
  const std::string text = std::string(300, 'a') + " cat " + std::string(300, 'b');
  const std::string expected = "…" + std::string(97, 'a') + " cat " +
                               std::string(98, 'b') + "…";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"})) == expected);
}

TEST_CASE("Snippet finds a match beyond the old 200 code point prefix", "[snippet]") {
  const std::string text = std::string(400, 'x') + " cat " + std::string(400, 'y');
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 5) == "… cat …");
}

TEST_CASE("Snippet prefers more matching occurrences to the earliest match",
          "[snippet]") {
  const std::string text = "cat" + std::string(40, ' ') + "cat cat cat" +
                           std::string(40, ' ') + "dog cat";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 11) ==
        "…cat cat cat…");
}

TEST_CASE("Snippet counts repeated occurrences of the same query word",
          "[snippet]") {
  const std::string text = "cat dog" + std::string(30, ' ') + "cat cat cat";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 11) ==
        "…cat cat cat");
}

TEST_CASE("Snippet breaks match count ties by the shorter core", "[snippet]") {
  const std::string text = "cat   dog" + std::string(30, ' ') + "cat dog zz";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 9) ==
        "… cat dog …");
}

TEST_CASE("Snippet compares core widths in code points rather than bytes",
          "[snippet]") {
  const std::string text = "cat abc dog" + std::string(30, ' ') + "cat 猫 dog zz";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 11) ==
        "… cat 猫 dog …");
}

TEST_CASE("Snippet breaks equal count and width ties by the earlier core",
          "[snippet]") {
  const std::string text = "cat dog" + std::string(30, ' ') + "cat dog";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 7) == "cat dog…");
}

TEST_CASE("Snippet accepts unsorted occurrences without changing the caller vector",
          "[snippet]") {
  const std::string text = "cat dog" + std::string(30, ' ') + "cat dog";
  auto matches = WordMatches(text, {"cat", "dog"});
  std::reverse(matches.begin(), matches.end());
  const auto original = matches;

  CHECK(MakeSnippet(text, matches, 7) == "cat dog…");
  REQUIRE(matches.size() == original.size());
  for (std::size_t i = 0; i < matches.size(); ++i) {
    CAPTURE(i);
    CHECK(matches[i].position == original[i].position);
    CHECK(matches[i].begin_byte == original[i].begin_byte);
    CHECK(matches[i].end_byte == original[i].end_byte);
  }
}

TEST_CASE("Snippet duplicate input occurrences do not increase the count",
          "[snippet]") {
  const std::string text = "cat dog" + std::string(30, ' ') + "cat cat cat";
  auto matches = WordMatches(text, {"cat", "dog"});
  REQUIRE_FALSE(matches.empty());
  const auto duplicate = matches.front();
  for (int i = 0; i < 10; ++i) {
    matches.push_back(duplicate);
  }
  CHECK(MakeSnippet(text, matches, 11) == "…cat cat cat");
}

TEST_CASE("Snippet retains unmatched words between matches", "[snippet]") {
  const std::string text = "cat one dog";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 11) == text);
}

TEST_CASE("Snippet transfers unavailable left context to the right", "[snippet]") {
  const std::string text = "cat abcdefghij";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 8) == "cat abcd…");
}

TEST_CASE("Snippet transfers unavailable right context to the left", "[snippet]") {
  const std::string text = "abcdefghij cat";
  CHECK(MakeSnippet(text, WordMatches(text, {"cat"}), 8) == "…ghij cat");
}

TEST_CASE("Snippet Cyrillic window length is not its byte length", "[snippet]") {
  const std::string text = "кот дом";
  const std::vector<Occurrence> matches{
      At(text, "кот", 0), At(text, "дом", 1)};
  CHECK(MakeSnippet(text, matches, 7) == text);
}

TEST_CASE("Snippet does not split four byte UTF8 in a prefix", "[snippet]") {
  CHECK(MakeSnippet("😀😀😀", {}, 2) == "😀😀…");
  CHECK(MakeSnippet("😀😀😀", {}, 1) == "😀…");
}

TEST_CASE("Snippet counts a four byte separator as one code point", "[snippet]") {
  const std::string text = "Кот😀дом";
  const std::vector<Occurrence> matches{
      At(text, "Кот", 0), At(text, "дом", 1)};
  CHECK(MakeSnippet(text, matches, 7) == text);
}

TEST_CASE("Snippet preserves decomposed original text when NFC changed token size",
          "[snippet]") {
  const std::string text = "aaa Е\u0308ж zzz";
  // The index word is "ёж"; its original range contains Е + combining mark + ж.
  CHECK(MakeSnippet(text, {At(text, "Е\u0308ж", 1)}, 3) == "…Е\u0308ж…");
}

TEST_CASE("Snippet preserves original text when lowercasing changed byte length",
          "[snippet]") {
  const std::string text = "aaa \u0130 zzz";
  // Original U+0130 occupies two UTF-8 bytes; normalized i + dot occupies three.
  CHECK(MakeSnippet(text, {At(text, "\u0130", 1)}, 1) == "…\u0130…");
}

TEST_CASE("Snippet limit counts code points including combining marks", "[snippet]") {
  const std::string text = "e\u0301!";
  CHECK(MakeSnippet(text, {At(text, "e\u0301", 0)}, 2) == "e\u0301…");
}

TEST_CASE("Snippet falls back to the first long matching word", "[snippet]") {
  const std::string text = "aa abcdefghij zz";
  CHECK(MakeSnippet(text, WordMatches(text, {"abcdefghij"}), 4) == "…abcd…");
}

TEST_CASE("Snippet can choose a fitting word after an oversized matching word",
          "[snippet]") {
  const std::string text = "abcdefghij cat";
  CHECK(MakeSnippet(text, WordMatches(text, {"abcdefghij", "cat"}), 3) == "…cat");
}

TEST_CASE("Snippet respects the bounds of a string view", "[snippet]") {
  const std::string backing = "PREcat dogPOST";
  const auto text = std::string_view(backing).substr(3, 7);
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), 7) == "cat dog");
}

TEST_CASE("Snippet accepts a very large limit without overflowing", "[snippet]") {
  const std::string text = "cat dog";
  const auto limit = std::numeric_limits<std::size_t>::max();
  CHECK(MakeSnippet(text, WordMatches(text, {"cat", "dog"}), limit) == text);
  CHECK(MakeSnippet(text, {}, limit) == text);
}

TEST_CASE("Snippet handles a large document with sparse matches", "[snippet]") {
  constexpr std::size_t padding = 2'000'000;
  const std::string text = std::string(padding, 'x') + " cat dog " +
                           std::string(padding, 'y');
  const std::vector<Occurrence> matches{
      {1, padding + 1, padding + 4},
      {2, padding + 5, padding + 8},
  };
  CHECK(MakeSnippet(text, matches, 7) == "…cat dog…");
}
