#include <drogon/drogon.h>

#include "api/v1/controller.h"
#include "api/v1/docs.h"
#include "log/log.h"
#include "service/github.h"
#include "service/service_impl.h"

using namespace std;
using namespace drogon;
using namespace logging;

int main() {
    const auto port(8080);
    logger.info("Starting wiki service server on port {}", port);

    try {
        app().loadConfigFile("config.json");

        const Json::Value &customConfig = app().getCustomConfig();
        const Json::Value &githubAppConfig = customConfig["github_app"];
        const std::string &appClientId = githubAppConfig["client_id"].asString();
        const std::string &appPrivateKeyPath = githubAppConfig["private_key_path"].asString();

        auto cache(service::MemoryCache{});
        auto servis(service::ServiceImpl{});
        auto github(service::GitHub{cache, appClientId, appPrivateKeyPath});
        auto database(service::Database{});
        auto documentation(service::Documentation{github, cache});
        auto controller(make_shared<api::v1::DocsController>(servis, github, database, documentation));
        auto ctrl(make_shared<api::v1::HelloWorld>(servis, github, database, documentation));

        app().setLogLevel(trantor::Logger::kDebug).addListener("0.0.0.0", port).setThreadNum(16);

        app().registerController(controller);
        app().registerController(ctrl);

        app().run();
    } catch (const std::exception &e) {
        logger.critical("Error running app: {}", e.what());
    }

    return 0;
}
