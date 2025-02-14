#include "auth.h"

#include <models/User.h>
#include <service/database.h>
#include <service/github.h>
#include <service/platforms.h>

#include "crypto.h"
#include "log/log.h"
#include "util.h"

#define GH_HOST "https://github.com"
#define GH_OAUTH_SCOPE "read:user+read:org"
#define GH_OAUTH_INIT_URL "https://github.com/login/oauth/authorize"
#define GH_OAUTH_TOKEN_PATH "/login/oauth/access_token"

#define MR_OAUTH_INIT_URL "https://modrinth.com/auth/authorize"
#define MR_OAUTH_TOKEN_PATH "/_internal/oauth/token"

#define SESSION_ID_LEN 64

using namespace drogon;
using namespace drogon_model::postgres;
using namespace std::chrono_literals;

std::string createSessionCacheKey(const std::string &sessionId) { return "session:" + sessionId; }

namespace service {
    Auth::Auth(const std::string &appUrl, const OAuthApp &ghApp, const OAuthApp &mrApp) :
        appUrl_(appUrl), githubApp_(ghApp), modrinthApp_(mrApp) {}

    std::string Auth::getGitHubOAuthInitURL() const {
        const auto callbackUrl = appUrl_ + "/api/v1/auth/callback/github";
        const auto params = std::format("?scope={}&client_id={}&redirect_uri={}", GH_OAUTH_SCOPE, githubApp_.clientId, callbackUrl);
        return GH_OAUTH_INIT_URL + params;
    }

    Task<std::optional<std::string>> Auth::requestUserAccessToken(const std::string code) const {
        const auto callbackUrl = appUrl_ + "/api/v1/auth/callback/github";
        const auto client = createHttpClient(GH_HOST);

        const auto tokenReq = HttpRequest::newHttpRequest();
        tokenReq->setMethod(Post);
        tokenReq->setPath(GH_OAUTH_TOKEN_PATH);
        tokenReq->setParameter("code", code);
        tokenReq->setParameter("client_id", githubApp_.clientId);
        tokenReq->setParameter("client_secret", githubApp_.clientSecret);
        tokenReq->setParameter("redirect_uri", callbackUrl);
        tokenReq->addHeader("Accept", "application/json");

        const auto response = co_await client->sendRequestCoro(tokenReq);
        if (const auto status = response->getStatusCode(); isSuccess(status)) {
            if (const auto jsonResp = response->getJsonObject()) {
                if (jsonResp->isMember("access_token")) {
                    const auto token = (*jsonResp)["access_token"].asString();
                    co_return token;
                }
                if (jsonResp->isMember("error")) {
                    const auto error = (*jsonResp)["error"].asString();
                    logger.error("Error requesting OAuth token: {}", error);
                }
            }
        }

        co_return std::nullopt;
    }

    Task<std::string> Auth::createUserSession(const std::string username, const std::string profile) const {
        const auto normalUserName = strToLower(username);

        co_await global::database->createUserIfNotExists(normalUserName);

        const auto sessionId = crypto::generateSecureRandomString(SESSION_ID_LEN);

        const std::unordered_map<std::string, std::string> values = {{"username", normalUserName}, {"profile", profile}};
        co_await global::cache->updateCacheHash(createSessionCacheKey(sessionId), values, 30 * 24h);

        co_return sessionId;
    }

    Task<std::optional<UserSession>> Auth::getSession(const HttpRequestPtr req) const {
        const auto token = req->getParameter("token");
        co_return co_await getSession(token);
    }

    Task<std::optional<UserSession>> Auth::getSession(const std::string id) const {
        if (id.empty()) {
            co_return std::nullopt;
        }

        const auto sid = createSessionCacheKey(id);
        const auto username = co_await global::cache->getHashMember(sid, "username");
        if (!username) {
            co_return std::nullopt;
        }
        const auto profile = co_await global::cache->getHashMember(sid, "profile");
        if (!profile) {
            co_return std::nullopt;
        }
        const auto profileJson = parseJsonString(*profile);
        if (!profileJson) {
            co_return std::nullopt;
        }

        const auto user = co_await global::database->getUser(*username);
        if (!user) {
            co_return std::nullopt;
        }

        Json::Value root;
        root["username"] = (*profileJson)["login"].asString();
        root["avatar_url"] = (*profileJson)["avatar_url"].asString();
        root["modrinth_id"] = user->getModrinthId() ? user->getValueOfModrinthId() : Json::Value::null;

        co_return UserSession{.sessionId = id, .username = *username, .user = *user, .profile = root};
    }

    Task<> Auth::expireSession(const std::string id) const { co_await global::cache->erase(createSessionCacheKey(id)); }

    std::string Auth::getModrinthOAuthInitURL(std::string state) const {
        const auto callbackUrl = appUrl_ + "/api/v1/auth/callback/modrinth";
        const auto params = std::format("?client_id={}&response_type=code&scope={}&redirect_uri={}&state={}", modrinthApp_.clientId,
                                        "USER_READ", callbackUrl, state);
        return MR_OAUTH_INIT_URL + params;
    }

    Task<std::optional<std::string>> Auth::requestModrinthUserAccessToken(std::string code) const {
        const auto callbackUrl = appUrl_ + "/api/v1/auth/callback/modrinth";
        const auto client = createHttpClient(MODRINTH_API_URL);
        client->setUserAgent(WIKI_USER_AGENT);

        const auto tokenReq = HttpRequest::newHttpRequest();
        tokenReq->setPath(MR_OAUTH_TOKEN_PATH);
        tokenReq->setMethod(Post);
        const auto body = std::format("code={}&client_id={}&redirect_uri={}&grant_type={}", code, modrinthApp_.clientId, callbackUrl,
                                      "authorization_code");
        tokenReq->setBody(body);
        tokenReq->setContentTypeString("application/x-www-form-urlencoded");
        tokenReq->addHeader("Authorization", modrinthApp_.clientSecret);

        const auto response = co_await client->sendRequestCoro(tokenReq);
        const auto status = response->getStatusCode();
        const auto jsonResp = response->getJsonObject();
        if (isSuccess(status)) {
            const auto token = (*jsonResp)["access_token"].asString();
            co_return token;
        }

        co_return std::nullopt;
    }

    Task<Error> Auth::linkModrinthAccount(const std::string username, const std::string token) const {
        const auto modrinthId = co_await global::platforms->modrinth_.getAuthenticatedUserID(token);
        if (!modrinthId) {
            co_return Error::ErrBadRequest;
        }

        co_return co_await global::database->linkUserModrinthAccount(username, *modrinthId);
    }

    Task<Error> Auth::unlinkModrinthAccount(const std::string username) const {
        co_return co_await global::database->unlinkUserModrinthAccount(username);
    }

    Task<std::optional<User>> Auth::getGitHubTokenUser(const std::string token) const {
        if (token.starts_with("ghp_")) {
            if (const auto [ghProfile, ghErr](co_await global::github->getAuthenticatedUser(token)); ghProfile) {
                const auto username = (*ghProfile)["login"].asString();
                const auto user = co_await global::database->getUser(strToLower(username));
                co_return user;
            }
        }
        co_return std::nullopt;
    }
}
