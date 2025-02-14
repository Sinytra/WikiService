#include "api/v1/auth.h"
#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/game.h"
#include "api/v1/projects.h"
#include "api/v1/websocket.h"

#include "config.h"
#include <service/database.h>
#include <service/github.h>
#include <service/cloudflare.h>

#include <git2.h>
#include <log/log.h>

using namespace std;
using namespace drogon;
using namespace logging;
using namespace service;

namespace service {
    trantor::EventLoopThreadPool cacheAwaiterThreadPool{1};
}

namespace global {
    std::shared_ptr<Database> database;
    std::shared_ptr<MemoryCache> cache;
    std::shared_ptr<GitHub> github;
    std::shared_ptr<RealtimeConnectionStorage> connections;
    std::shared_ptr<Storage> storage;
    std::shared_ptr<CloudFlare> cloudFlare;
    std::shared_ptr<Auth> auth;

    std::shared_ptr<Platforms> platforms;

    std::shared_ptr<content::Ingestor> ingestor;
}

int main() {
    constexpr auto port(8080);
    logger.info("Starting wiki service server on port {}", port);

    try {
        const auto level = trantor::Logger::logLevel();
        app().setLogLevel(level).addListener("0.0.0.0", port).setThreadNum(16);
        configureLoggingLevel();

        const auto [authConfig, githubAppConfig, mrApp, cloudFlareConfig, appUrl, curseForgeKey, storagePath] = config::configure();

        global::database = std::make_shared<Database>();
        global::cache = std::make_shared<MemoryCache>();
        global::github = std::make_shared<GitHub>();
        global::connections = std::make_shared<RealtimeConnectionStorage>();
        global::storage = std::make_shared<Storage>(storagePath);
        global::cloudFlare = std::make_shared<CloudFlare>(cloudFlareConfig);
        global::auth = std::make_shared<Auth>(appUrl, OAuthApp{githubAppConfig.clientId, githubAppConfig.clientSecret},
                                              OAuthApp{mrApp.clientId, mrApp.clientSecret});
        auto curseForge(CurseForgePlatform{curseForgeKey});
        auto modrinth(ModrinthPlatform{});
        global::platforms = std::make_shared<Platforms>(curseForge, modrinth);
        global::ingestor = std::make_shared<content::Ingestor>();

        auto authController(make_shared<api::v1::AuthController>(authConfig));
        auto controller(make_shared<api::v1::DocsController>());
        auto browseController(make_shared<api::v1::BrowseController>());
        auto projectsController(make_shared<api::v1::ProjectsController>());
        auto projectWSController(make_shared<api::v1::ProjectWebSocketController>());
        auto gameController(make_shared<api::v1::GameController>());

        app().registerController(authController);
        app().registerController(controller);
        app().registerController(browseController);
        app().registerController(projectsController);
        app().registerController(projectWSController);
        app().registerController(gameController);

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
