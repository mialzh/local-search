#include "search_component.hpp"

#include "search/errors.hpp"
#include "search/text_processor.hpp"

#include <filesystem>
#include <mutex>
#include <utility>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/utils/async.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace local_search {

SearchComponent::SearchComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ComponentBase(config, context),
      fs_task_processor_(context.GetTaskProcessor(
          config["fs-task-processor"].As<std::string>()
      )),
      store_(userver::utils::Async(
          fs_task_processor_,
          "create-document-store",
          [directory = config["data-dir"].As<std::string>()] {
              return DocumentStore(std::filesystem::path(directory));
          }
      ).Get()),
      engine_(store_),
      embedder_(
          context.FindComponent<EmbeddingClientComponent>(
              "embedding-client"
          ).GetClient()
      ),
      vector_engine_(store_, embedder_){
    userver::utils::Async(
        fs_task_processor_,
        "restore-search-index",
        [this] {
            engine_.Restore();
            vector_engine_.Restore();
        }
    ).Get();
}

AddedDocument SearchComponent::AddDocument(
    std::string title, std::string text
) {
    return userver::utils::Async(
        fs_task_processor_,
        "add-document",
        [this, title = std::move(title), text = std::move(text)]() mutable {
            // Validate before writing anything to storage.
            if (TextProcessor::Process(text).empty()) {
                throw ValidationError(
                    "InvalidText", "Document must contain at least one word"
                );
            }

            std::lock_guard lock(mutex_);

            // Keep text available for vector indexing.
            auto added = engine_.AddDocument(std::move(title), text);

            try {
                vector_engine_.IndexDocument(added.document_id, text);
            } catch (...) {
                // Roll back the document and rebuild the BM25 index.
                store_.Remove(added.document_id);
                engine_.Restore();
                throw;
            }

            return added;
        }
    ).Get();
}

std::vector<SearchResult> SearchComponent::Search(
    std::string query, std::size_t limit
) {
    return userver::utils::Async(
        fs_task_processor_,
        "search-documents",
        [this, query = std::move(query), limit]() mutable {
            std::lock_guard lock(mutex_);
            return engine_.Search(std::move(query), limit);
        }
    ).Get();
}

std::vector<VectorSearchResult> SearchComponent::SearchVector(
    std::string query, std::size_t limit
) {
    return userver::utils::Async(
        fs_task_processor_,
        "vector-search-documents",
        [this, query = std::move(query), limit] {
            if (limit == 0 || limit > 100) {
                throw std::invalid_argument(
                    "Limit must be between 1 and 100"
                );
            }

            if (TextProcessor::Process(query).empty()) {
                throw ValidationError(
                    "InvalidQuery",
                    "Query must contain at least one word"
                );
            }

            std::lock_guard lock(mutex_);
            return vector_engine_.Search(query, limit);
        }
    ).Get();
}

Document SearchComponent::GetDocument(DocumentId id) {
    return userver::utils::Async(
        fs_task_processor_,
        "get-document",
        [this, id] {
            std::lock_guard lock(mutex_);
            return store_.Get(id);
        }
    ).Get();
}

DocumentWithMatches SearchComponent::GetDocumentWithMatches(
    DocumentId id, std::string query
) {
    return userver::utils::Async(
        fs_task_processor_,
        "get-document-with-matches",
        [this, id, query = std::move(query)]() mutable {
            std::lock_guard lock(mutex_);
            return engine_.GetDocumentWithMatches(id, std::move(query));
        }
    ).Get();
}

userver::yaml_config::Schema SearchComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<
        userver::components::ComponentBase
    >(R"(
type: object
description: Local search engine
additionalProperties: false
properties:
    data-dir:
        type: string
        description: Directory containing stored documents
    fs-task-processor:
        type: string
        description: Task processor for blocking filesystem operations
)");
}

}  // namespace local_search
