#include "search_handler.hpp"

#include "components/search_component.hpp"
#include "search/errors.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_request.hpp>

namespace local_search::http {

namespace {
using HttpStatus = userver::server::http::HttpStatus;

std::size_t GetLimit(const userver::server::http::HttpRequest& request) {
    const auto& limits = request.GetArgVector("limit");
    if (limits.empty()) {
        return 10;
    }

    if (limits.size() != 1) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidLimit",
            "Provide at most one limit parameter"
        );
    }

    const auto& text = limits.front();
    std::size_t limit = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, limit);

    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        limit < 1 || limit > 100) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidLimit",
            "Limit must be a decimal integer between 1 and 100"
        );
    }

    return limit;
}
}  // namespace

SearchHandler::SearchHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ApiHandlerBase(config, context),
      search_(context.FindComponent<SearchComponent>()) {}

std::string SearchHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    const auto& queries = request.GetArgVector("q");
    if (queries.size() != 1 || queries.front().empty() ||
        queries.front().size() > 2048) {
        ThrowApiError(
            HttpStatus::kBadRequest, "InvalidQuery",
            "Provide exactly one non-empty q parameter, up to 2048 bytes"
        );
    }

    const auto limit = GetLimit(request);

    try {
        const auto found = search_.Search(queries.front(), limit);
        userver::formats::json::ValueBuilder results(
            userver::formats::common::Type::kArray
        );

        for (const auto& result : found) {
            if (!std::isfinite(result.score)) {
                throw std::runtime_error("Search score is not finite");
            }

            userver::formats::json::ValueBuilder item;
            item["document_id"] = result.document_id;
            item["title"] = result.title;
            item["score"] = result.score;
            item["snippet"] = result.snippet;
            results.PushBack(std::move(item));
        }

        userver::formats::json::ValueBuilder response;
        response["results"] = std::move(results);
        return ReplyJson(request, HttpStatus::kOk, response.ExtractValue());
    } catch (const ValidationError& error) {
        ThrowApiError(HttpStatus::kBadRequest, "InvalidQuery", error.what());
    } catch (const std::invalid_argument& error) {
        ThrowApiError(HttpStatus::kBadRequest, "InvalidQuery", error.what());
    }
}

}  // namespace local_search::http
