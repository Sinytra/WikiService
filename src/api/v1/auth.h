#pragma once

#include <drogon/HttpController.h>

#include "config.h"
#include "service/github.h"

#include <service/auth.h>

namespace api::v1 {
    class AuthController final : public drogon::HttpController<AuthController, false> {
    public:
        explicit AuthController(const config::AuthConfig &, service::Auth &, service::GitHub &, service::MemoryCache &, service::Database &);

        // clang-format off
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(AuthController::initLogin,        "/api/v1/auth/login",                           drogon::Get);
        ADD_METHOD_TO(AuthController::callbackGithub,   "/api/v1/auth/callback/github?code={1:code}",   drogon::Get);
        ADD_METHOD_TO(AuthController::logout,           "/api/v1/auth/logout",                          drogon::Get);

        ADD_METHOD_TO(AuthController::linkModrinth,     "/api/v1/auth/link/modrinth",                                   drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(AuthController::callbackModrinth, "/api/v1/auth/callback/modrinth?code={1:code}&state={2:state}", drogon::Get);
        ADD_METHOD_TO(AuthController::unlinkModrinth,   "/api/v1/auth/unlink/modrinth",                                 drogon::Post,   "AuthFilter");

        ADD_METHOD_TO(AuthController::userProfile,      "/api/v1/auth/user",    drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(AuthController::deleteAccount,    "/api/v1/auth/user",    drogon::Delete, "AuthFilter");
        METHOD_LIST_END
        // clang-format on

        drogon::Task<> initLogin(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> callbackGithub(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                      std::string code) const;
        drogon::Task<> logout(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> linkModrinth(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> callbackModrinth(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                        std::string code, std::string state) const;
        drogon::Task<> unlinkModrinth(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> userProfile(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> deleteAccount(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

    private:
        const config::AuthConfig &config_;
        service::Auth &auth_;
        service::GitHub &github_;
        service::Database &database_;
        service::MemoryCache &cache_;
        const std::string appDomain_;
    };
}
