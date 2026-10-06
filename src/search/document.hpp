#pragma once
#include <cstdint>
#include <string>

namespace local_search {
using DocumentId = std::uint64_t;

struct Document {
  DocumentId id;
  std::string title;
  std::string text;
};
} // namespace local_search
