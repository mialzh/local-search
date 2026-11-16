#pragma once

#include "search/document.hpp"

#include <string>

namespace local_search {

struct VectorSearchResult {
    DocumentId document_id;
    std::string title;
    double distance;
    std::string snippet;
};

} // namespace local_search