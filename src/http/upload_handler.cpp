#include "upload_handler.hpp"

#include "components/search_component.hpp"
#include "search/errors.hpp"
#include "search/text_processor.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/form_data_arg.hpp>
#include <userver/server/http/http_request.hpp>

namespace local_search::http {

namespace {
using HttpStatus = userver::server::http::HttpStatus;

constexpr std::size_t kMaxFileSize = 5 * 1024 * 1024;
constexpr std::size_t kMaxBodySize = 6 * 1024 * 1024;
constexpr std::size_t kMaxTitleSize = 255;

std::string GetTitle(const std::string& filename) {
    const auto separator = filename.find_last_of("/\\");
    std::string title = filename.substr(
        separator == std::string::npos ? 0 : separator + 1
    );

    if (title.empty() || title.size() > kMaxTitleSize) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidRequest",
            "Filename must contain a non-empty title up to 255 bytes"
        );
    }

    try {
        (void)TextProcessor::Normalize(title);
    } catch (const ValidationError&) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidRequest",
            "Filename must be valid UTF-8 without U+0000"
        );
    }

    if (title.size() < 4) {
        ThrowApiError(
            HttpStatus::kUnsupportedMediaType, "UnsupportedFileType",
            "Only .txt files are supported"
        );
    }

    std::string extension = title.substr(title.size() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](char ch) {
            return ch >= 'A' && ch <= 'Z'
                ? static_cast<char>(ch - 'A' + 'a') : ch;
        });

    if (extension != ".txt") {
        ThrowApiError(
            HttpStatus::kUnsupportedMediaType, "UnsupportedFileType",
            "Only .txt files are supported"
        );
    }

    return title;
}
}  // namespace

UploadHandler::UploadHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ApiHandlerBase(config, context),
      search_(context.FindComponent<SearchComponent>()) {}

std::string UploadHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    if (request.RequestBody().size() > kMaxBodySize) {
        ThrowApiError(
            HttpStatus::kPayloadTooLarge, "DocumentTooLarge",
            "Request body must not exceed 6 MiB"
        );
    }

    const auto& files = request.GetFormDataArgVector("file");
    if (request.FormDataArgCount() != 1 || files.size() != 1 ||
        !files.front().filename) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidRequest",
            "Provide exactly one multipart file named file"
        );
    }

    const auto& file = files.front();
    std::string title = GetTitle(*file.filename);

    // The size limit applies before stripping the optional UTF-8 BOM.
    if (file.value.size() > kMaxFileSize) {
        ThrowApiError(
            HttpStatus::kPayloadTooLarge, "DocumentTooLarge",
            "Document must not exceed 5 MiB"
        );
    }

    std::string text(file.value);
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.erase(0, 3);
    }

    try {
        const auto added = search_.AddDocument(
            std::move(title), std::move(text)
        );

        userver::formats::json::ValueBuilder response;
        response["document_id"] = added.document_id;
        response["title"] = added.title;
        response["word_count"] = added.word_count;

        return ReplyJson(request, HttpStatus::kCreated, response.ExtractValue());
    } catch (const ValidationError& error) {
        ThrowApiError(HttpStatus::kBadRequest, "InvalidText", error.what());
    }
    // Storage errors and other unexpected exceptions are handled by userver
    // and ApiHandlerBase as a JSON 500 InternalError.
}

}  // namespace local_search::http
