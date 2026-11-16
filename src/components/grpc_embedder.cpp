#include "grpc_embedder.hpp"

#include "embedding_client.usrv.pb.hpp"

#include <userver/ugrpc/client/call_options.hpp>

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>

namespace local_search {
namespace {

USERVER_NAMESPACE::ugrpc::client::CallOptions MakeCallOptions() {
    USERVER_NAMESPACE::ugrpc::client::CallOptions options;
    options.SetTimeout(std::chrono::seconds{5});
    return options;
}

Embedding ConvertEmbedding(const embedding::v1::Embedding& response) {
    if (response.values_size() != static_cast<int>(kVectorSize)) {
        throw std::runtime_error("Unexpected embedding dimension");
    }

    Embedding result{};
    auto output = result.begin();
    for (float value : response.values()) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("Embedding contains a non-finite value");
        }
        *output++ = value;
    }
    return result;
}

} // namespace

Embedding GrpcEmbedder::EmbedQuery(std::string_view query) {
    embedding::v1::EmbedQueryRequest request;
    request.set_query(std::string{query});

    return ConvertEmbedding(client_.EmbedQuery(request, MakeCallOptions()));
}

std::vector<Embedding> GrpcEmbedder::EmbedPassages(
    std::span<const std::string_view> passages) {
    if (passages.empty()) {
        return {};
    }

    embedding::v1::EmbedPassagesRequest request;
    for (std::string_view passage : passages) {
        request.add_passages(std::string{passage});
    }

    const auto response = client_.EmbedPassages(request, MakeCallOptions());
    if (static_cast<std::size_t>(response.embeddings_size()) != passages.size()) {
        throw std::runtime_error("Unexpected number of passage embeddings");
    }

    std::vector<Embedding> result;
    result.reserve(passages.size());
    for (const auto& embedding : response.embeddings()) {
        result.push_back(ConvertEmbedding(embedding));
    }
    return result;
}

} // namespace local_search