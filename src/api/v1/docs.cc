#include "docs.h"

#include <drogon/HttpClient.h>
#include <models/Mod.h>

#include "log/log.h"

#include <string>
#include <fstream>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    void errorResponse(const Error &error, const std::string &message,
                       std::function<void(const HttpResponsePtr &)> &callback) {
        Json::Value json;
        json["error"] = message;
        const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
        resp->setStatusCode(mapError(error));

        callback(resp);
    }

    DocsController::DocsController(Service &s, GitHub &g, Database &db, Documentation &d)
        : service_(s), github_(g), database_(db), documentation_(d) {}

    Task<> DocsController::page(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                std::string mod, std::string path) const {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
            }
            if (path.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing path parameter", callback);
            }

            const auto [modResult, modError] = co_await database_.getModSource(mod);
            if (!modResult) {
                co_return errorResponse(modError, "Mod not found", callback);
            }
            const std::string repo = *modResult->getSourceRepo();

            const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
            if (!installationId) {
                co_return errorResponse(installationIdError, "Missing repository installation", callback);
            }

            const auto [installationToken, error3](co_await github_.getInstallationToken(installationId.value()));
            if (!installationToken) {
                co_return errorResponse(installationIdError, "GitHub authentication failure", callback);
            }

            const auto locale = req->getOptionalParameter<std::string>("locale");
            if (locale) {
                const auto [locales, localesError](co_await documentation_.hasAvailableLocale(*modResult, *locale, *installationToken));
                logger.info("Has locale for {}: {}", *locale, locales);
            }

            const auto ref = req->getOptionalParameter<std::string>("version");
            const auto prefixedPath = *modResult->getSourcePath() + '/' + path;
            const auto [contents, contentsError](
                co_await github_.getRepositoryContents(repo, ref, prefixedPath, installationToken.value()));
            if (!contents) {
                co_return errorResponse(contentsError, "File not found", callback);
            }

            const auto resp = HttpResponse::newHttpJsonResponse(*contents);
            resp->setStatusCode(k200OK);
            callback(resp);

            // auto [modResponse, err](co_await service_.getMod(ModRequest{.id = mod}));
            // if (auto respValue = modResponse; respValue && err == Error::Ok) {
            //     Json::Value json;
            //     json["message"] = "Hello";
            //     json["id"] = modResponse.value().id;
            //     json["name"] = modResponse.value().name;
            //     const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
            //     resp->setStatusCode(drogon::k200OK);
            //
            //     callback(resp);
            // } else {
            //     Json::Value json;
            //     json["error"] = "Mod does not exist";
            //     const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
            //     resp->setStatusCode(drogon::k500InternalServerError);
            //
            //     callback(resp);
            // }
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
        }

        co_return;
    }
}
