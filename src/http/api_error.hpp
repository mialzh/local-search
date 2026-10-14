#pragma once

#include <string>
#include <string_view>

#include <userver/formats/json/value.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_status.hpp>

namespace local_search::http {

[[noreturn]] void ThrowApiError(
    userver::server::http::HttpStatus status,
    std::string code,
    std::string message
);

std::string ReplyJson(
    const userver::server::http::HttpRequest& request,
    userver::server::http::HttpStatus status,
    const userver::formats::json::Value& value
);

class ApiHandlerBase : public userver::server::handlers::HttpHandlerBase {
public:
    using HttpHandlerBase::HttpHandlerBase;

    userver::server::handlers::FormattedErrorData GetFormattedExternalErrorBody(
        const userver::server::handlers::CustomHandlerException& error
    ) const override;
};

}  // namespace local_search::http
