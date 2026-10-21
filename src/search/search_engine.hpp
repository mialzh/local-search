#pragma once

#include "bm25.hpp"
#include "document.hpp"
#include "errors.hpp"
#include "inverted_index.hpp"
#include "search_result.hpp"
#include "storage/document_store.hpp"
#include "text_processor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace local_search {
struct AddedDocument {
  DocumentId document_id;
  std::string title;
  std::size_t word_count;
};

class SearchEngine {
public:
  explicit SearchEngine(DocumentStore &store);

  void Restore();

  AddedDocument AddDocument(std::string title, std::string text);

  std::vector<SearchResult> Search(std::string query, std::size_t limit) const;

  Document GetDocument(DocumentId document_id) const;
private:
  DocumentStore &store_;
  InvertedIndex index_;
  DocumentId next_document_id_ = 1;
};
} // namespace local_search