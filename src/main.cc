#include "api/v1/auth.h"
#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/game.h"
#include "api/v1/projects.h"
#include "api/v1/system.h"
#include "api/v1/websocket.h"

#include <service/cloudflare.h>
#include <service/database/database.h>
#include <service/github.h>
#include <service/lang/lang.h>
#include <service/storage/realtime.h>
#include "config.h"
#include "version.h"

#include <api/v1/error.h>
#include <git2.h>
#include <log/log.h>

#include <service/content/recipe_builtin.h>
#include "api/v1/moderation.h"
#include "service/content/game_data.h"
#include "service/lang/crowdin.h"

using namespace drogon;
using namespace logging;
using namespace service;

namespace service {
    trantor::EventLoopThreadPool cacheAwaiterThreadPool{10};
}

namespace global {
    std::shared_ptr<Database> database;
    std::shared_ptr<MemoryCache> cache;
    std::shared_ptr<GitHub> github;
    std::shared_ptr<realtime::ConnectionManager> connections;
    std::shared_ptr<Storage> storage;
    std::shared_ptr<CloudFlare> cloudFlare;
    std::shared_ptr<Auth> auth;
    std::shared_ptr<LangService> lang;
    std::shared_ptr<GameDataService> gameData;
    std::shared_ptr<Crowdin> crowdin;

    std::shared_ptr<Platforms> platforms;
}

void globalExceptionHandler(const std::exception &e, const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    if (const auto cast = dynamic_cast<const ApiException *>(&e); cast != nullptr) {
        const auto resp = HttpResponse::newHttpJsonResponse(std::move(cast->data));
        resp->setStatusCode(api::v1::mapError(cast->error));

        callback(resp);
        return;
    }

    logger.error("Unhandled exception: {}", e.what());
    callback(statusResponse(k500InternalServerError));
}

Task<> runStartupTaks() {
    co_await global::database->failLoadingDeployments();
    co_await global::gameData->setupGameData();
    co_await global::crowdin->getAvailableLocales(); // TODO Reload in production
}

int main() {
    constexpr auto port(8080);
    logger.info("Starting wiki service version {} on port {}", PROJECT_VERSION, port);

    try {
        const auto level = trantor::Logger::logLevel();
        app().setLogLevel(level).addListener("0.0.0.0", port).setThreadNum(16);
        configureLoggingLevel();

        const auto [authConfig, githubAppConfig, mrApp, cloudFlareConfig, crowdinConfig, appUrl, curseForgeKey, storagePath] =
            config::configure();

        global::database = std::make_shared<Database>();
        global::cache = std::make_shared<MemoryCache>();
        global::github = std::make_shared<GitHub>();
        global::connections = std::make_shared<realtime::ConnectionManager>();
        global::storage = std::make_shared<Storage>(storagePath);
        global::cloudFlare = std::make_shared<CloudFlare>(cloudFlareConfig);
        global::auth = std::make_shared<Auth>(appUrl, OAuthApp{githubAppConfig.clientId, githubAppConfig.clientSecret},
                                              OAuthApp{mrApp.clientId, mrApp.clientSecret});
        global::lang = std::make_shared<LangService>();
        global::gameData = std::make_shared<GameDataService>(storagePath);
        global::crowdin = std::make_shared<Crowdin>(crowdinConfig);
        auto curseForge(CurseForgePlatform{curseForgeKey});
        auto modrinth(ModrinthPlatform{});
        global::platforms = std::make_shared<Platforms>(curseForge, modrinth);

        auto authController(std::make_shared<api::v1::AuthController>(authConfig));
        auto controller(std::make_shared<api::v1::DocsController>());
        auto browseController(std::make_shared<api::v1::BrowseController>());
        auto projectsController(std::make_shared<api::v1::ProjectsController>());
        auto projectWSController(std::make_shared<api::v1::ProjectWebSocketController>());
        auto gameController(std::make_shared<api::v1::GameController>());
        auto systemController(std::make_shared<api::v1::SystemController>());
        auto moderationController(std::make_shared<api::v1::ModerationController>());

        app().registerController(authController);
        app().registerController(controller);
        app().registerController(browseController);
        app().registerController(projectsController);
        app().registerController(projectWSController);
        app().registerController(gameController);
        app().registerController(systemController);
        app().registerController(moderationController);
        app().setExceptionHandler(globalExceptionHandler);

        cacheAwaiterThreadPool.start();
        git_libgit2_init();

        content::loadBuiltinRecipeTypes();

        app().getLoop()->queueInLoop(async_func([]() -> Task<> { co_await runStartupTaks(); }));

        app().run();

        git_libgit2_shutdown();
        for (auto &loop : cacheAwaiterThreadPool.getLoops()) {
            loop->quit();
        }
        cacheAwaiterThreadPool.wait();
    } catch (const std::exception &e) {
        logger.critical("Error running app: {}", e.what());
    }

    return 0;
}
