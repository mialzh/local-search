#pragma once

#include "api_error.hpp"
#include "components/search_component.hpp"

namespace local_search::http {
class OpenDocumentHandler final : public ApiHandlerBase {
public:
    static constexpr std::string_view kName = "handler-open-document";

    OpenDocumentHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
private:
    SearchComponent& search_;
};  

} //namespace local_search::http