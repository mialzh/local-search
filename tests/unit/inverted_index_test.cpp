#include "search/inverted_index.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using local_search::DocumentId;
using local_search::InvertedIndex;
using local_search::Occurrence;
using local_search::Token;

namespace {

using ExpectedPostings = std::map<DocumentId, std::vector<Occurrence>>;

// Токены для текста, в котором слова разделены одним пробелом.
// Слова здесь уже считаются нормализованными.
std::vector<Token> Tokens(
    std::initializer_list<std::string_view> words) {
  std::vector<Token> tokens;
  tokens.reserve(words.size());

  std::size_t byte_offset = 0;

  for (const auto word : words) {
    tokens.push_back(Token{
        std::string(word),
        Occurrence{
            tokens.size(),
            byte_offset,
            byte_offset + word.size(),
        },
    });

    byte_offset += word.size() + 1;
  }

  return tokens;
}

void RequireOccurrences(
    const std::vector<Occurrence>& actual,
    const std::vector<Occurrence>& expected) {
  REQUIRE(actual.size() == expected.size());

  for (std::size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);

    REQUIRE(actual[i].position == expected[i].position);
    REQUIRE(actual[i].begin_byte == expected[i].begin_byte);
    REQUIRE(actual[i].end_byte == expected[i].end_byte);
  }
}

void RequirePostings(
    const InvertedIndex& index,
    std::string_view word,
    const ExpectedPostings& expected) {
  CAPTURE(word);

  const auto* postings = index.Find(word);
  REQUIRE(postings != nullptr);
  REQUIRE(postings->size() == expected.size());

  // Порядок документов в postings не фиксируем.
  std::map<DocumentId, const std::vector<Occurrence>*> actual;

  for (const auto& posting : *postings) {
    // Один документ должен встречаться ровно в одном Posting слова.
    REQUIRE(actual.emplace(
        posting.document_id, &posting.occurrences).second);
  }

  for (const auto& [id, occurrences] : expected) {
    CAPTURE(id);

    const auto it = actual.find(id);
    REQUIRE(it != actual.end());

    RequireOccurrences(*it->second, occurrences);
  }
}

} // namespace

TEST_CASE("InvertedIndex starts empty", "[inverted_index]") {
  const InvertedIndex index;

  REQUIRE(index.DocumentCount() == 0);
  REQUIRE(index.AverageDocumentLength() == 0.0);
  REQUIRE(index.Find("missing") == nullptr);
  REQUIRE_THROWS_AS(index.GetDocumentLength(1), std::runtime_error);
}

TEST_CASE("InvertedIndex stores all occurrences in one posting",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(42, Tokens({"cat", "cat", "house"}));

  RequirePostings(index, "cat", {
      {42, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(index, "house", {
      {42, {{2, 8, 13}}},
  });

  REQUIRE(index.GetDocumentLength(42) == 3);
  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.AverageDocumentLength() == 3.0);
}

TEST_CASE("InvertedIndex preserves positions across other words",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(
      1, Tokens({"cat", "house", "cat", "dog", "cat"}));

  RequirePostings(index, "cat", {
      {1, {{0, 0, 3}, {2, 10, 13}, {4, 18, 21}}},
  });

  RequirePostings(index, "house", {
      {1, {{1, 4, 9}}},
  });

  RequirePostings(index, "dog", {
      {1, {{3, 14, 17}}},
  });

  REQUIRE(index.GetDocumentLength(1) == 5);
}

TEST_CASE("InvertedIndex preserves original UTF8 byte offsets",
          "[inverted_index]") {
  InvertedIndex index;

  // Исходный текст: "  e\u0301!É".
  // Оба слова нормализованы в "é", но занимали разное число байтов.
  const std::vector<Token> tokens{
      {"é", {0, 2, 5}},
      {"é", {1, 6, 8}},
  };

  index.AddDocument(1, tokens);

  RequirePostings(index, "é", {
      {1, {{0, 2, 5}, {1, 6, 8}}},
  });

  REQUIRE(index.GetDocumentLength(1) == 2);
}

TEST_CASE("InvertedIndex stores Cyrillic byte offsets",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"кот", "дом", "кот"}));

  RequirePostings(index, "кот", {
      {1, {{0, 0, 6}, {2, 14, 20}}},
  });

  RequirePostings(index, "дом", {
      {1, {{1, 7, 13}}},
  });

  REQUIRE(index.GetDocumentLength(1) == 3);
}

TEST_CASE("InvertedIndex separates document and term frequency",
          "[inverted_index]") {
  InvertedIndex index;

  // Добавляем ID не по возрастанию.
  index.AddDocument(100, Tokens({"cat", "cat", "cat", "bird"}));
  index.AddDocument(7, Tokens({"cat", "cat", "house"}));

  RequirePostings(index, "cat", {
      {7, {{0, 0, 3}, {1, 4, 7}}},
      {100, {{0, 0, 3}, {1, 4, 7}, {2, 8, 11}}},
  });

  RequirePostings(index, "bird", {
      {100, {{3, 12, 16}}},
  });

  RequirePostings(index, "house", {
      {7, {{2, 8, 13}}},
  });

  const auto* postings = index.Find("cat");
  REQUIRE(postings != nullptr);
  REQUIRE(postings->size() == 2);

  REQUIRE(index.GetDocumentLength(7) == 3);
  REQUIRE(index.GetDocumentLength(100) == 4);
  REQUIRE(index.DocumentCount() == 2);
  REQUIRE(index.AverageDocumentLength() == 3.5);
}

TEST_CASE("InvertedIndex uses floating point average length",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"one"}));
  index.AddDocument(2, Tokens({"two"}));
  index.AddDocument(3, Tokens({"three", "three"}));

  REQUIRE(index.AverageDocumentLength() == Catch::Approx(4.0 / 3.0));
}

TEST_CASE("InvertedIndex Find respects string view boundaries",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"cat"}));

  const std::string text = "xxcatyy";
  const std::string_view word = std::string_view(text).substr(2, 3);
  const InvertedIndex& read_only = index;

  RequirePostings(read_only, word, {
      {1, {{0, 0, 3}}},
  });

  const auto* original = read_only.Find("cat");

  REQUIRE(read_only.Find(text) == nullptr);
  REQUIRE(read_only.Find("") == nullptr);
  REQUIRE(read_only.Find("missing") == nullptr);
  REQUIRE(read_only.Find("missing") == nullptr);

  REQUIRE(read_only.Find("cat") == original);
  REQUIRE(read_only.DocumentCount() == 1);
  REQUIRE(read_only.AverageDocumentLength() == 1.0);
}

TEST_CASE("InvertedIndex rejects unknown document lengths",
          "[inverted_index]") {
  InvertedIndex index;
  index.AddDocument(5, Tokens({"known"}));

  REQUIRE_THROWS_AS(index.GetDocumentLength(0), std::runtime_error);
  REQUIRE_THROWS_AS(index.GetDocumentLength(99), std::runtime_error);

  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.GetDocumentLength(5) == 1);
  REQUIRE(index.AverageDocumentLength() == 1.0);
}

TEST_CASE("InvertedIndex rejects zero ID without changing contents",
          "[inverted_index][validation]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"cat", "cat"}));

  REQUIRE_THROWS_AS(
      index.AddDocument(0, Tokens({"new"})),
      std::runtime_error);

  REQUIRE_THROWS_AS(index.GetDocumentLength(0), std::runtime_error);
  REQUIRE(index.Find("new") == nullptr);
  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.AverageDocumentLength() == 2.0);

  RequirePostings(index, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });
}

TEST_CASE("InvertedIndex rejects empty documents and permits retry",
          "[inverted_index][validation]") {
  InvertedIndex index;

  REQUIRE_THROWS_AS(index.AddDocument(1, {}), std::runtime_error);
  REQUIRE(index.DocumentCount() == 0);
  REQUIRE(index.AverageDocumentLength() == 0.0);
  REQUIRE_THROWS_AS(index.GetDocumentLength(1), std::runtime_error);

  index.AddDocument(1, Tokens({"cat", "cat"}));

  REQUIRE_THROWS_AS(index.AddDocument(2, {}), std::runtime_error);
  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.AverageDocumentLength() == 2.0);
  REQUIRE_THROWS_AS(index.GetDocumentLength(2), std::runtime_error);

  index.AddDocument(2, Tokens({"dog"}));

  REQUIRE(index.DocumentCount() == 2);
  REQUIRE(index.AverageDocumentLength() == 1.5);

  RequirePostings(index, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(index, "dog", {
      {2, {{0, 0, 3}}},
  });
}

TEST_CASE("InvertedIndex rejects duplicate ID without replacing contents",
          "[inverted_index][validation]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"cat", "cat"}));
  index.AddDocument(2, Tokens({"dog"}));

  REQUIRE_THROWS_AS(
      index.AddDocument(1, Tokens({"cat", "new", "new"})),
      std::runtime_error);

  REQUIRE(index.DocumentCount() == 2);
  REQUIRE(index.GetDocumentLength(1) == 2);
  REQUIRE(index.GetDocumentLength(2) == 1);
  REQUIRE(index.AverageDocumentLength() == 1.5);
  REQUIRE(index.Find("new") == nullptr);

  RequirePostings(index, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(index, "dog", {
      {2, {{0, 0, 3}}},
  });
}

TEST_CASE("InvertedIndex owns words and occurrences after input destruction",
          "[inverted_index]") {
  InvertedIndex index;
  const std::string word(80, 'x');

  {
    auto tokens = Tokens({word, word});
    index.AddDocument(1, tokens);

    // Добавление не должно забирать данные из входного вектора.
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].word == word);
    REQUIRE(tokens[1].word == word);

    tokens[0].word.assign(80, 'y');
    tokens[0].occurrence = Occurrence{99, 999, 1000};
    tokens[1].occurrence = Occurrence{100, 1001, 1002};
    tokens.clear();
  }

  RequirePostings(index, word, {
      {1, {{0, 0, 80}, {1, 81, 161}}},
  });

  REQUIRE(index.Find(std::string(80, 'y')) == nullptr);
  REQUIRE(index.GetDocumentLength(1) == 2);
}

TEST_CASE("InvertedIndex copy construction produces independent contents",
          "[inverted_index][copy]") {
  InvertedIndex original;
  original.AddDocument(1, Tokens({"cat", "cat"}));

  InvertedIndex copy = original;

  REQUIRE(copy.Find("cat") != original.Find("cat"));

  original.AddDocument(2, Tokens({"dog"}));
  copy.AddDocument(3, Tokens({"cat", "bird", "bird"}));

  RequirePostings(original, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  REQUIRE(original.Find("bird") == nullptr);
  REQUIRE(original.DocumentCount() == 2);
  REQUIRE(original.AverageDocumentLength() == 1.5);

  RequirePostings(copy, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
      {3, {{0, 0, 3}}},
  });

  RequirePostings(copy, "bird", {
      {3, {{1, 4, 8}, {2, 9, 13}}},
  });

  REQUIRE(copy.Find("dog") == nullptr);
  REQUIRE(copy.DocumentCount() == 2);
  REQUIRE(copy.AverageDocumentLength() == 2.5);
}

TEST_CASE("InvertedIndex copy assignment replaces contents independently",
          "[inverted_index][copy]") {
  InvertedIndex original;
  original.AddDocument(1, Tokens({"cat", "cat"}));

  InvertedIndex copy;
  copy.AddDocument(99, Tokens({"old"}));

  copy = original;

  REQUIRE(copy.Find("old") == nullptr);
  REQUIRE_THROWS_AS(copy.GetDocumentLength(99), std::runtime_error);

  copy.AddDocument(2, Tokens({"cat"}));

  RequirePostings(original, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(copy, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
      {2, {{0, 0, 3}}},
  });

  REQUIRE(original.DocumentCount() == 1);
  REQUIRE(original.AverageDocumentLength() == 2.0);
  REQUIRE(copy.DocumentCount() == 2);
  REQUIRE(copy.AverageDocumentLength() == 1.5);
}

TEST_CASE("InvertedIndex Swap exchanges all contents",
          "[inverted_index][swap]") {
  InvertedIndex left;
  left.AddDocument(1, Tokens({"cat", "cat", "dog"}));

  InvertedIndex right;
  right.AddDocument(2, Tokens({"bird"}));
  right.AddDocument(3, Tokens({"fish", "fish"}));

  left.Swap(right);

  REQUIRE(left.DocumentCount() == 2);
  REQUIRE(left.AverageDocumentLength() == 1.5);
  REQUIRE(left.GetDocumentLength(2) == 1);
  REQUIRE(left.GetDocumentLength(3) == 2);
  REQUIRE(left.Find("cat") == nullptr);
  REQUIRE_THROWS_AS(left.GetDocumentLength(1), std::runtime_error);

  RequirePostings(left, "bird", {
      {2, {{0, 0, 4}}},
  });

  RequirePostings(left, "fish", {
      {3, {{0, 0, 4}, {1, 5, 9}}},
  });

  REQUIRE(right.DocumentCount() == 1);
  REQUIRE(right.AverageDocumentLength() == 3.0);
  REQUIRE(right.GetDocumentLength(1) == 3);
  REQUIRE(right.Find("fish") == nullptr);

  RequirePostings(right, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(right, "dog", {
      {1, {{2, 8, 11}}},
  });

  // После Swap оба индекса должны поддерживать новые добавления.
  left.AddDocument(4, Tokens({"fish"}));
  right.AddDocument(5, Tokens({"dog", "dog"}));

  REQUIRE(left.AverageDocumentLength() == Catch::Approx(4.0 / 3.0));
  REQUIRE(right.AverageDocumentLength() == 2.5);

  RequirePostings(left, "fish", {
      {3, {{0, 0, 4}, {1, 5, 9}}},
      {4, {{0, 0, 4}}},
  });

  RequirePostings(right, "dog", {
      {1, {{2, 8, 11}}},
      {5, {{0, 0, 3}, {1, 4, 7}}},
  });
}

TEST_CASE("InvertedIndex Swap with an empty index permits reuse",
          "[inverted_index][swap]") {
  InvertedIndex empty;
  InvertedIndex populated;
  populated.AddDocument(1, Tokens({"cat", "cat"}));

  empty.Swap(populated);

  RequirePostings(empty, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  REQUIRE(empty.GetDocumentLength(1) == 2);
  REQUIRE(empty.DocumentCount() == 1);
  REQUIRE(empty.AverageDocumentLength() == 2.0);

  REQUIRE(populated.DocumentCount() == 0);
  REQUIRE(populated.AverageDocumentLength() == 0.0);
  REQUIRE(populated.Find("cat") == nullptr);
  REQUIRE_THROWS_AS(populated.GetDocumentLength(1), std::runtime_error);

  populated.AddDocument(1, Tokens({"dog"}));

  RequirePostings(populated, "dog", {
      {1, {{0, 0, 3}}},
  });

  REQUIRE(populated.AverageDocumentLength() == 1.0);
  REQUIRE(empty.Find("dog") == nullptr);
}

TEST_CASE("InvertedIndex Swap with itself preserves contents",
          "[inverted_index][swap]") {
  InvertedIndex index;
  index.AddDocument(1, Tokens({"cat", "cat", "dog"}));

  index.Swap(index);

  RequirePostings(index, "cat", {
      {1, {{0, 0, 3}, {1, 4, 7}}},
  });

  RequirePostings(index, "dog", {
      {1, {{2, 8, 11}}},
  });

  REQUIRE(index.GetDocumentLength(1) == 3);
  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.AverageDocumentLength() == 3.0);
}

TEST_CASE("InvertedIndex preserves occurrences during container growth",
          "[inverted_index]") {
  InvertedIndex index;
  ExpectedPostings expected_common;

  constexpr DocumentId document_count = 128;

  for (DocumentId id = 1; id <= document_count; ++id) {
    const std::string word = "unique" + std::to_string(id);

    const std::vector<Token> tokens{
        {"common", {0, 0, 6}},
        {word, {1, 7, 7 + word.size()}},
        {"common", {2, 80, 86}},
    };

    index.AddDocument(id, tokens);

    expected_common.emplace(
        id, std::vector<Occurrence>{{0, 0, 6}, {2, 80, 86}});
  }

  REQUIRE(index.DocumentCount() == document_count);
  REQUIRE(index.AverageDocumentLength() == 3.0);

  RequirePostings(index, "common", expected_common);

  for (DocumentId id = 1; id <= document_count; ++id) {
    CAPTURE(id);

    const std::string word = "unique" + std::to_string(id);

    REQUIRE(index.GetDocumentLength(id) == 3);

    RequirePostings(index, word, {
        {id, {{1, 7, 7 + word.size()}}},
    });
  }
}

TEST_CASE("InvertedIndex accepts the maximum nonzero document ID",
          "[inverted_index]") {
  InvertedIndex index;
  const DocumentId id = std::numeric_limits<DocumentId>::max();

  index.AddDocument(id, Tokens({"edge"}));

  RequirePostings(index, "edge", {
      {id, {{0, 0, 4}}},
  });

  REQUIRE(index.GetDocumentLength(id) == 1);
  REQUIRE(index.DocumentCount() == 1);
  REQUIRE(index.AverageDocumentLength() == 1.0);
}