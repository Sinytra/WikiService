#include "config.h"

#include <drogon/drogon.h>
#include <log/log.h>
#include <service/schemas.h>
#include <service/util.h>

using namespace drogon;
using namespace logging;
using namespace config;

void configureAppFromEnvironment() {
    Json::Value root;
    {
        Json::Value db;
        db["name"] = "default";
        db["rdbms"] = "postgresql";
        db["host"] = std::getenv("DB_HOST");
        db["port"] = std::stoi(std::getenv("DB_PORT"));
        db["dbname"] = std::getenv("DB_DATABASE");
        db["user"] = std::getenv("DB_USER");
        db["passwd"] = std::getenv("DB_PASSWORD");
        db["is_fast"] = true;
        db["connection_number"] = 1;
        db["timeout"] = 20;

        root["db_clients"] = Json::Value(Json::arrayValue);
        root["db_clients"].append(db);
    }
    {
        Json::Value redis;
        redis["name"] = "default";
        redis["host"] = std::getenv("REDIS_HOST");
        redis["port"] = std::stoi(std::getenv("REDIS_PORT"));
        redis["username"] = std::getenv("REDIS_USER");
        redis["passwd"] = std::getenv("REDIS_PASSWORD");
        redis["db"] = 0;
        redis["is_fast"] = true;
        redis["number_of_connections"] = 1;
        redis["timeout"] = 20;

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
}

SystemConfig configureFromEnvironment() {
    logger.info("Loading configuration from environment");

    AuthConfig auth = {.frontendUrl = std::getenv("AUTH_FRONTEND_URL"),
                       .callbackUrl = std::getenv("AUTH_CALLBACK_URL"),
                       .settingsCallbackUrl = std::getenv("AUTH_SETTINGS_CALLBACK_URL"),
                       .errorCallbackUrl = std::getenv("AUTH_ERROR_CALLBACK_URL"),
                       .tokenSecret = std::getenv("AUTH_TOKEN_SECRET")};
    GitHubConfig githubApp = {
        .clientId = std::getenv("GITHUB_CLIENT_ID"),
        .clientSecret = std::getenv("GITHUB_CLIENT_SECRET"),
    };
    Modrinth modrinth = {.clientId = std::getenv("MODRINTH_CLIENT_ID"), .clientSecret = std::getenv("MODRINTH_CLIENT_SECRET")};
    CloudFlare cloudFlare = {.token = std::getenv("CLOUDFLARE_TOKEN"),
                             .accountTag = std::getenv("CLOUDFLARE_ACCOUNT_TAG"),
                             .siteTag = std::getenv("CLOUDFLARE_SITE_TAG")};
    Crowdin crowdin = {.token = std::getenv("CROWDIN_TOKEN"), .projectId = std::getenv("CROWDIN_PROJECT_ID")};
    return {.auth = auth,
            .githubApp = githubApp,
            .modrinth = modrinth,
            .cloudFlare = cloudFlare,
            .crowdin = crowdin,

            .appUrl = std::getenv("APP_URL"),
            .curseForgeKey = std::getenv("CURSEFORGE_KEY"),
            .storagePath = std::getenv("STORAGE_PATH")};
}

SystemConfig config::configure() {
    if (const std::filesystem::path configPath("config.json"); !exists(configPath)) {
        return configureFromEnvironment();
    }
    app().loadConfigFile("config.json");

    Json::Value customConfig = app().getCustomConfig();

    if (const auto error = validateJson(schemas::systemConfig, customConfig)) {
        logger.error("App config validation failed at {}: {}", error->pointer.to_string(), error->msg);
        throw std::runtime_error("Invalid configuration");
    }

    const Json::Value &authConfig = customConfig["auth"];
    AuthConfig auth = {.frontendUrl = authConfig["frontend_url"].asString(),
                       .callbackUrl = authConfig["callback_url"].asString(),
                       .settingsCallbackUrl = authConfig["settings_callback_url"].asString(),
                       .errorCallbackUrl = authConfig["error_callback_url"].asString(),
                       .tokenSecret = authConfig["token_secret"].asString()};
    const Json::Value &githubAppConfig = customConfig["github_app"];
    GitHubConfig githubApp = {.clientId = githubAppConfig["client_id"].asString(),
                              .clientSecret = githubAppConfig["client_secret"].asString()};
    const Json::Value &mrApp = customConfig["modrinth_app"];
    Modrinth modrinth = {.clientId = mrApp["client_id"].asString(), .clientSecret = mrApp["client_secret"].asString()};
    const Json::Value &cloudFlareConfig = customConfig["cloudflare"];
    CloudFlare cloudFlare = {.token = cloudFlareConfig["token"].asString(),
                             .accountTag = cloudFlareConfig["account_tag"].asString(),
                             .siteTag = cloudFlareConfig["site_tag"].asString()};
    const Json::Value &crowdinConfig = customConfig["crowdin"];
    Crowdin crowdin = {.token = crowdinConfig["token"].asString(), .projectId = crowdinConfig["project_id"].asString()};
    SystemConfig config = {.auth = auth,
                           .githubApp = githubApp,
                           .modrinth = modrinth,
                           .cloudFlare = cloudFlare,
                           .crowdin = crowdin,
                           .appUrl = customConfig["app_url"].asString(),
                           .curseForgeKey = customConfig["curseforge_key"].asString(),
                           .storagePath = customConfig["storage_path"].asString()};

    if (!customConfig.isMember("api_key") || customConfig["api_key"].asString().empty()) {
        logger.warn("No API key configured, allowing public API access.");
    }

    return config;
}
