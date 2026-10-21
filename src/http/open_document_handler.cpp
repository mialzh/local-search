#include "open_document_handler.hpp"
#include "search/document.hpp"

#include <userver/server/http/http_request.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <charconv>
#include <system_error>

namespace local_search::http {


OpenDocumentHandler::OpenDocumentHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ApiHandlerBase(config, context),
      search_(context.FindComponent<SearchComponent>()) {}

std::string OpenDocumentHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context
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
    const auto document = search_.GetDocument(id);
    userver::formats::json::ValueBuilder response;

    response["document_id"] = document.id;
    response["title"] = document.title;
    response["text"] = document.text;

    return ReplyJson(
        request,
        userver::server::http::HttpStatus::kOk,
        response.ExtractValue()
    );
}

}  // namespace local_search::http