#pragma once

#include <string>
#include <string_view>

#include <userver/server/handlers/http_handler_base.hpp>

namespace local_search::http {

class HealthHandler final
    : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-health";

    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

}  // namespace local_search::http