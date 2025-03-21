#include "auth.h"
#include "error.h"

#include <include/uri.h>
#include <log/log.h>
#include <service/crypto.h>
#include <service/database.h>
#include <service/github.h>
#include <service/util.h>
#include <string>

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

#define SESSION_COOKIE "sessionid"

namespace api::v1 {
    AuthController::AuthController(const config::AuthConfig &config) : config_(config), appDomain_(uri{config.frontendUrl}.get_host()) {}

    Task<> AuthController::initLogin(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto url = global::auth->getGitHubOAuthInitURL();
        const auto resp = HttpResponse::newRedirectionResponse(url);
        callback(resp);
        co_return;
    }

    Task<> AuthController::callbackGithub(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                          std::string code) const {
        if (code.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing code parameter");
        }

        if (const auto token = co_await global::auth->requestUserAccessToken(code)) {
            const auto [profile, err] = co_await global::github->getAuthenticatedUser(*token);
            if (!profile) {
                const auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                callback(resp);
            }

            const auto username = (*profile)["login"].asString();
            const auto sessionId = co_await global::auth->createUserSession(username, serializeJsonString(*profile));

            const auto resp = HttpResponse::newRedirectionResponse(config_.callbackUrl);
            Cookie cookie;
            cookie.setKey(SESSION_COOKIE);
            cookie.setValue(sessionId);
            cookie.setSecure(true);
            cookie.setHttpOnly(true);
            cookie.setSameSite(Cookie::SameSite::kStrict);
            cookie.setPath("/");
            cookie.setMaxAge(std::chrono::duration_cast<std::chrono::seconds>(30 * 24h).count());
            cookie.setDomain(appDomain_);
            resp->addCookie(cookie);

            callback(resp);
            co_return;
        }

        callback(HttpResponse::newRedirectionResponse(config_.errorCallbackUrl));

        co_return;
    }

    Task<> AuthController::logout(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = req->getCookie(SESSION_COOKIE);

        co_await global::auth->expireSession(session);

        const auto resp = HttpResponse::newRedirectionResponse(config_.frontendUrl);
        Cookie cookie;
        cookie.setKey(SESSION_COOKIE);
        cookie.setValue("");
        cookie.setPath("/");
        cookie.setMaxAge(0);
        cookie.setDomain(appDomain_);
        resp->addCookie(cookie);

        callback(resp);
        co_return;
    }

    Task<> AuthController::linkModrinth(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto token = req->getParameter("token");
        if (token.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing token parameter");
        }
        const auto encryptToken = crypto::encryptString(token, config_.tokenSecret);

        const auto url = global::auth->getModrinthOAuthInitURL(encryptToken);
        const auto resp = HttpResponse::newRedirectionResponse(url);
        callback(resp);
        co_return;
    }

    Task<> AuthController::callbackModrinth(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string code, const std::string state) const {
        if (code.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing code parameter");
        }
        if (state.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing state parameter");
        }

        const auto decryptedToken = crypto::decryptString(state, config_.tokenSecret);

        const auto session = co_await global::auth->getSession(decryptedToken);
        if (!session) {
            throw ApiException(Error::ErrUnauthorized, "Unauthorized");
        }

        if (const auto token = co_await global::auth->requestModrinthUserAccessToken(code)) {
            if (const auto result = co_await global::auth->linkModrinthAccount(session->username, *token); result != Error::Ok) {
                throw ApiException(result, "Error linking Modrinth account");
            }

            const auto resp = HttpResponse::newRedirectionResponse(config_.settingsCallbackUrl);
            callback(resp);
            co_return;
        }

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }

    Task<> AuthController::unlinkModrinth(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};

        if (const auto result = co_await global::database->unlinkUserModrinthAccount(session.username); result != Error::Ok) {
            throw ApiException(result, "Failed to unlink Modrinth account");
        }

        callback(HttpResponse::newHttpResponse());
    }

    Task<> AuthController::userProfile(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};
        Json::Value root = session.profile;
        root["created_at"] = session.user.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d");
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> AuthController::deleteAccount(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};

        if (const auto result = co_await global::database->deleteUserProjects(session.username); result != Error::Ok) {
            throw ApiException(result, "Error deleting account");
        }

        if (const auto result = co_await global::database->deleteUser(session.username); result != Error::Ok) {
            throw ApiException(result, "Error deleting account");
        }

        co_await global::auth->expireSession(session.sessionId);

        Cookie cookie;
        cookie.setKey(SESSION_COOKIE);
        cookie.setValue("");
        cookie.setPath("/");
        cookie.setMaxAge(0);
        cookie.setDomain(appDomain_);

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody("Account deleted successfully");
        resp->addCookie(cookie);
        callback(resp);

        co_return;
    }
}
