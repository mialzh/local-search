#include "components/search_component.hpp"
#include "http/health_handler.hpp"
#include "http/search_handler.hpp"
#include "http/upload_handler.hpp"

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/utils/daemon_run.hpp>

int main(int argc, char* argv[]) {
    auto component_list =
        userver::components::MinimalServerComponentList()
            .Append<local_search::SearchComponent>()
            .Append<local_search::http::HealthHandler>()
            .Append<local_search::http::SearchHandler>()
            .Append<local_search::http::UploadHandler>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
