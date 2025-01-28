#pragma once

#include "cache.h"
#include "database.h"
#include "github.h"
#include "platforms.h"

#include <drogon/HttpRequest.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>

namespace service {
    struct UserSession {
        std::string sessionId;
        std::string username;
        User user;
        Json::Value profile;
    };

    struct OAuthApp {
        std::string clientId;
        std::string clientSecret;
    };

    class Auth : public CacheableServiceBase {
    public:
        explicit Auth(const std::string &, const OAuthApp &, const OAuthApp &, Database &, MemoryCache &, Platforms &, GitHub &);

        std::string getGitHubOAuthInitURL() const;
        drogon::Task<std::string> createUserSession(std::string username, std::string profile) const;
        drogon::Task<std::optional<std::string>> requestUserAccessToken(std::string code) const;
        drogon::Task<std::optional<UserSession>> getSession(drogon::HttpRequestPtr req) const;
        drogon::Task<std::optional<UserSession>> getSession(std::string id) const;
        drogon::Task<> expireSession(std::string id) const;

        std::string getModrinthOAuthInitURL(std::string state) const;
        drogon::Task<std::optional<std::string>> requestModrinthUserAccessToken(std::string code) const;
        drogon::Task<Error> linkModrinthAccount(std::string username, std::string token) const;
        drogon::Task<Error> unlinkModrinthAccount(std::string username) const;

        drogon::Task<std::optional<User>> getGitHubTokenUser(std::string token) const;
    private:
        Database &database_;
        MemoryCache &cache_;
        Platforms &platforms_;
        GitHub &github_;
        const std::string appUrl_;
        const OAuthApp githubApp_;
        const OAuthApp modrinthApp_;
    };
}