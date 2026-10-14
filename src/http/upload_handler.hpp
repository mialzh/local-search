#pragma once

#include "api_error.hpp"

#include <string>
#include <string_view>

namespace local_search {
class SearchComponent;
}

namespace local_search::http {

class UploadHandler final : public ApiHandlerBase {
public:
    static constexpr std::string_view kName = "handler-upload";

    UploadHandler(
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

}  // namespace local_search::http
