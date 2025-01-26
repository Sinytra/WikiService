#include "auth.h"

#include <service/util.h>
#include "log/log.h"

#include <fstream>
#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

namespace api::v1 {
    AuthController::AuthController(const std::string &fe, const std::string &cb, Auth &a, GitHub &gh, MemoryCache &c, Database &d) :
        appFrontendUrl_(fe), authCallbackUrl_(cb), auth_(a), github_(gh), cache_(c), database_(d) {}

    Task<> AuthController::initLogin(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) const {
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

            const auto resp = HttpResponse::newRedirectionResponse("http://localhost:3000/en/dev");
            Cookie cookie;
            cookie.setKey("sessionid");
            cookie.setValue(sessionId);
            cookie.setSecure(true);
            cookie.setHttpOnly(true);
            cookie.setSameSite(Cookie::SameSite::kStrict); // TODO This breaks live logs
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

    Task<> AuthController::logout(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session = req->getCookie("sessionid");

        co_await auth_.expireSession(session);

        const auto resp = HttpResponse::newRedirectionResponse(appFrontendUrl_);
        Cookie cookie;
        cookie.setKey("sessionid");
        cookie.setValue("");
        cookie.setPath("/");
        cookie.setMaxAge(0);
        resp->addCookie(cookie);

        callback(resp);
        co_return;
    }

    Task<> AuthController::linkModrinth(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                        std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        const auto url = auth_.getModrinthOAuthInitURL(token);
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

        const auto session = co_await auth_.getSession(state);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        // TODO Use User-Agent
        if (const auto token = co_await auth_.requestModrinthUserAccessToken(code)) {
            if (const auto result = co_await auth_.linkModrinthAccount(session->username, *token); result != Error::Ok) {
                co_return errorResponse(result, "Error linking Modrinth account", callback);
            }

            const auto resp = HttpResponse::newRedirectionResponse(authCallbackUrl_);
            callback(resp);
            co_return;
        }

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        callback(resp);

        co_return;
    }

    Task<> AuthController::userProfile(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrUnauthorized, "Missing token parameter", callback);
        }

        const auto session = co_await auth_.getSession(token);
        if (!session) {
            co_return errorResponse(Error::ErrUnauthorized, "Unauthorized", callback);
        }

        const auto resp = HttpResponse::newHttpJsonResponse(session->profile);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
