#include <drogon/drogon.h>
#include <service/cloudflare.h>
#include <stdexcept>

#include "api/v1/auth.h"
#include "api/v1/browse.h"
#include "api/v1/docs.h"
#include "api/v1/projects.h"
#include "api/v1/websocket.h"
#include "git2.h"
#include "log/log.h"
#include "service/github.h"
#include "service/platforms.h"
#include "service/schemas.h"
#include "service/storage.h"
#include "service/util.h"
#include "service/content/game_content.h"

using namespace std;
using namespace drogon;
using namespace logging;

namespace service {
    trantor::EventLoopThreadPool cacheAwaiterThreadPool{1};
}

Json::Value configureFromEnvironment() {
    logger.info("Loading configuration from environment");
    Json::Value root;
    {
        Json::Value db;
        db["name"] = "default";
        db["rdbms"] = "postgresql";
        db["host"] = std::getenv("DB_HOST");
        db["port"] = stoi(std::getenv("DB_PORT"));
        db["dbname"] = std::getenv("DB_DATABASE");
        db["user"] = std::getenv("DB_USER");
        db["passwd"] = std::getenv("DB_PASSWORD");
        db["is_fast"] = true;
        db["connection_number"] = 1;

        root["db_clients"] = Json::Value(Json::arrayValue);
        root["db_clients"].append(db);
    }
    {
        Json::Value redis;
        redis["name"] = "default";
        redis["host"] = std::getenv("REDIS_HOST");
        redis["port"] = stoi(std::getenv("REDIS_PORT"));
        redis["username"] = std::getenv("REDIS_USER");
        redis["passwd"] = std::getenv("REDIS_PASSWORD");
        redis["db"] = 0;
        redis["is_fast"] = true;
        redis["number_of_connections"] = 1;

        root["redis_clients"] = Json::Value(Json::arrayValue);
        root["redis_clients"].append(redis);
    }
    {
        Json::Value app;
        Json::Value log;
        log["use_spdlog"] = true;
        log["log_path"] = std::getenv("LOG_PATH");
        log["log_size_limit"] = 100000000;
        log["max_files"] = 10;
        log["log_level"] = std::getenv("LOG_LEVEL");
        log["display_local_time"] = false;

        app["log"] = log;
        root["app"] = app;
    }
    app().loadConfigJson(root);

    Json::Value custom;
    custom["app_url"] = std::getenv("APP_URL");
    custom["frontend_url"] = std::getenv("FRONTEND_URL");
    {
        Json::Value auth;
        auth["callback_url"] = std::getenv("AUTH_CALLBACK_URL");
        auth["settings_callback_url"] = std::getenv("AUTH_SETTINGS_CALLBACK_URL");
        auth["error_callback_url"] = std::getenv("AUTH_ERROR_CALLBACK_URL");
        auth["token_secret"] = std::getenv("AUTH_TOKEN_SECRET");
        custom["auth"] = auth;
    }
    {
        Json::Value github;
        github["name"] = std::getenv("GITHUB_APP_NAME");
        github["client_id"] = std::getenv("GITHUB_CLIENT_ID");
        github["client_secret"] = std::getenv("GITHUB_CLIENT_SECRET");
        custom["github"] = github;
    }
    {
        Json::Value modrinth;
        modrinth["client_id"] = std::getenv("MODRINTH_CLIENT_ID");
        modrinth["client_secret"] = std::getenv("MODRINTH_CLIENT_SECRET");
        custom["modrinth"] = modrinth;
    }
    {
        Json::Value cloudflare;
        cloudflare["token"] = std::getenv("CLOUDFLARE_TOKEN");
        cloudflare["account_tag"] = std::getenv("CLOUDFLARE_ACCOUNT_TAG");
        cloudflare["site_tag"] = std::getenv("CLOUDFLARE_SITE_TAG");
        custom["cloudflare"] = cloudflare;
    }
    custom["curseforge_key"] = std::getenv("CURSEFORGE_KEY");
    custom["storage_path"] = std::getenv("STORAGE_PATH");

    return custom;
}

Json::Value readCustomConfig() {
    if (const std::filesystem::path configPath("config.json"); !exists(configPath)) {
        return configureFromEnvironment();
    }
    app().loadConfigFile("config.json");
    return app().getCustomConfig();
}

int main() {
    constexpr auto port(8080);
    logger.info("Starting wiki service server on port {}", port);

    try {
        const Json::Value &customConfig = readCustomConfig();

        const auto level = trantor::Logger::logLevel();
        app().setLogLevel(level).addListener("0.0.0.0", port).setThreadNum(16);
        configureLoggingLevel();

        if (const auto error = validateJson(schemas::systemConfig, customConfig)) {
            logger.error("App config validation failed at {}: {}", error->pointer.to_string(), error->msg);
            throw std::runtime_error("Invalid configuration");
        }

        // TODO Turn into structs
        const std::string &appUrl = customConfig["app_url"].asString();
        const std::string &appFrontendUrl = customConfig["frontend_url"].asString();
        const Json::Value &authConfig = customConfig["auth"];
        const std::string &authCallbackUrl = authConfig["callback_url"].asString();
        const std::string &authSettingsCallbackUrl = authConfig["settings_callback_url"].asString();
        const std::string &authErrorCallbackUrl = authConfig["error_callback_url"].asString();
        const std::string &authTokenSecret = authConfig["token_secret"].asString();
        const Json::Value &githubAppConfig = customConfig["github_app"];
        const std::string &appClientId = githubAppConfig["client_id"].asString();
        const std::string &appClientSecret = githubAppConfig["client_secret"].asString();
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
        auto github(GitHub{});
        auto connections(RealtimeConnectionStorage{});
        auto ingestor(content::Ingestor{database});
        auto storage(Storage{storageBasePath, cache, connections, ingestor});
        auto cloudflare(CloudFlare{cloudFlareToken, cloudFlareAccTag, cloudFlareSiteTag, cache});
        auto modrinth(ModrinthPlatform{});
        auto curseForge(CurseForgePlatform{curseForgeKey});
        auto platforms(Platforms(curseForge, modrinth));
        auto auth(Auth{appUrl, {appClientId, appClientSecret}, {mrAppClientId, mrAppClientSecret}, database, cache, platforms, github});

        auto authController(make_shared<api::v1::AuthController>(appFrontendUrl, authCallbackUrl, authSettingsCallbackUrl,
                                                                 authErrorCallbackUrl, authTokenSecret, auth, github, cache, database));
        auto controller(make_shared<api::v1::DocsController>(database, storage));
        auto browseController(make_shared<api::v1::BrowseController>(database));
        auto projectsController(make_shared<api::v1::ProjectsController>(auth, platforms, database, storage, cloudflare));
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
