#pragma once

#include "embedding.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace local_search {

class Embedder {
public:
    virtual ~Embedder() = default;

    virtual Embedding EmbedQuery(std::string_view query) = 0;

    virtual std::vector<Embedding> EmbedPassages(
        std::span<const std::string_view> passages) = 0;
};

} // namespace local_search