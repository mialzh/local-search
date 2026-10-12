// Place in tests/unit/search_engine_test.cpp and add to the search_tests target.
// In tests/unit/CMakeLists.txt, after creating search_tests:
// target_sources(search_tests PRIVATE search_engine_test.cpp)
// Run the existing project command: make test.
// Requires Catch2 3 with Catch2::Catch2WithMain. Do not define another main().
// These are component/integration tests: real SearchEngine, index, BM25,
// TextProcessor and DocumentStore are used together with temporary files.
// Every test owns a fresh directory that is removed on scope exit, including
// when an assertion fails or an exception is thrown.
// [contract] cases check additional requirements from TASKS.md, rather than
// accepting every behavior of the current implementation as correct.

#include "search/errors.hpp"
#include "search/search_engine.hpp"
#include "storage/document_store.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Catch::Approx;
using local_search::Document;
using local_search::DocumentId;
using local_search::DocumentStore;
using local_search::SearchEngine;
using local_search::SearchResult;
using local_search::StorageError;

class TempDirectory {
 public:
  TempDirectory() {
    const fs::path base = fs::temp_directory_path();
    std::random_device random;

    for (int attempt = 0; attempt < 128; ++attempt) {
      const fs::path candidate =
          base / ("local_search_engine_test_" + std::to_string(random()) +
                  "_" + std::to_string(random()));

      std::error_code error;
      if (fs::create_directory(candidate, error)) {
        path_ = candidate;
        return;
      }
      if (error) {
        throw fs::filesystem_error("Cannot create test directory", candidate,
                                   error);
      }
      // A colliding directory belongs to someone else; never remove or reuse it.
    }
    throw std::runtime_error("Cannot find an unused test directory");
  }

  ~TempDirectory() noexcept {
    std::error_code error;
    fs::remove_all(path_, error);
    if (error) {
      std::fprintf(stderr, "SearchEngine test cleanup failed (error %d)\n",
                   error.value());
    }
  }

  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  TempDirectory(TempDirectory&&) = delete;
  TempDirectory& operator=(TempDirectory&&) = delete;

  const fs::path& Path() const noexcept { return path_; }

 private:
  fs::path path_;
};

struct SearchEngineFixture {
  // Declaration order ensures store outlives engine, and temp outlives both.
  TempDirectory temp;
  DocumentStore store;
  SearchEngine engine;

  SearchEngineFixture() : store(temp.Path()), engine(store) {}
};

void WriteFile(const fs::path& path, std::string_view contents) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Cannot open test fixture");
  }
  file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  file.close();
  if (!file) {
    throw std::runtime_error("Cannot write or close test fixture");
  }
}

const SearchResult& ResultFor(const std::vector<SearchResult>& results,
                              DocumentId id) {
  const auto found = std::find_if(results.begin(), results.end(),
                                [id](const auto& result) {
                                  return result.document_id == id;
                                });
  if (found == results.end()) {
    throw std::runtime_error("Expected document missing from results: " +
                             std::to_string(id));
  }
  return *found;
}

std::vector<DocumentId> SortedIds(const std::vector<SearchResult>& results) {
  std::vector<DocumentId> ids;
  for (const auto& result : results) {
    ids.push_back(result.document_id);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

void CheckSameResults(const std::vector<SearchResult>& actual,
                      const std::vector<SearchResult>& expected) {
  REQUIRE(actual.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    REQUIRE(actual[i].document_id == expected[i].document_id);
    REQUIRE(actual[i].title == expected[i].title);
    REQUIRE(actual[i].snippet == expected[i].snippet);
    REQUIRE(actual[i].score == Approx(expected[i].score).epsilon(1e-12));
  }
}

}  // namespace

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine searches an empty collection",
                 "[search_engine][integration]") {
  REQUIRE(engine.Search("alpha", 10).empty());
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine saves the original document and returns metadata",
                 "[search_engine][integration]") {
  const std::string title = "Original document.txt";
  const std::string text = "Alpha, beta! ALPHA.";
  const auto added = engine.AddDocument(title, text);

  REQUIRE(added.document_id == 1);
  REQUIRE(added.title == title);
  REQUIRE(added.word_count == 3);

  const auto saved = store.Get(added.document_id);
  REQUIRE(saved.id == added.document_id);
  REQUIRE(saved.title == title);
  REQUIRE(saved.text == text);

  const auto results = engine.Search("alpha", 10);
  REQUIRE(results.size() == 1);
  REQUIRE(results.front().document_id == added.document_id);
  REQUIRE(results.front().title == title);
  REQUIRE(results.front().snippet == text);
  REQUIRE(std::isfinite(results.front().score));
  REQUIRE(results.front().score > 0.0);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine assigns distinct IDs to duplicate documents",
                 "[search_engine][integration]") {
  const auto first = engine.AddDocument("same.txt", "alpha beta");
  const auto second = engine.AddDocument("same.txt", "alpha beta");

  REQUIRE(first.document_id == 1);
  REQUIRE(second.document_id == 2);
  REQUIRE(store.LoadAll().size() == 2);
  const std::vector<DocumentId> expected{1, 2};
  REQUIRE(SortedIds(engine.Search("alpha", 10)) == expected);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine normalizes document and query text consistently",
                 "[search_engine][integration]") {
  const auto added = engine.AddDocument("case.txt", "Alpha, beta! ALPHA.");
  const auto baseline = engine.Search("alpha", 10);
  REQUIRE(baseline.size() == 1);
  REQUIRE(baseline.front().document_id == added.document_id);

  CheckSameResults(engine.Search(" \tALPHA!!!\r\n", 10), baseline);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine skips unknown words in a query",
                 "[search_engine][integration]") {
  engine.AddDocument("known.txt", "alpha beta");

  REQUIRE(engine.Search("unlisted", 10).empty());
  const auto baseline = engine.Search("alpha", 10);
  REQUIRE(baseline.size() == 1);
  CheckSameResults(engine.Search("alpha unlisted", 10), baseline);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine uses OR semantics and sums query contributions",
                 "[search_engine][integration]") {
  // Equal lengths and term frequencies make the two single-word matches equal.
  // The document matching both words must receive their sum and rank first.
  const auto both = engine.AddDocument("both.txt", "alpha beta filler filler");
  const auto only_alpha =
      engine.AddDocument("alpha.txt", "alpha filler filler filler");
  const auto only_beta =
      engine.AddDocument("beta.txt", "beta filler filler filler");
  engine.AddDocument("other.txt", "gamma filler filler filler");

  const auto results = engine.Search("alpha beta", 10);
  REQUIRE(results.size() == 3);
  REQUIRE(results.front().document_id == both.document_id);

  std::vector<DocumentId> expected{both.document_id, only_alpha.document_id,
                                   only_beta.document_id};
  std::sort(expected.begin(), expected.end());
  REQUIRE(SortedIds(results) == expected);

  const auto alpha_results = engine.Search("alpha", 10);
  const auto beta_results = engine.Search("beta", 10);
  const double expected_both_score =
      ResultFor(alpha_results, both.document_id).score +
      ResultFor(beta_results, both.document_id).score;
  REQUIRE(results.front().score == Approx(expected_both_score).epsilon(1e-12));
  REQUIRE(ResultFor(results, only_alpha.document_id).score ==
          Approx(ResultFor(alpha_results, only_alpha.document_id).score));
  REQUIRE(ResultFor(results, only_beta.document_id).score ==
          Approx(ResultFor(beta_results, only_beta.document_id).score));
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine ranks larger term frequencies first at equal length",
                 "[search_engine][integration]") {
  const auto weak = engine.AddDocument("weak.txt", "alpha filler filler filler");
  const auto strong = engine.AddDocument("strong.txt", "alpha alpha alpha filler");
  engine.AddDocument("other.txt", "filler filler filler filler");

  const auto results = engine.Search("alpha", 10);
  REQUIRE(results.size() == 2);
  REQUIRE(results[0].document_id == strong.document_id);
  REQUIRE(results[1].document_id == weak.document_id);
  REQUIRE(results[0].score > results[1].score);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine gives rare words a larger contribution",
                 "[search_engine][integration]") {
  const auto rare = engine.AddDocument("rare.txt", "rare filler");
  const auto common = engine.AddDocument("common.txt", "common filler");
  engine.AddDocument("common2.txt", "common filler");
  engine.AddDocument("common3.txt", "common filler");

  const auto results = engine.Search("rare common", 10);
  REQUIRE(results.size() == 4);
  REQUIRE(results.front().document_id == rare.document_id);
  REQUIRE(ResultFor(results, rare.document_id).score >
          ResultFor(results, common.document_id).score);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine accounts for document length in BM25",
                 "[search_engine][integration]") {
  const auto short_document = engine.AddDocument("short.txt", "alpha filler");
  const auto long_document = engine.AddDocument(
      "long.txt", "alpha filler filler filler filler filler filler filler");

  const auto results = engine.Search("alpha", 10);
  REQUIRE(results.size() == 2);
  REQUIRE(results[0].document_id == short_document.document_id);
  REQUIRE(results[1].document_id == long_document.document_id);
  REQUIRE(results[0].score > results[1].score);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine applies limit after sorting by score",
                 "[search_engine][integration]") {
  const auto weak = engine.AddDocument("weak.txt", "alpha filler filler filler");
  const auto strong = engine.AddDocument("strong.txt", "alpha alpha alpha alpha");
  const auto middle = engine.AddDocument("middle.txt", "alpha alpha filler filler");

  const auto full = engine.Search("alpha", 100);
  REQUIRE(full.size() == 3);
  REQUIRE(full[0].document_id == strong.document_id);
  REQUIRE(full[1].document_id == middle.document_id);
  REQUIRE(full[2].document_id == weak.document_id);
  REQUIRE(full[0].score > full[1].score);
  REQUIRE(full[1].score > full[2].score);

  SECTION("One result") {
    const std::vector<SearchResult> expected{full[0]};
    CheckSameResults(engine.Search("alpha", 1), expected);
  }
  SECTION("Two results") {
    const std::vector<SearchResult> expected{full[0], full[1]};
    CheckSameResults(engine.Search("alpha", 2), expected);
  }
  SECTION("Limit exceeds the number of matches") {
    CheckSameResults(engine.Search("alpha", 10), full);
  }
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine returns the first 200 ASCII characters as snippet",
                 "[search_engine][integration]") {
  std::string text;
  for (int i = 0; i < 80; ++i) {
    text += "alpha ";
  }
  text += "needle";
  const auto added = engine.AddDocument("long.txt", text);

  const auto results = engine.Search("needle", 10);
  REQUIRE(results.size() == 1);
  REQUIRE(results.front().document_id == added.document_id);
  REQUIRE(results.front().snippet == text.substr(0, 200));
}

TEST_CASE("SearchEngine restores documents and scores after reopening storage",
          "[search_engine][integration][restore]") {
  TempDirectory temp;
  std::vector<SearchResult> before;
  {
    DocumentStore writer(temp.Path());
    SearchEngine original(writer);
    original.AddDocument("first.txt", "alpha filler filler filler");
    original.AddDocument("second.txt", "alpha alpha alpha filler");
    before = original.Search("alpha", 10);
    REQUIRE(before.size() == 2);
  }

  DocumentStore reader(temp.Path());
  SearchEngine restored(reader);
  restored.Restore();

  CheckSameResults(restored.Search("alpha", 10), before);
  const auto added = restored.AddDocument("third.txt", "beta filler");
  REQUIRE(added.document_id == 3);
  REQUIRE(reader.LoadAll().size() == 3);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine restores next ID from the maximum rather than count",
                 "[search_engine][integration][restore]") {
  store.Save(Document{7, "seven.txt", "beta filler"});
  store.Save(Document{2, "two.txt", "alpha filler"});

  engine.Restore();

  const auto alpha = engine.Search("alpha", 10);
  const auto beta = engine.Search("beta", 10);
  REQUIRE(alpha.size() == 1);
  REQUIRE(alpha.front().document_id == 2);
  REQUIRE(beta.size() == 1);
  REQUIRE(beta.front().document_id == 7);
  REQUIRE(engine.AddDocument("next.txt", "gamma filler").document_id == 8);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine repeated Restore does not duplicate data",
                 "[search_engine][integration][restore]") {
  engine.AddDocument("first.txt", "alpha filler filler filler");
  engine.AddDocument("second.txt", "alpha alpha alpha filler");
  const auto before = engine.Search("alpha", 10);
  REQUIRE(before.size() == 2);

  engine.Restore();
  engine.Restore();

  CheckSameResults(engine.Search("alpha", 10), before);
  REQUIRE(store.LoadAll().size() == 2);
  REQUIRE(engine.AddDocument("next.txt", "beta filler").document_id == 3);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine Restore replaces the old index and resets an empty store",
                 "[search_engine][integration][restore]") {
  const auto old = engine.AddDocument("old.txt", "oldterm filler");
  store.Remove(old.document_id);

  SECTION("A different collection") {
    store.Save(Document{9, "new.txt", "newterm filler"});
    engine.Restore();
    REQUIRE(engine.Search("oldterm", 10).empty());
    const auto results = engine.Search("newterm", 10);
    REQUIRE(results.size() == 1);
    REQUIRE(results.front().document_id == 9);
    REQUIRE(engine.AddDocument("next.txt", "alpha filler").document_id == 10);
  }
  SECTION("An empty collection") {
    engine.Restore();
    REQUIRE(engine.Search("oldterm", 10).empty());
    REQUIRE(engine.AddDocument("first.txt", "alpha filler").document_id == 1);
  }
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine failed Restore preserves old results and next ID",
                 "[search_engine][integration][restore][exceptions]") {
  engine.AddDocument("original.txt", "alpha filler");
  const auto before = engine.Search("alpha", 10);
  REQUIRE(before.size() == 1);
  const fs::path broken = temp.Path() / "documents" / "99.json";
  WriteFile(broken, "{broken JSON");

  REQUIRE_THROWS_AS(engine.Restore(), StorageError);
  CheckSameResults(engine.Search("alpha", 10), before);

  REQUIRE(fs::remove(broken));
  REQUIRE(engine.AddDocument("next.txt", "beta filler").document_id == 2);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine save failure preserves results statistics and next ID",
                 "[search_engine][integration][exceptions]") {
  engine.AddDocument("original.txt", "alpha filler");
  const auto before = engine.Search("alpha", 10);
  REQUIRE(before.size() == 1);

  // Make writes fail without relying on chmod (which root can bypass).
  // Keep the original files in this test's own directory so they can be restored.
  const fs::path documents = temp.Path() / "documents";
  const fs::path backup = temp.Path() / "documents_backup";
  fs::rename(documents, backup);
  WriteFile(documents, "This is a file, not a directory.");

  REQUIRE_THROWS_AS(engine.AddDocument("failed.txt", "beta beta beta"),
                    StorageError);

  REQUIRE(fs::remove(documents));
  fs::rename(backup, documents);
  REQUIRE(store.LoadAll().size() == 1);
  REQUIRE(store.Get(1).title == "original.txt");
  CheckSameResults(engine.Search("alpha", 10), before);
  REQUIRE(engine.Search("beta", 10).empty());
  const auto next = engine.AddDocument("next.txt", "beta filler");
  REQUIRE(next.document_id == 2);
  const auto results = engine.Search("beta", 10);
  REQUIRE(results.size() == 1);
  REQUIRE(results.front().document_id == next.document_id);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine propagates failures while loading result documents",
                 "[search_engine][integration][exceptions]") {
  const auto added = engine.AddDocument("missing.txt", "alpha filler");
  store.Remove(added.document_id);

  REQUIRE_THROWS_AS(engine.Search("alpha", 10), StorageError);
}

// The following cases intentionally enforce TASKS.md requirements that are
// missing from the last SearchEngine implementation shown in the conversation.

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine duplicate query words do not increase scores",
                 "[search_engine][integration][contract]") {
  engine.AddDocument("first.txt", "alpha beta filler filler");
  engine.AddDocument("second.txt", "alpha filler filler filler");
  const auto single = engine.Search("alpha", 10);
  REQUIRE(single.size() == 2);
  CheckSameResults(engine.Search("alpha alpha ALPHA", 10), single);

  const auto distinct = engine.Search("alpha beta", 10);
  CheckSameResults(engine.Search("beta alpha beta alpha", 10), distinct);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine breaks equal score ties by ascending document ID",
                 "[search_engine][integration][contract]") {
  const auto first = engine.AddDocument("first.txt", "alpha filler");
  const auto second = engine.AddDocument("second.txt", "alpha filler");
  const auto third = engine.AddDocument("third.txt", "alpha filler");

  const auto results = engine.Search("alpha", 10);
  REQUIRE(results.size() == 3);
  REQUIRE(results[0].score == Approx(results[1].score));
  REQUIRE(results[1].score == Approx(results[2].score));
  REQUIRE(results[0].document_id == first.document_id);
  REQUIRE(results[1].document_id == second.document_id);
  REQUIRE(results[2].document_id == third.document_id);
  const auto limited = engine.Search("alpha", 1);
  REQUIRE(limited.size() == 1);
  REQUIRE(limited.front().document_id == first.document_id);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine snippet contains 200 code points without splitting UTF8",
                 "[search_engine][integration][contract]") {
  // 199 ASCII code points, followed by a two-byte Cyrillic letter as point 200.
  // substr(0, 200) would include only its first byte, creating invalid UTF-8.
  const std::string expected = "alpha " + std::string(193, 'x') + "\xD1\x8F";
  const std::string text = expected + " needle";
  engine.AddDocument("unicode.txt", text);

  const auto results = engine.Search("needle", 10);
  REQUIRE(results.size() == 1);
  REQUIRE(results.front().snippet == expected);
  REQUIRE(results.front().snippet.size() == 201);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine rejects queries without tokens",
                 "[search_engine][integration][contract][validation]") {
  engine.AddDocument("original.txt", "alpha filler");
  for (const std::string query : {"", " \t\r\n", "!!!"}) {
    CAPTURE(query);
    REQUIRE_THROWS(engine.Search(query, 10));
  }
  REQUIRE(engine.Search("alpha", 10).size() == 1);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine rejects limits outside 1 to 100",
                 "[search_engine][integration][contract][validation]") {
  engine.AddDocument("original.txt", "alpha filler");
  REQUIRE_THROWS(engine.Search("alpha", 0));
  REQUIRE_THROWS(engine.Search("alpha", 101));
  REQUIRE(engine.Search("alpha", 1).size() == 1);
  REQUIRE(engine.Search("alpha", 100).size() == 1);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine rejects documents without tokens and consumes no ID",
                 "[search_engine][integration][contract][validation]") {
  for (const std::string text : {"", " \t\r\n", "!!!"}) {
    CAPTURE(text);
    REQUIRE_THROWS(engine.AddDocument("empty.txt", text));
    REQUIRE(store.LoadAll().empty());
    REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "1.json.tmp"));
  }
  REQUIRE(engine.AddDocument("valid.txt", "alpha filler").document_id == 1);
}

TEST_CASE_METHOD(SearchEngineFixture,
                 "SearchEngine rejects Restore when the next document ID would overflow",
                 "[search_engine][integration][contract][restore]") {
  engine.AddDocument("original.txt", "alpha filler");
  const auto before = engine.Search("alpha", 10);
  REQUIRE(before.size() == 1);
  const DocumentId largest = std::numeric_limits<DocumentId>::max();
  store.Save(Document{largest, "largest.txt", "beta filler"});

  REQUIRE_THROWS(engine.Restore());
  CheckSameResults(engine.Search("alpha", 10), before);
  REQUIRE(engine.Search("beta", 10).empty());

  store.Remove(largest);
  REQUIRE(engine.AddDocument("next.txt", "gamma filler").document_id == 2);
}
