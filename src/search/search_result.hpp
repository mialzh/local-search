#pragma once
#include "document.hpp"

namespace local_search {

struct SearchResult {
  DocumentId document_id;
  std::string title;
  double score;
  std::string snippet;
};

} // namespace local_search