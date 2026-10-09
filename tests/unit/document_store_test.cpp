// Place this file in tests/unit/ and add it to the Catch2 test target.
// Every test uses its own system temporary directory.
// TempDirectory removes that directory (including all documents) on scope exit.

#include "search/errors.hpp"
#include "storage/document_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

namespace fs = std::filesystem;
using local_search::Document;
using local_search::DocumentId;
using local_search::DocumentStore;
using local_search::StorageError;

class TempDirectory {
 public:
  TempDirectory() {
    const fs::path base = fs::temp_directory_path();
    std::random_device random;

    for (int attempt = 0; attempt < 128; ++attempt) {
      const fs::path candidate =
          base / ("local_search_document_store_test_" +
                  std::to_string(random()) + "_" + std::to_string(random()));

      std::error_code error;
      if (fs::create_directory(candidate, error)) {
        path_ = candidate;
        return;
      }
      if (error) {
        throw fs::filesystem_error("Cannot create test directory", candidate,
                                   error);
      }
      // An existing directory belongs to someone else; never reuse or remove it.
    }
    throw std::runtime_error("Cannot find an unused test directory");
  }

  ~TempDirectory() noexcept {
    std::error_code error;
    fs::remove_all(path_, error);
    if (error) {
      // Destructors must not throw while a failed assertion is unwinding.
      std::fprintf(stderr, "Test directory cleanup failed (error %d)\n",
                   error.value());
    }
  }

  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  TempDirectory(TempDirectory&&) = delete;
  TempDirectory& operator=(TempDirectory&&) = delete;

  const fs::path& Path() const noexcept {
    return path_;
  }

 private:
  fs::path path_;
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

// Arguments are raw JSON fragments, so tests can supply invalid field types.
// The test target does not need to include or link a JSON library itself.
std::string StoredJson(std::string_view id = "1",
                       std::string_view version = "1",
                       std::string_view title = R"("example.txt")",
                       std::string_view text = R"("hello")") {
  return "{\"schema_version\":" + std::string(version) +
         ",\"document_id\":" + std::string(id) +
         ",\"title\":" + std::string(title) +
         ",\"text\":" + std::string(text) + "}";
}

void CheckDocument(const Document& actual, const Document& expected) {
  REQUIRE(actual.id == expected.id);
  REQUIRE(actual.title == expected.title);
  REQUIRE(actual.text == expected.text);
}

}  // namespace

TEST_CASE("DocumentStore creates its directory and starts empty",
          "[document_store]") {
  TempDirectory temp;
  const fs::path data_dir = temp.Path() / "nested" / "data";
  DocumentStore store(data_dir);

  REQUIRE(fs::is_directory(data_dir / "documents"));
  REQUIRE(store.LoadAll().empty());
}

TEST_CASE("DocumentStore reports directory creation errors as StorageError",
          "[document_store][validation]") {
  TempDirectory temp;
  const fs::path data_dir = temp.Path() / "regular_file";
  WriteFile(data_dir, "This is a file, not a directory.");

  REQUIRE_THROWS_AS(DocumentStore{data_dir}, StorageError);
}

TEST_CASE("DocumentStore preserves Unicode and escaped characters",
          "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());

  Document original{42, "Документ \"пример\".txt",
                    "Первая строка\nВторая строка\tC:\\documents\\file\n"};
  original.text.append("\0tail", 5);
  store.Save(original);

  CheckDocument(store.Get(original.id), original);
}

TEST_CASE("DocumentStore accepts empty title and text", "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const Document original{1, "", ""};

  store.Save(original);

  CheckDocument(store.Get(1), original);
}

TEST_CASE("DocumentStore reads saved documents through a new instance",
          "[document_store]") {
  TempDirectory temp;
  const Document original{7, "persistent.txt", "Saved before reopening."};

  {
    DocumentStore writer(temp.Path());
    writer.Save(original);
  }

  DocumentStore reader(temp.Path());
  CheckDocument(reader.Get(7), original);
  const auto documents = reader.LoadAll();
  REQUIRE(documents.size() == 1);
  CheckDocument(documents.front(), original);
}

TEST_CASE("DocumentStore rejects zero IDs without creating files",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());

  REQUIRE_THROWS_AS(store.Save(Document{0, "zero.txt", "text"}), StorageError);
  REQUIRE_THROWS_AS(store.Get(0), StorageError);
  REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "0.json"));
  REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "0.json.tmp"));
  REQUIRE(store.LoadAll().empty());
}

TEST_CASE("DocumentStore does not overwrite an existing document",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const Document original{1, "original.txt", "Original text."};
  store.Save(original);

  REQUIRE_THROWS_AS(
      store.Save(Document{1, "replacement.txt", "Replacement text."}),
      StorageError);

  CheckDocument(store.Get(1), original);
  REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "1.json.tmp"));
}

TEST_CASE("DocumentStore leaves only a final file after saving",
          "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  store.Save(Document{5, "five.txt", "hello"});

  const fs::path documents_dir = temp.Path() / "documents";
  REQUIRE(fs::is_regular_file(documents_dir / "5.json"));
  REQUIRE(fs::file_size(documents_dir / "5.json") > 0);
  REQUIRE_FALSE(fs::exists(documents_dir / "5.json.tmp"));
}

TEST_CASE("DocumentStore reports a missing document as StorageError",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());

  REQUIRE_THROWS_AS(store.Get(123), StorageError);
}

TEST_CASE("DocumentStore rejects malformed JSON and non-object roots",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path path = temp.Path() / "documents" / "1.json";

  for (const std::string_view contents :
       {"", "{", "this is not JSON", "[]", "null", "42"}) {
    CAPTURE(contents);
    WriteFile(path, contents);
    REQUIRE_THROWS_AS(store.Get(1), StorageError);
  }
}

TEST_CASE("DocumentStore validates schema_version",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path path = temp.Path() / "documents" / "1.json";

  SECTION("Missing version") {
    WriteFile(path, R"({"document_id":1,"title":"example.txt","text":"hello"})");
    REQUIRE_THROWS_AS(store.Get(1), StorageError);
  }

  SECTION("Unsupported version or wrong type") {
    for (const std::string_view version :
         {"0", "2", "-1", "1.0", R"("1")", "true", "null"}) {
      CAPTURE(version);
      WriteFile(path, StoredJson("1", version));
      REQUIRE_THROWS_AS(store.Get(1), StorageError);
    }
  }
}

TEST_CASE("DocumentStore validates the stored document ID",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path path = temp.Path() / "documents" / "1.json";

  SECTION("Missing ID") {
    WriteFile(path, R"({"schema_version":1,"title":"example.txt","text":"hello"})");
    REQUIRE_THROWS_AS(store.Get(1), StorageError);
  }

  SECTION("Zero, negative, wrong type, overflow, or mismatch") {
    for (const std::string_view id :
         {"0", "-1", "1.0", R"("1")", "true", "null",
          "18446744073709551616", "2"}) {
      CAPTURE(id);
      WriteFile(path, StoredJson(id));
      REQUIRE_THROWS_AS(store.Get(1), StorageError);
    }
  }

  SECTION("A negative ID must not wrap into a large positive ID") {
    const DocumentId largest = std::numeric_limits<DocumentId>::max();
    WriteFile(temp.Path() / "documents" /
                  (std::to_string(largest) + ".json"),
              StoredJson("-1"));
    REQUIRE_THROWS_AS(store.Get(largest), StorageError);
  }
}

TEST_CASE("DocumentStore validates title and text",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path path = temp.Path() / "documents" / "1.json";

  SECTION("Missing title") {
    WriteFile(path, R"({"schema_version":1,"document_id":1,"text":"hello"})");
    REQUIRE_THROWS_AS(store.Get(1), StorageError);
  }

  SECTION("Missing text") {
    WriteFile(path, R"({"schema_version":1,"document_id":1,"title":"example.txt"})");
    REQUIRE_THROWS_AS(store.Get(1), StorageError);
  }

  SECTION("Non-string fields") {
    for (const std::string_view value : {"123", "true", "null", "[]", "{}"}) {
      CAPTURE(value);

      WriteFile(path, StoredJson("1", "1", value));
      REQUIRE_THROWS_AS(store.Get(1), StorageError);

      WriteFile(path, StoredJson("1", "1", R"("example.txt")", value));
      REQUIRE_THROWS_AS(store.Get(1), StorageError);
    }
  }
}

TEST_CASE("DocumentStore supports the largest DocumentId", "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const Document original{
      std::numeric_limits<DocumentId>::max(), "max.txt", "Largest valid ID."};

  store.Save(original);

  CheckDocument(store.Get(original.id), original);
  const auto documents = store.LoadAll();
  REQUIRE(documents.size() == 1);
  CheckDocument(documents.front(), original);
}

TEST_CASE("DocumentStore loads documents in numeric ID order",
          "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());

  store.Save(Document{42, "forty-two.txt", "forty-two"});
  store.Save(Document{10, "ten.txt", "ten"});
  store.Save(Document{2, "two.txt", "two"});
  store.Save(Document{1, "one.txt", "one"});

  const auto documents = store.LoadAll();
  REQUIRE(documents.size() == 4);
  CheckDocument(documents[0], Document{1, "one.txt", "one"});
  CheckDocument(documents[1], Document{2, "two.txt", "two"});
  CheckDocument(documents[2], Document{10, "ten.txt", "ten"});
  CheckDocument(documents[3], Document{42, "forty-two.txt", "forty-two"});
}

TEST_CASE("DocumentStore ignores temporary files and unrelated entries",
          "[document_store]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path documents_dir = temp.Path() / "documents";
  const Document original{1, "one.txt", "one"};
  store.Save(original);

  WriteFile(documents_dir / "2.json.tmp", "{unfinished JSON");
  WriteFile(documents_dir / "notes.txt", "not JSON");
  fs::create_directory(documents_dir / "3.json");
  WriteFile(documents_dir / "3.json" / "4.json", StoredJson("4"));

  const auto documents = store.LoadAll();
  REQUIRE(documents.size() == 1);
  CheckDocument(documents.front(), original);
}

TEST_CASE("DocumentStore rejects invalid final filenames",
          "[document_store][validation]") {
  for (const std::string_view filename :
       {"word.json", "0.json", "-1.json", "+1.json", "01.json", "1x.json",
        "18446744073709551616.json"}) {
    CAPTURE(filename);
    TempDirectory temp;
    DocumentStore store(temp.Path());
    WriteFile(temp.Path() / "documents" / std::string(filename), StoredJson());

    REQUIRE_THROWS_AS(store.LoadAll(), StorageError);
  }
}

TEST_CASE("DocumentStore does not skip corrupt final files",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  store.Save(Document{2, "two.txt", "A valid document."});

  SECTION("Malformed JSON") {
    WriteFile(temp.Path() / "documents" / "1.json", "{broken");
  }

  SECTION("Unsupported schema") {
    WriteFile(temp.Path() / "documents" / "1.json", StoredJson("1", "2"));
  }

  SECTION("Filename and stored ID disagree") {
    WriteFile(temp.Path() / "documents" / "1.json", StoredJson("2"));
  }

  REQUIRE_THROWS_AS(store.LoadAll(), StorageError);
}

TEST_CASE("DocumentStore reports unavailable storage as StorageError",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const fs::path documents_dir = temp.Path() / "documents";
  fs::remove_all(documents_dir);

  REQUIRE_THROWS_AS(store.Save(Document{1, "one.txt", "one"}), StorageError);
  REQUIRE_THROWS_AS(store.Get(1), StorageError);
  REQUIRE_THROWS_AS(store.LoadAll(), StorageError);
  REQUIRE_FALSE(fs::exists(documents_dir));
}

TEST_CASE("DocumentStore does not publish a document after serialization fails",
          "[document_store][validation]") {
  TempDirectory temp;
  DocumentStore store(temp.Path());
  const Document invalid{1, "invalid.txt", std::string(1, '\xFF')};

  REQUIRE_THROWS_AS(store.Save(invalid), StorageError);
  REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "1.json"));
  REQUIRE_FALSE(fs::exists(temp.Path() / "documents" / "1.json.tmp"));
  REQUIRE(store.LoadAll().empty());
}

TEST_CASE("Temporary test directories are removed automatically",
          "[document_store][cleanup]") {
  fs::path path;

  SECTION("Normal scope exit") {
    {
      TempDirectory temp;
      path = temp.Path();
      DocumentStore store(path);
      store.Save(Document{1, "one.txt", "one"});
      REQUIRE(fs::exists(path / "documents" / "1.json"));
    }
    REQUIRE_FALSE(fs::exists(path));
  }

  SECTION("Exception unwinding") {
    bool caught = false;
    try {
      TempDirectory temp;
      path = temp.Path();
      DocumentStore store(path);
      store.Save(Document{1, "one.txt", "one"});
      throw std::runtime_error("Simulate test interruption");
    } catch (const std::runtime_error&) {
      caught = true;
    }

    REQUIRE(caught);
    REQUIRE_FALSE(path.empty());
    REQUIRE_FALSE(fs::exists(path));
  }
}
