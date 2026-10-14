#include "health_handler.hpp"

#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>

namespace local_search::http {

std::string HealthHandler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& response = request.GetHttpResponse();

    response.SetStatus(userver::server::http::HttpStatus::kOk);
    response.SetContentType(userver::http::content_type::kTextPlain);

    return "ok\n";
}

}  // namespace local_search::http