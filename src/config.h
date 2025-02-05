#pragma once

#include <string>

namespace config {
  struct AuthConfig {
    std::string frontendUrl;
    std::string callbackUrl;
    std::string settingsCallbackUrl;
    std::string errorCallbackUrl;
    std::string tokenSecret;
  };

  struct GitHubConfig {
    std::string clientId;
    std::string clientSecret;
  };

  struct Modrinth {
    std::string clientId;
    std::string clientSecret;
  };

  struct CloudFlare {
    std::string token;
    std::string accountTag;
    std::string siteTag;
  };

  struct SystemConfig {
    AuthConfig auth;
    GitHubConfig githubApp;
    Modrinth modrinth;
    CloudFlare cloudFlare;

    std::string appUrl;
    std::string curseForgeKey;
    std::string storagePath;
  };

  SystemConfig configure();
}