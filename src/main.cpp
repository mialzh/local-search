#include "components/search_component.hpp"
#include "http/health_handler.hpp"
#include "http/search_handler.hpp"
#include "http/upload_handler.hpp"
#include "http/open_document_handler.hpp"

#include <userver/components/fs_cache.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/http_handler_static.hpp>
#include <userver/utils/daemon_run.hpp>

int main(int argc, char* argv[]) {
    auto component_list =
        userver::components::MinimalServerComponentList()
            .Append<local_search::SearchComponent>()
            .Append<local_search::http::HealthHandler>()
            .Append<local_search::http::SearchHandler>()
            .Append<local_search::http::UploadHandler>()
            .Append<local_search::http::OpenDocumentHandler>()
            .Append<userver::components::FsCache>("fs-cache-web")
            .Append<userver::server::handlers::HttpHandlerStatic>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
