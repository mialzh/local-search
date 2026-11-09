#include "open_document_handler.hpp"
#include "search/document.hpp"

#include <userver/server/http/http_request.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <charconv>
#include <system_error>
#include <stdexcept>
#include <utility>

namespace local_search::http {


OpenDocumentHandler::OpenDocumentHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ApiHandlerBase(config, context),
      search_(context.FindComponent<SearchComponent>()) {}

std::string OpenDocumentHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    const auto& id_text = request.GetPathArg("document_id");
    DocumentId id = 0;
    const char* begin = id_text.data();
    const char* end = begin + id_text.size();
    const auto [ptr, error] = std::from_chars(begin, end, id);

    if (error != std::errc{} || ptr != end || id == 0) {
        ThrowApiError(
            userver::server::http::HttpStatus::kBadRequest,
            "InvalidDocumentId",
            "Document ID must be a positive integer"
        );
    }
    const auto& queries = request.GetArgVector("q");
    if (queries.size() > 1 ||
        (!queries.empty() && queries.front().size() > 2048)) {
        ThrowApiError(
            userver::server::http::HttpStatus::kBadRequest,
            "InvalidQuery", "Provide at most one q parameter, up to 2048 bytes"
        );
    }

    try {
        const auto result = search_.GetDocumentWithMatches(
            id, queries.empty() ? std::string{} : queries.front());
        const auto& document = result.document;
        userver::formats::json::ValueBuilder response;
        response["document_id"] = document.id;
        response["title"] = document.title;
        response["text"] = document.text;

        userver::formats::json::ValueBuilder matched_words(
            userver::formats::common::Type::kArray
        );
        for (const auto& word : result.matched_words) {
            matched_words.PushBack(word);
        }
        response["matched_words"] = std::move(matched_words);

        return ReplyJson(
            request,
            userver::server::http::HttpStatus::kOk,
            response.ExtractValue()
        );
    } catch (const ValidationError& error) {
        ThrowApiError(userver::server::http::HttpStatus::kBadRequest,
                      "InvalidQuery", error.what());
    } catch (const std::invalid_argument& error) {
        ThrowApiError(userver::server::http::HttpStatus::kBadRequest,
                      "InvalidQuery", error.what());
    }
}

}  // namespace local_search::http
