#pragma once

#include "search/search_engine.hpp"
#include "storage/document_store.hpp"
#include "grpc_embedder.hpp"
#include "vector_search/vector_search_engine.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "embedding_client.usrv.pb.hpp"

#include <userver/ugrpc/client/simple_client_component.hpp>
#include <stdexcept>


#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/engine/task/task_processor_fwd.hpp>
#include <userver/yaml_config/schema.hpp>

namespace local_search {

using EmbeddingClientComponent =
userver::ugrpc::client::SimpleClientComponent<
    embedding::v1::EmbeddingServiceClient>;

class SearchComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "search-component";

    std::vector<VectorSearchResult> SearchVector(
    std::string query, std::size_t limit);

    SearchComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    AddedDocument AddDocument(std::string title, std::string text);
    std::vector<SearchResult> Search(std::string query, std::size_t limit);
    std::vector<VectorSearchResult> SearchVector(
    std::string query, std::size_t limit);
    
    Document GetDocument(DocumentId id);
    DocumentWithMatches GetDocumentWithMatches(DocumentId id, std::string query);

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    userver::engine::TaskProcessor& fs_task_processor_;
    DocumentStore store_;
    SearchEngine engine_;

    GrpcEmbedder embedder_;
    VectorSearchEngine vector_engine_;

    userver::engine::Mutex mutex_;
};

}  // namespace local_search
