#include <drogon/drogon.h>

#include "api/v1/docs.h"
#include "api/v1/browse.h"
#include "log/log.h"
#include "service/github.h"

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

        if (!customConfig.isMember("api_key") || customConfig["api_key"].asString().empty()) {
            logger.warn("No API key configured, allowing public API access.");
        }

        auto cache(service::MemoryCache{});
        auto github(service::GitHub{cache, appClientId, appPrivateKeyPath});
        auto database(service::Database{});
        auto documentation(service::Documentation{github, cache});
        auto controller(make_shared<api::v1::DocsController>(github, database, documentation));
        auto browseController(make_shared<api::v1::BrowseController>(database));

        app().setLogLevel(trantor::Logger::kDebug).addListener("0.0.0.0", port).setThreadNum(16);

        app().registerController(controller);
        app().registerController(browseController);

        app().run();
    } catch (const std::exception &e) {
        logger.critical("Error running app: {}", e.what());
    }

    return 0;
}
