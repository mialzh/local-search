#pragma once

#include "search/document.hpp"

#include <filesystem>
#include <vector>

namespace local_search {
class DocumentStore {
public:
  explicit DocumentStore(std::filesystem::path data_dir);

  void Save(const Document &document);
  Document Get(DocumentId id) const;
  std::vector<Document> LoadAll() const;

private:
  std::filesystem::path documents_dir_;
};

} // namespace local_search
