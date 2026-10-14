#include "api_error.hpp"

#include <utility>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/http/http_error.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>

namespace local_search::http {

namespace {
using HttpStatus = userver::server::http::HttpStatus;
namespace handlers = userver::server::handlers;
namespace json = userver::formats::json;

json::Value MakeError(std::string_view code, std::string_view message) {
    json::ValueBuilder value;
    value["error"]["code"] = code;
    value["error"]["message"] = message;
    return value.ExtractValue();
}
}  // namespace

[[noreturn]] void ThrowApiError(
    HttpStatus status, std::string code, std::string message
) {
    const auto handler_code = static_cast<int>(status) >= 500
        ? handlers::HandlerErrorCode::kServerSideError
        : handlers::HandlerErrorCode::kClientError;

    throw userver::server::http::CustomHandlerException(
        handler_code,
        status,
        handlers::ServiceErrorCode{std::move(code)},
        handlers::ExternalBody{std::move(message)}
    );
}

std::string ReplyJson(
    const userver::server::http::HttpRequest& request,
    HttpStatus status,
    const json::Value& value
) {
    auto& response = request.GetHttpResponse();
    response.SetStatus(status);
    response.SetContentType(userver::http::content_type::kApplicationJson);
    return json::ToString(value);
}

handlers::FormattedErrorData ApiHandlerBase::GetFormattedExternalErrorBody(
    const handlers::CustomHandlerException& error
) const {
    const auto status = userver::server::http::GetHttpStatus(error);
    std::string code = error.GetServiceCode();
    std::string message = error.GetExternalErrorBody();

    if (static_cast<int>(status) >= 500) {
        code = "InternalError";
        message = "Internal server error";
    } else if (status == HttpStatus::kPayloadTooLarge) {
        code = "DocumentTooLarge";
        message = "Document or request body is too large";
    } else if (code.empty()) {
        if (HandlerName() == "handler-search") {
            code = "InvalidQuery";
            message = "Invalid search request";
        } else {
            code = "InvalidRequest";
            message = "Invalid upload request";
        }
    }

    return {
        json::ToString(MakeError(code, message)),
        userver::http::content_type::kApplicationJson
    };
}

}  // namespace local_search::http
