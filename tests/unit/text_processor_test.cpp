#include "search/text_processor.hpp"
#include "search/errors.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

using namespace local_search;

namespace {
struct Expected {
  std::string word;
  std::string original;
  std::size_t begin;
  std::size_t end;
};

void CheckTokens(std::string_view text,
                 const std::vector<Expected>& expected) {
  const auto tokens = TextProcessor::Process(text);
  REQUIRE(tokens.size() == expected.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    INFO("token index: " << i);
    const auto& token = tokens[i];
    CHECK(token.word == expected[i].word);
    CHECK(token.occurrence.position == i);
    CHECK(token.occurrence.begin_byte == expected[i].begin);
    CHECK(token.occurrence.end_byte == expected[i].end);
    REQUIRE(token.occurrence.begin_byte <= token.occurrence.end_byte);
    REQUIRE(token.occurrence.end_byte <= text.size());
    CHECK(text.substr(token.occurrence.begin_byte,
                      token.occurrence.end_byte -
                          token.occurrence.begin_byte) == expected[i].original);
  }
}

std::string NormalizeUtf8(std::string_view text) {
  auto normalized = TextProcessor::Normalize(text);
  std::string result;
  normalized.toUTF8String(result);
  return result;
}
}

TEST_CASE("Normalize lowercases Russian and English", "[text_processor]") {
  CHECK(NormalizeUtf8("HeLLo") == "hello");
  CHECK(NormalizeUtf8("КОТ ЁЖ") == "кот ёж");
}

TEST_CASE("Normalize produces NFC and is idempotent", "[text_processor]") {
  const auto normalized = NormalizeUtf8("Е\u0308ж");
  CHECK(normalized == "ёж");
  CHECK(NormalizeUtf8(normalized) == normalized);
  CHECK(NormalizeUtf8("мо\u0301ре") == "мо\u0301ре");
}

TEST_CASE("Normalize accepts empty text", "[text_processor]") {
  CHECK(NormalizeUtf8("").empty());
}

TEST_CASE("Process preserves original UTF8 byte offsets", "[text_processor]") {
  CheckTokens("Я кот!", {{"я", "Я", 0, 2}, {"кот", "кот", 3, 9}});
}

TEST_CASE("Process preserves repeated tokens and skips separators",
          "[text_processor]") {
  CheckTokens("  кот   кот  ",
              {{"кот", "кот", 2, 8}, {"кот", "кот", 11, 17}});
}

TEST_CASE("Process emits the final word without a separator",
          "[text_processor]") {
  CheckTokens("кот", {{"кот", "кот", 0, 6}});
}

TEST_CASE("Process handles empty text and separator-only text",
          "[text_processor]") {
  CHECK(TextProcessor::Process("").empty());
  CHECK(TextProcessor::Process("  !?\t\r\n").empty());
  CHECK(TextProcessor::Process("😀—…").empty());
}

TEST_CASE("Process handles punctuation digits and whitespace",
          "[text_processor]") {
  CheckTokens("C++\tfoo-bar\n42",
              {{"c", "C", 0, 1}, {"foo", "foo", 4, 7},
               {"bar", "bar", 8, 11}, {"42", "42", 12, 14}});
}

TEST_CASE("Process keeps combining marks inside words",
          "[text_processor]") {
  CheckTokens("мо\u0301ре", {{"мо\u0301ре", "мо\u0301ре", 0, 10}});
}

TEST_CASE("Process ignores a combining mark before a word",
          "[text_processor]") {
  CheckTokens("\u0301кот", {{"кот", "кот", 2, 8}});
}

TEST_CASE("Process stores original offsets when NFC changes byte length",
          "[text_processor]") {
  CheckTokens("Е\u0308ж!", {{"ёж", "Е\u0308ж", 0, 6}});
}

TEST_CASE("Process stores original offsets when lowercasing changes length",
          "[text_processor]") {
  CheckTokens("\u0130!", {{"i\u0307", "\u0130", 0, 2}});
}

TEST_CASE("Process skips four-byte Unicode separators", "[text_processor]") {
  CheckTokens("😀Кот😀dog",
              {{"кот", "Кот", 4, 10}, {"dog", "dog", 14, 17}});
}

TEST_CASE("Process preserves yo and inflected forms as different words",
          "[text_processor]") {
  CheckTokens("е ё кот коты",
              {{"е", "е", 0, 2}, {"ё", "ё", 3, 5},
               {"кот", "кот", 6, 12}, {"коты", "коты", 13, 21}});
}

TEST_CASE("Process respects a string_view substring boundary",
          "[text_processor]") {
  const std::string backing = "xxКот!tail";
  CheckTokens(std::string_view(backing).substr(2, 6),
              {{"кот", "Кот", 0, 6}});
}

TEST_CASE("Split counts words for reserve", "[text_processor]") {
  for (const auto text : {"", " !? ", "  кот кот  ", "C++ foo-bar 42"}) {
    auto normalized = TextProcessor::Normalize(text);
    CHECK(TextProcessor::Split(normalized) ==
          TextProcessor::Process(text).size());
  }
}

TEST_CASE("SplitToTokens and Process produce the same tokens",
          "[text_processor]") {
  const std::string text = "Кот, кот!";
  const auto direct = TextProcessor::SplitToTokens(text);
  const auto processed = TextProcessor::Process(text);
  REQUIRE(direct.size() == processed.size());
  for (std::size_t i = 0; i < direct.size(); ++i) {
    CHECK(direct[i].word == processed[i].word);
    CHECK(direct[i].occurrence.position == processed[i].occurrence.position);
    CHECK(direct[i].occurrence.begin_byte == processed[i].occurrence.begin_byte);
    CHECK(direct[i].occurrence.end_byte == processed[i].occurrence.end_byte);
  }
}

TEST_CASE("Text processing rejects embedded zero bytes",
          "[text_processor]") {
  const std::string text("cat\0dog", 7);
  REQUIRE_THROWS_AS(TextProcessor::Normalize(text), ValidationError);
  REQUIRE_THROWS_AS(TextProcessor::SplitToTokens(text), ValidationError);
  REQUIRE_THROWS_AS(TextProcessor::Process(text), ValidationError);
}

TEST_CASE("Text processing rejects malformed UTF8", "[text_processor]") {
  const std::vector<std::string> invalid = {
      std::string("\x80", 1),
      std::string("\xD0", 1),
      std::string("\xD0\x20", 2),
      std::string("\xC0\xAF", 2),
      std::string("\xED\xA0\x80", 3),
      std::string("\xF4\x90\x80\x80", 4),
      std::string("cat ") + std::string("\xFF", 1)};
  for (const auto& text : invalid) {
    REQUIRE_THROWS_AS(TextProcessor::Normalize(text), ValidationError);
    REQUIRE_THROWS_AS(TextProcessor::SplitToTokens(text), ValidationError);
    REQUIRE_THROWS_AS(TextProcessor::Process(text), ValidationError);
  }
}

