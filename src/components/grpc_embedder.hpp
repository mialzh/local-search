#pragma once

#include "vector_search/embedder.hpp"

namespace local_search::embedding::v1 {
class EmbeddingServiceClient;
}

namespace local_search {

class GrpcEmbedder final : public Embedder {
public:
    explicit GrpcEmbedder(embedding::v1::EmbeddingServiceClient& client)
        : client_(client) {}

    Embedding EmbedQuery(std::string_view query) override;
    std::vector<Embedding> EmbedPassages(
        std::span<const std::string_view> passages) override;

private:
    embedding::v1::EmbeddingServiceClient& client_;
};

} // namespace local_search