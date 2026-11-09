#pragma once
#include "document.hpp"
#include <string>
#include <vector>

namespace local_search {

struct SearchResult {
  DocumentId document_id;
  std::string title;
  double score;
  std::string snippet;
  std::vector<std::string> matched_words{};
};

} // namespace local_search
