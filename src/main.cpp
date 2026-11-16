#include "embedding_client.usrv.pb.hpp"

#include <userver/testsuite/testsuite_support.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/client/component_list.hpp>
#include <userver/ugrpc/client/simple_client_component.hpp>

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
            .Append<userver::components::TestsuiteSupport>()
            .AppendComponentList(userver::ugrpc::client::MinimalComponentList())
            .Append<userver::ugrpc::client::ClientFactoryComponent>()
            .Append<userver::ugrpc::client::SimpleClientComponent<
                local_search::embedding::v1::EmbeddingServiceClient>>("embedding-client")
            .Append<local_search::SearchComponent>()
            .Append<local_search::http::HealthHandler>()
            .Append<local_search::http::SearchHandler>()
            .Append<local_search::http::UploadHandler>()
            .Append<local_search::http::OpenDocumentHandler>()
            .Append<userver::components::FsCache>("fs-cache-web")
            .Append<userver::server::handlers::HttpHandlerStatic>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
