#pragma once
#include "document.hpp"
#include <string>

namespace local_search {

struct SearchResult {
  DocumentId document_id;
  std::string title;
  double score;
  std::string snippet;
};

} // namespace local_search