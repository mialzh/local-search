#include "search/inverted_index.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using local_search::DocumentId;
using local_search::InvertedIndex;

namespace {
void RequirePostings(
    const InvertedIndex& index,
    std::string_view word,
    const std::map<DocumentId, std::size_t>& expected) {
    const auto* postings = index.Find(word);
    REQUIRE(postings != nullptr);
    REQUIRE(postings->size() == expected.size());

    std::map<DocumentId, std::size_t> actual;
    for (const auto& posting : *postings) {
        REQUIRE(actual.emplace(posting.document_id, posting.frequency).second);
    }
    REQUIRE(actual == expected);
}
} // namespace

TEST_CASE("InvertedIndex starts empty", "[inverted_index]") {
    const InvertedIndex index;

    REQUIRE(index.DocumentCount() == 0);
    REQUIRE(index.AverageDocumentLength() == 0.0);
    REQUIRE(index.Find("missing") == nullptr);
    REQUIRE_THROWS_AS(index.GetDocumentLength(1), std::runtime_error);
}

TEST_CASE("InvertedIndex counts repetitions once per document", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(42, {"кот", "кот", "дом"});

    RequirePostings(index, "кот", {{42, 2}});
    RequirePostings(index, "дом", {{42, 1}});
    REQUIRE(index.GetDocumentLength(42) == 3);
    REQUIRE(index.DocumentCount() == 1);
    REQUIRE(index.AverageDocumentLength() == 3.0);
}

TEST_CASE("InvertedIndex separates document frequency from term frequency", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(7, {"cat", "cat", "house"});
    index.AddDocument(100, {"cat", "cat", "cat", "bird"});

    RequirePostings(index, "cat", {{7, 2}, {100, 3}});
    RequirePostings(index, "house", {{7, 1}});
    RequirePostings(index, "bird", {{100, 1}});
    REQUIRE(index.GetDocumentLength(7) == 3);
    REQUIRE(index.GetDocumentLength(100) == 4);
    REQUIRE(index.DocumentCount() == 2);
    REQUIRE(index.AverageDocumentLength() == 3.5);
}

TEST_CASE("InvertedIndex uses floating point average length", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(1, {"one"});
    index.AddDocument(2, {"two"});
    index.AddDocument(3, {"three", "three"});

    REQUIRE(index.AverageDocumentLength() == Catch::Approx(4.0 / 3.0));
}

TEST_CASE("InvertedIndex missing lookups do not modify contents", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(1, {"cat", "cat"});
    const InvertedIndex& read_only = index;
    const auto* original = read_only.Find("cat");

    REQUIRE(read_only.Find("missing") == nullptr);
    REQUIRE(read_only.Find("missing") == nullptr);
    REQUIRE(read_only.DocumentCount() == 1);
    REQUIRE(read_only.AverageDocumentLength() == 2.0);
    REQUIRE(read_only.Find("cat") == original);
    RequirePostings(read_only, "cat", {{1, 2}});
}

TEST_CASE("InvertedIndex rejects unknown document lengths", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(5, {"known"});

    REQUIRE_THROWS_AS(index.GetDocumentLength(0), std::runtime_error);
    REQUIRE_THROWS_AS(index.GetDocumentLength(99), std::runtime_error);
    REQUIRE(index.DocumentCount() == 1);
    REQUIRE(index.GetDocumentLength(5) == 1);
    REQUIRE(index.AverageDocumentLength() == 1.0);
}

TEST_CASE("InvertedIndex rejects zero ID without changing contents", "[inverted_index][validation]") {
    InvertedIndex index;
    index.AddDocument(1, {"cat", "cat"});

    REQUIRE_THROWS_AS(index.AddDocument(0, {"new"}), std::runtime_error);
    REQUIRE_THROWS_AS(index.GetDocumentLength(0), std::runtime_error);
    REQUIRE(index.Find("new") == nullptr);
    REQUIRE(index.DocumentCount() == 1);
    REQUIRE(index.AverageDocumentLength() == 2.0);
    RequirePostings(index, "cat", {{1, 2}});
}

TEST_CASE("InvertedIndex rejects empty documents and permits retry", "[inverted_index][validation]") {
    InvertedIndex index;

    REQUIRE_THROWS_AS(index.AddDocument(1, {}), std::runtime_error);
    REQUIRE(index.DocumentCount() == 0);
    REQUIRE(index.AverageDocumentLength() == 0.0);

    index.AddDocument(1, {"cat", "cat"});
    REQUIRE_THROWS_AS(index.AddDocument(2, {}), std::runtime_error);
    REQUIRE_THROWS_AS(index.GetDocumentLength(2), std::runtime_error);
    REQUIRE(index.DocumentCount() == 1);
    REQUIRE(index.AverageDocumentLength() == 2.0);

    index.AddDocument(2, {"dog"});
    REQUIRE(index.DocumentCount() == 2);
    REQUIRE(index.AverageDocumentLength() == 1.5);
    RequirePostings(index, "cat", {{1, 2}});
    RequirePostings(index, "dog", {{2, 1}});
}

TEST_CASE("InvertedIndex rejects duplicate ID without replacing the document", "[inverted_index][validation]") {
    InvertedIndex index;
    index.AddDocument(1, {"cat", "cat"});
    index.AddDocument(2, {"dog"});

    REQUIRE_THROWS_AS(index.AddDocument(1, {"cat", "new", "new"}), std::runtime_error);
    REQUIRE(index.DocumentCount() == 2);
    REQUIRE(index.GetDocumentLength(1) == 2);
    REQUIRE(index.GetDocumentLength(2) == 1);
    REQUIRE(index.AverageDocumentLength() == 1.5);
    REQUIRE(index.Find("new") == nullptr);
    RequirePostings(index, "cat", {{1, 2}});
    RequirePostings(index, "dog", {{2, 1}});
}

TEST_CASE("InvertedIndex Find respects string view boundaries", "[inverted_index]") {
    InvertedIndex index;
    index.AddDocument(1, {"cat"});
    const std::string text = "xxcatyy";
    const std::string_view word = std::string_view(text).substr(2, 3);

    RequirePostings(index, word, {{1, 1}});
    REQUIRE(index.Find(std::string_view(text)) == nullptr);
    REQUIRE(index.Find(std::string_view{}) == nullptr);
}

TEST_CASE("InvertedIndex owns indexed words after the input is destroyed", "[inverted_index]") {
    InvertedIndex index;
    const std::string original_word(80, 'x');
    {
        std::vector<std::string> words{original_word, original_word};
        index.AddDocument(1, words);
        words[0].assign(80, 'y');
        words.clear();
    }

    RequirePostings(index, original_word, {{1, 2}});
    REQUIRE(index.Find(std::string(80, 'y')) == nullptr);
    REQUIRE(index.GetDocumentLength(1) == 2);
}

TEST_CASE("InvertedIndex copy construction produces independent contents", "[inverted_index][copy]") {
    InvertedIndex original;
    original.AddDocument(1, {"cat", "cat"});
    InvertedIndex copy = original;

    REQUIRE(copy.Find("cat") != original.Find("cat"));
    original.AddDocument(2, {"dog"});
    copy.AddDocument(3, {"cat", "bird", "bird"});

    RequirePostings(original, "cat", {{1, 2}});
    REQUIRE(original.Find("bird") == nullptr);
    REQUIRE(original.DocumentCount() == 2);
    REQUIRE(original.AverageDocumentLength() == 1.5);

    RequirePostings(copy, "cat", {{1, 2}, {3, 1}});
    RequirePostings(copy, "bird", {{3, 2}});
    REQUIRE(copy.Find("dog") == nullptr);
    REQUIRE(copy.DocumentCount() == 2);
    REQUIRE(copy.AverageDocumentLength() == 2.5);
}

TEST_CASE("InvertedIndex copy assignment replaces contents independently", "[inverted_index][copy]") {
    InvertedIndex original;
    original.AddDocument(1, {"cat", "cat"});
    InvertedIndex copy;
    copy.AddDocument(99, {"old"});

    copy = original;
    REQUIRE(copy.Find("old") == nullptr);
    REQUIRE_THROWS_AS(copy.GetDocumentLength(99), std::runtime_error);
    copy.AddDocument(2, {"cat"});

    RequirePostings(original, "cat", {{1, 2}});
    RequirePostings(copy, "cat", {{1, 2}, {2, 1}});
    REQUIRE(original.DocumentCount() == 1);
    REQUIRE(original.AverageDocumentLength() == 2.0);
    REQUIRE(copy.DocumentCount() == 2);
    REQUIRE(copy.AverageDocumentLength() == 1.5);
}

TEST_CASE("InvertedIndex Swap exchanges postings lengths and total length", "[inverted_index][swap]") {
    InvertedIndex left;
    left.AddDocument(1, {"cat", "cat", "dog"});
    InvertedIndex right;
    right.AddDocument(2, {"bird"});
    right.AddDocument(3, {"fish", "fish"});

    left.Swap(right);

    REQUIRE(left.DocumentCount() == 2);
    REQUIRE(left.AverageDocumentLength() == 1.5);
    REQUIRE(left.GetDocumentLength(2) == 1);
    REQUIRE(left.GetDocumentLength(3) == 2);
    REQUIRE(left.Find("cat") == nullptr);
    RequirePostings(left, "bird", {{2, 1}});
    RequirePostings(left, "fish", {{3, 2}});

    REQUIRE(right.DocumentCount() == 1);
    REQUIRE(right.AverageDocumentLength() == 3.0);
    REQUIRE(right.GetDocumentLength(1) == 3);
    REQUIRE(right.Find("fish") == nullptr);
    RequirePostings(right, "cat", {{1, 2}});
    RequirePostings(right, "dog", {{1, 1}});

    left.AddDocument(4, {"fish"});
    right.AddDocument(5, {"dog", "dog"});
    REQUIRE(left.AverageDocumentLength() == Catch::Approx(4.0 / 3.0));
    REQUIRE(right.AverageDocumentLength() == 2.5);
    RequirePostings(left, "fish", {{3, 2}, {4, 1}});
    RequirePostings(right, "dog", {{1, 1}, {5, 2}});
}

TEST_CASE("InvertedIndex Swap with an empty index permits subsequent additions", "[inverted_index][swap]") {
    InvertedIndex empty;
    InvertedIndex populated;
    populated.AddDocument(1, {"cat", "cat"});

    empty.Swap(populated);

    RequirePostings(empty, "cat", {{1, 2}});
    REQUIRE(empty.GetDocumentLength(1) == 2);
    REQUIRE(empty.DocumentCount() == 1);
    REQUIRE(empty.AverageDocumentLength() == 2.0);
    REQUIRE(populated.DocumentCount() == 0);
    REQUIRE(populated.AverageDocumentLength() == 0.0);
    REQUIRE(populated.Find("cat") == nullptr);
    REQUIRE_THROWS_AS(populated.GetDocumentLength(1), std::runtime_error);

    populated.AddDocument(1, {"dog"});
    RequirePostings(populated, "dog", {{1, 1}});
    REQUIRE(populated.AverageDocumentLength() == 1.0);
    REQUIRE(empty.Find("dog") == nullptr);
}

TEST_CASE("InvertedIndex Swap with itself preserves contents", "[inverted_index][swap]") {
    InvertedIndex index;
    index.AddDocument(1, {"cat", "cat", "dog"});

    index.Swap(index);

    RequirePostings(index, "cat", {{1, 2}});
    RequirePostings(index, "dog", {{1, 1}});
    REQUIRE(index.GetDocumentLength(1) == 3);
    REQUIRE(index.DocumentCount() == 1);
    REQUIRE(index.AverageDocumentLength() == 3.0);
}
