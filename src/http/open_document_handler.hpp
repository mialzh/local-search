#pragma once

#include "api_error.hpp"
#include "components/search_component.hpp"

namespace local_search::http {
class OpenDocumentHandler final : public ApiHandlerBase {
public:
    static constexpr std::string_view kName = "handler-open-document";

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};  

} //namespace local_search::http