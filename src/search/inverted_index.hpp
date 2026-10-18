#pragma once

#include "document.hpp"
#include "errors.hpp"
#include "text_processor.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace local_search {
struct Posting {
  DocumentId document_id;
  std::vector<Occurrence> occurrences;
};

class InvertedIndex {
private:
  std::unordered_map<std::string, std::vector<Posting>> postings_;
  std::unordered_map<DocumentId, std::size_t> document_lengths_;
  std::size_t total_word_count_ = 0;

public:
  void AddDocument(DocumentId id, const std::vector<Token> &words);

  const std::vector<Posting> *Find(std::string_view word) const;

  std::size_t GetDocumentLength(DocumentId id) const;

  std::size_t DocumentCount() const;

  double AverageDocumentLength() const;

  void Swap(InvertedIndex &other) noexcept;
};
} // namespace local_search