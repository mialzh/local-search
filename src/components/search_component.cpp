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
      engine_(store_) {
    userver::utils::Async(
        fs_task_processor_,
        "restore-search-index",
        [this] { engine_.Restore(); }
    ).Get();
}

AddedDocument SearchComponent::AddDocument(std::string title, std::string text) {
    return userver::utils::Async(
        fs_task_processor_,
        "add-document",
        [this, title = std::move(title), text = std::move(text)]() mutable {
            // The current core does not reject empty tokens before writing.
            // Validate here so a rejected HTTP upload never reaches storage.
            if (TextProcessor::Process(text).empty()) {
                throw ValidationError(
                    "InvalidText", "Document must contain at least one word"
                );
            }

            std::lock_guard lock(mutex_);
            return engine_.AddDocument(std::move(title), std::move(text));
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
