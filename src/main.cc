#include <drogon/drogon.h>
#include <stdexcept>

#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/projects.h"
#include "log/log.h"
#include "service/github.h"
#include "service/platforms.h"
#include "service/schemas.h"
#include "service/util.h"

using namespace std;
using namespace drogon;
using namespace logging;

int main() {
    constexpr auto port(8080);
    logger.info("Starting wiki service server on port {}", port);

    try {
        app().loadConfigFile("config.json");

        const auto level = trantor::Logger::logLevel();
        app().setLogLevel(level).addListener("0.0.0.0", port).setThreadNum(16);
        configureLoggingLevel();

        const Json::Value &customConfig = app().getCustomConfig();

        if (const auto error = validateJson(schemas::systemConfig, customConfig)) {
            logger.error("App config validation failed at {}: {}", error->pointer.to_string(), error->msg);
            throw std::runtime_error("Invalid configuration");
        }

        const Json::Value &githubAppConfig = customConfig["github_app"];
        const std::string &appName = githubAppConfig["name"].asString();
        const std::string &appClientId = githubAppConfig["client_id"].asString();
        const std::string &appPrivateKeyPath = githubAppConfig["private_key_path"].asString();
        const std::string &curseForgeKey = customConfig["curseforge_key"].asString();
        const Json::Value &mrApp = customConfig["modrinth_app"];
        const std::string &mrAppClientId = mrApp["client_id"].asString();
        const std::string &mrAppClientSecret = mrApp["client_secret"].asString();
        const std::string &mrAppRedirectUrl = mrApp["redirect_url"].asString();

        if (!customConfig.isMember("api_key") || customConfig["api_key"].asString().empty()) {
            logger.warn("No API key configured, allowing public API access.");
        }

        auto cache(service::MemoryCache{});
        auto github(service::GitHub{cache, appName, appClientId, appPrivateKeyPath});
        auto database(service::Database{});
        auto documentation(service::Documentation{github, cache});

        auto modrinth(service::ModrinthPlatform{mrAppClientId, mrAppClientSecret, mrAppRedirectUrl});
        auto curseForge(service::CurseForgePlatform{curseForgeKey});
        auto platforms(service::Platforms(curseForge, modrinth));

        auto controller(make_shared<api::v1::DocsController>(github, database, documentation));
        auto browseController(make_shared<api::v1::BrowseController>(database));
        auto projectsController(make_shared<api::v1::ProjectsController>(github, platforms, database, documentation));

        app().registerController(controller);
        app().registerController(browseController);
        app().registerController(projectsController);

        app().run();
    } catch (const std::exception &e) {
        logger.critical("Error running app: {}", e.what());
    }

    return 0;
}
