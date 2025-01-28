#include "auth.h"

#include <service/util.h>
#include "log/log.h"

#include <fstream>
#include <service/crypto.h>
#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

#define SESSION_COOKIE "sessionid"

namespace api::v1 {
    AuthController::AuthController(const std::string &fe, const std::string &cb, const std::string &cbs, const std::string &tk, Auth &a,
                                   GitHub &gh, MemoryCache &c, Database &d) :
        auth_(a), github_(gh), database_(d), cache_(c), appFrontendUrl_(fe), authCallbackUrl_(cb), authSettingsCallbackUrl_(cbs),
        tokenEncryptionKey_(tk) {}

    Task<> AuthController::initLogin(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto url = auth_.getGitHubOAuthInitURL();
        const auto resp = HttpResponse::newRedirectionResponse(url);
        callback(resp);
        co_return;
    }

    Task<> AuthController::callbackGithub(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                          std::string code) const {
        if (code.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing code parameter", callback);
        }

        if (const auto token = co_await auth_.requestUserAccessToken(code)) {
            // TODO Handle errors

            const auto [profile, err] = co_await github_.getAuthenticatedUser(*token);
            if (!profile) {
                const auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                callback(resp);
            }

            const auto username = (*profile)["login"].asString();
            const auto sessionId = co_await auth_.createUserSession(username, serializeJsonString(*profile));

            const auto resp = HttpResponse::newRedirectionResponse(authCallbackUrl_);
            Cookie cookie;
            cookie.setKey(SESSION_COOKIE);
            cookie.setValue(sessionId);
            cookie.setSecure(true);
            cookie.setHttpOnly(true);
            cookie.setSameSite(Cookie::SameSite::kStrict);
            cookie.setPath("/");
            cookie.setMaxAge(std::chrono::duration_cast<std::chrono::seconds>(30 * 24h).count());
            resp->addCookie(cookie);

            callback(resp);
            co_return;
        }

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        callback(resp);

        co_return;
    }

    Task<> AuthController::logout(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = req->getCookie(SESSION_COOKIE);

        co_await auth_.expireSession(session);

        const auto resp = HttpResponse::newRedirectionResponse(appFrontendUrl_);
        Cookie cookie;
        cookie.setKey(SESSION_COOKIE);
        cookie.setValue("");
        cookie.setPath("/");
        cookie.setMaxAge(0);
        resp->addCookie(cookie);

        callback(resp);
        co_return;
    }

    Task<> AuthController::linkModrinth(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto token = req->getParameter("token");
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }
        const auto encryptToken = crypto::encryptString(token, tokenEncryptionKey_);

        const auto url = auth_.getModrinthOAuthInitURL(encryptToken);
        const auto resp = HttpResponse::newRedirectionResponse(url);
        callback(resp);
        co_return;
    }

    Task<> AuthController::callbackModrinth(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string code,
                                            std::string state) const {
        if (code.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing code parameter", callback);
        }
        if (state.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing state parameter", callback);
        }

        const auto decryptedToken = crypto::decryptString(state, tokenEncryptionKey_);

        const auto session = co_await auth_.getSession(decryptedToken);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        if (const auto token = co_await auth_.requestModrinthUserAccessToken(code)) {
            if (const auto result = co_await auth_.linkModrinthAccount(session->username, *token); result != Error::Ok) {
                co_return errorResponse(result, "Error linking Modrinth account", callback);
            }

            const auto resp = HttpResponse::newRedirectionResponse(authSettingsCallbackUrl_);
            callback(resp);
            co_return;
        }

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        callback(resp);

        co_return;
    }

    Task<> AuthController::unlinkModrinth(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = co_await auth_.getSession(req);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        if (const auto result = co_await database_.unlinkUserModrinthAccount(session->username); result != Error::Ok) {
            co_return errorResponse(result, "Failed to unlink Modrinth account", callback);
        }

        callback(HttpResponse::newHttpResponse());

        co_return;
    }

    Task<> AuthController::userProfile(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = co_await auth_.getSession(req);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        Json::Value root = session->profile;
        root["created_at"] = session->user.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d");
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }

    Task<> AuthController::deleteAccount(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = co_await auth_.getSession(req);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        if (const auto result = co_await database_.deleteUserProjects(session->username); result != Error::Ok) {
            co_return errorResponse(result, "Error deleting account", callback);
        }

        if (const auto result = co_await database_.deleteUser(session->username); result != Error::Ok) {
            co_return errorResponse(result, "Error deleting account", callback);
        }

        co_await auth_.expireSession(session->sessionId);

        Cookie cookie;
        cookie.setKey(SESSION_COOKIE);
        cookie.setValue("");
        cookie.setPath("/");
        cookie.setMaxAge(0);

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody("Account deleted successfully");
        resp->addCookie(cookie);
        callback(resp);

        co_return;
    }
}
