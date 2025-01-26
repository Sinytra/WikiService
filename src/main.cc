#include <drogon/drogon.h>
#include <service/cloudflare.h>
#include <stdexcept>

#include "api/v1/auth.h"
#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/websocket.h"
#include "api/v1/projects.h"
#include "git2.h"
#include "log/log.h"
#include "service/github.h"
#include "service/platforms.h"
#include "service/schemas.h"
#include "service/storage.h"
#include "service/util.h"

using namespace std;
using namespace drogon;
using namespace logging;

namespace service {
    trantor::EventLoopThreadPool cacheAwaiterThreadPool{1};
}

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

        const std::string appUrl = customConfig["app_url"].asString();
        const std::string appFrontendUrl = customConfig["frontend_url"].asString();
        const Json::Value &authConfig = customConfig["auth"];
        const std::string authCallbackUrl = authConfig["callback_url"].asString();
        const Json::Value &githubAppConfig = customConfig["github_app"];
        const std::string &appName = githubAppConfig["name"].asString();
        const std::string &appClientId = githubAppConfig["client_id"].asString();
        const std::string &appClientSecret = githubAppConfig["client_secret"].asString();
        const std::string &appPrivateKeyPath = githubAppConfig["private_key_path"].asString();
        const std::string &curseForgeKey = customConfig["curseforge_key"].asString();
        const std::string &storageBasePath = customConfig["storage_path"].asString();
        const Json::Value &mrApp = customConfig["modrinth_app"];
        const std::string &mrAppClientId = mrApp["client_id"].asString();
        const std::string &mrAppClientSecret = mrApp["client_secret"].asString();
        const Json::Value &cloudFlareConfig = customConfig["cloudflare"];
        const std::string &cloudFlareToken = cloudFlareConfig["token"].asString();
        const std::string &cloudFlareAccTag = cloudFlareConfig["account_tag"].asString();
        const std::string &cloudFlareSiteTag = cloudFlareConfig["site_tag"].asString();

        if (!customConfig.isMember("api_key") || customConfig["api_key"].asString().empty()) {
            logger.warn("No API key configured, allowing public API access.");
        }

        auto database(Database{});
        auto cache(MemoryCache{});
        auto github(GitHub{cache, appName, appClientId, appPrivateKeyPath});
        auto connections(RealtimeConnectionStorage{});
        auto storage(Storage{storageBasePath, cache, connections});

        auto cloudflare(CloudFlare{cloudFlareToken, cloudFlareAccTag, cloudFlareSiteTag, cache});

        auto modrinth(ModrinthPlatform{});
        auto curseForge(CurseForgePlatform{curseForgeKey});
        auto platforms(Platforms(curseForge, modrinth));

        auto auth(Auth{appUrl, { appClientId, appClientSecret }, {mrAppClientId, mrAppClientSecret}, database, cache, platforms});

        auto authController(make_shared<api::v1::AuthController>(appFrontendUrl, authCallbackUrl, auth, github, cache, database));
        auto controller(make_shared<api::v1::DocsController>(github, database, storage));
        auto browseController(make_shared<api::v1::BrowseController>(database));
        auto projectsController(make_shared<api::v1::ProjectsController>(auth, github, platforms, database, storage, cloudflare));
        auto projectWSController(make_shared<api::v1::ProjectWebSocketController>(database, storage, connections, auth));

        app().registerController(authController);
        app().registerController(controller);
        app().registerController(browseController);
        app().registerController(projectsController);
        app().registerController(projectWSController);

        cacheAwaiterThreadPool.start();
        git_libgit2_init();

        app().run();

        git_libgit2_shutdown();
        cacheAwaiterThreadPool.getLoop(0)->quit();
        cacheAwaiterThreadPool.wait();
    } catch (const std::exception &e) {
        logger.critical("Error running app: {}", e.what());
    }

    return 0;
}
