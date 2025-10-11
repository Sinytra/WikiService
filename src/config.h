#pragma once

#include <string>

namespace config {
    struct AuthConfig {
        std::string frontendUrl;
        std::string callbackUrl;
        std::string settingsCallbackUrl;
        std::string errorCallbackUrl;
        std::string tokenSecret;
        std::string frontendApiKey;
    };

    struct GitHubConfig {
        std::string clientId;
        std::string clientSecret;
    };

    struct Modrinth {
        std::string clientId;
        std::string clientSecret;
    };

    struct Crowdin {
        std::string token;
        std::string projectId;
    };

    struct Sentry {
        std::string dsn;
    };

    struct SystemConfig {
        AuthConfig auth;
        GitHubConfig githubApp;
        Modrinth modrinth;
        Crowdin crowdin;
        Sentry sentry;

        std::string appUrl;
        std::string curseForgeKey;
        std::string storagePath;
        std::string salt;
        bool local;
    };

    SystemConfig configure();
}
