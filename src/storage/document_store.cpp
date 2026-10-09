#include <algorithm>
#include <charconv>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <system_error>

#include "document_store.hpp"
#include "search/errors.hpp"

namespace local_search {
DocumentStore::DocumentStore(std::filesystem::path data_dir)
    : documents_dir_(data_dir / "documents") {
  try {
    std::filesystem::create_directories(documents_dir_);
  } catch (const std::filesystem::filesystem_error &error) {
    throw StorageError(error.what());
  }
}

void DocumentStore::Save(const Document &document) {

  auto filename = std::to_string(document.id) + ".json";
  auto final_path = documents_dir_ / filename;
  auto temporary_path = documents_dir_ / (filename + ".tmp");
  try {
    if (document.id == 0) {
      throw StorageError("Invalid document ID");
    }
    if (std::filesystem::exists(final_path)) {
      throw StorageError("Document already exists");
    }
    nlohmann::json data = {{"schema_version", 1},
                           {"document_id", document.id},
                           {"title", document.title},
                           {"text", document.text}};
    std::string json_text = data.dump(2);
    std::ofstream file(temporary_path, std::ios::binary);
    if (!file) {
      throw StorageError("Cannot open temporary file");
    }
    file << json_text;
    file.close();
    if (!file) {
      throw StorageError("Cannot write or close temporary file");
    }
    std::filesystem::rename(temporary_path, final_path);
  } catch (const std::exception &error) {
    std::error_code ignored;
    std::filesystem::remove(temporary_path, ignored);
    throw StorageError(error.what());
  }
};

Document DocumentStore::Get(DocumentId id) const {
  if (id == 0) {
    throw StorageError("Invalid document ID");
  }
  auto filename = std::to_string(id) + ".json";
  auto file_path = documents_dir_ / filename;
  try {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
      throw StorageError("Cannot open document file");
    }
    nlohmann::json data = nlohmann::json::parse(file);
    const auto &version = data.at("schema_version");
    if (!version.is_number_integer() || version != 1) {
      throw StorageError("Unsupported schema version");
    }
    file.close();
    if (!file) {
      throw StorageError("Cannot read or close document file");
    }
    const auto &json_id = data.at("document_id");
    if (!json_id.is_number_unsigned()) {
      throw StorageError("Invalid document ID");
    }

    DocumentId stored_id = json_id.get<DocumentId>();
    if (stored_id == 0) {
      throw StorageError("Invalid document ID");
    }
    if (stored_id != id) {
      throw StorageError("Document ID mismatch");
    }
    if (!data.at("title").is_string()) {
      throw std::invalid_argument("invalid document title");
    }
    std::string title = data.at("title").get<std::string>();
    if (!data.at("text").is_string()) {
      throw std::invalid_argument("document text is not string");
    }
    std::string text = data.at("text").get<std::string>();
    return Document{stored_id, title, text};
  } catch (const std::exception &error) {
    throw StorageError(error.what());
  }
}

std::vector<Document> DocumentStore::LoadAll() const {
  try {
    std::vector<Document> result;
    for (const auto &entry :
         std::filesystem::directory_iterator(documents_dir_)) {

      if (!entry.is_regular_file() || entry.path().extension() != ".json") {
        continue;
      }

      const std::string name = entry.path().stem().string(); // "42"
      DocumentId id = 0;

      const char *begin = name.data();
      const char *end = begin + name.size();
      const auto [ptr, error] = std::from_chars(begin, end, id);

      if (error != std::errc{} || ptr != end || id == 0 ||
          name != std::to_string(id)) {
        throw StorageError("Invalid document filename");
      }

      result.push_back(Get(id));
    }
    std::sort(result.begin(), result.end(),
              [](const Document &lhs, const Document &rhs) {
                return lhs.id < rhs.id;
              });
    return result;
  } catch (const std::exception &error) {
    throw StorageError(error.what());
  }
}
} // namespace local_search
