#include <drogon/drogon.h>
#include <service/cloudflare.h>
#include <stdexcept>

#include "config.h"
#include "api/v1/auth.h"
#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/game.h"
#include "api/v1/projects.h"
#include "api/v1/websocket.h"
#include "git2.h"
#include "log/log.h"
#include "service/content/game_content.h"
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
        const auto level = trantor::Logger::logLevel();
        app().setLogLevel(level).addListener("0.0.0.0", port).setThreadNum(16);
        configureLoggingLevel();

        const auto [authConfig, githubAppConfig, mrApp, cloudFlareConfig, appUrl, curseForgeKey, storagePath] =
            config::configure();

        auto database(Database{});
        auto cache(MemoryCache{});
        auto github(GitHub{});
        auto connections(RealtimeConnectionStorage{});
        auto ingestor(content::Ingestor{database});
        auto storage(Storage{storagePath, cache, connections, ingestor});
        auto cloudflare(CloudFlare{cloudFlareConfig, cache});
        auto modrinth(ModrinthPlatform{});
        auto curseForge(CurseForgePlatform{curseForgeKey});
        auto platforms(Platforms(curseForge, modrinth));
        auto auth(Auth{appUrl,
                       {githubAppConfig.clientId, githubAppConfig.clientSecret},
                       {mrApp.clientId, mrApp.clientSecret},
                       database,
                       cache,
                       platforms,
                       github});

        auto authController(make_shared<api::v1::AuthController>(authConfig, auth, github, cache, database));
        auto controller(make_shared<api::v1::DocsController>(database, storage));
        auto browseController(make_shared<api::v1::BrowseController>(database));
        auto projectsController(make_shared<api::v1::ProjectsController>(auth, platforms, database, storage, cloudflare));
        auto projectWSController(make_shared<api::v1::ProjectWebSocketController>(database, storage, connections, auth));
        auto gameController(make_shared<api::v1::GameController>(database, storage));

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
