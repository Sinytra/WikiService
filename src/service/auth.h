#pragma once

#include <json/value.h>
#include <models/User.h>
#include <service/cache.h>
#include <service/util.h>

#define ROLE_ADMIN "admin"

using namespace drogon_model::postgres;

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
        explicit Auth(const std::string &, const OAuthApp &, const OAuthApp &);

        std::string getGitHubOAuthInitURL() const;
        drogon::Task<std::string> createUserSession(std::string username, std::string profile) const;
        drogon::Task<std::optional<std::string>> requestUserAccessToken(std::string code) const;
        drogon::Task<UserSession> getSession(drogon::HttpRequestPtr req) const;
        drogon::Task<> ensurePrivilegedAccess(drogon::HttpRequestPtr req) const;
        drogon::Task<std::optional<UserSession>> getSession(std::string id) const;
        drogon::Task<UserSession> getExternalSession(drogon::HttpRequestPtr req) const;
        drogon::Task<> expireSession(std::string id) const;

        std::string getModrinthOAuthInitURL(std::string state) const;
        drogon::Task<std::optional<std::string>> requestModrinthUserAccessToken(std::string code) const;
        drogon::Task<TaskResult<>> linkModrinthAccount(std::string username, std::string token) const;
        drogon::Task<TaskResult<>> unlinkModrinthAccount(std::string username) const;

        drogon::Task<TaskResult<User>> getGitHubTokenUser(std::string token) const;

    private:
        const std::string appUrl_;
        const OAuthApp githubApp_;
        const OAuthApp modrinthApp_;
    };
}

namespace global {
    extern std::shared_ptr<service::Auth> auth;
}
