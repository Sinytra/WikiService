#include "docs.h"

#include <drogon/HttpClient.h>
#include <models/Mod.h>

#include "log/log.h"

#include <fstream>
#include <string>

#include "error.h"
#include <service/util.h>

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

    DocsController::DocsController(Service &s, GitHub &g, Database &db, Documentation &d) :
        service_(s), github_(g), database_(db), documentation_(d) {}

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
                const auto [locales, localesError](
                        co_await documentation_.hasAvailableLocale(*modResult, *locale, *installationToken));
                logger.info("Has locale for {}: {}", *locale, locales);
            }

            const auto ref = req->getOptionalParameter<std::string>("version");
            const auto prefixedPath = *modResult->getSourcePath() + '/' + path;
            const auto [contents, contentsError](
                    co_await github_.getRepositoryContents(repo, ref, prefixedPath, installationToken.value()));
            if (!contents) {
                co_return errorResponse(contentsError, "File not found", callback);
            }

            const auto [isPublic, publicError](co_await github_.isPublicRepository(modResult->getValueOfSourceRepo(), *installationToken));

            Json::Value root;
            {
                Json::Value modJson;
                modJson["id"] = modResult->getValueOfId();
                modJson["name"] = modResult->getValueOfName();
                modJson["platform"] = modResult->getValueOfPlatform();
                modJson["slug"] = modResult->getValueOfSlug();
                modJson["source_repo"] = modResult->getValueOfSourceRepo();
                modJson["is_community"] = modResult->getValueOfIsCommunity();
                modJson["is_public"] = isPublic;
                root["mod"] = modJson;
            }
            root["content"] = (*contents)["content"];
            if (isPublic) {
                root["edit_url"] = (*contents)["html_url"];
            }
            // TODO Update and edit url

            const auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k200OK);
            callback(resp);
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
        }

        co_return;
    }

    Task<> DocsController::tree(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                std::string mod) const {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
            }

            const auto [modResult, modError] = co_await database_.getModSource(mod);
            if (!modResult) {
                co_return errorResponse(modError, "Mod not found", callback);
            }
            const std::string repo = modResult->getValueOfSourceRepo();

            const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
            if (!installationId) {
                co_return errorResponse(installationIdError, "Missing repository installation", callback);
            }

            const auto [installationToken, error3](co_await github_.getInstallationToken(installationId.value()));
            if (!installationToken) {
                co_return errorResponse(installationIdError, "GitHub authentication failure", callback);
            }

            const auto [contents,
                        contentsError](co_await documentation_.getDirectoryTree(*modResult, *installationToken));
            if (!contents) {
                co_return errorResponse(contentsError, "Error getting dir tree", callback);
            }

            const auto [isPublic, publicError](co_await github_.isPublicRepository(modResult->getValueOfSourceRepo(), *installationToken));

            Json::Value root;
            {
                Json::Value modJson;
                modJson["id"] = modResult->getValueOfId();
                modJson["name"] = modResult->getValueOfName();
                modJson["platform"] = modResult->getValueOfPlatform();
                modJson["slug"] = modResult->getValueOfSlug();
                modJson["source_repo"] = modResult->getValueOfSourceRepo();
                modJson["is_community"] = modResult->getValueOfIsCommunity();
                modJson["is_public"] = isPublic;
                root["mod"] = modJson;
            }
            root["tree"] = contents;

            const auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k200OK);
            callback(resp);
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }

        co_return;
    }

    Task<> DocsController::asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                 std::string mod, std::string location) const
    {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
            }
            if (location.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing location parameter", callback);
            }

            const auto [modResult, modError] = co_await database_.getModSource(mod);
            if (!modResult) {
                co_return errorResponse(modError, "Mod not found", callback);
            }
            const std::string repo = modResult->getValueOfSourceRepo();

            const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
            if (!installationId) {
                co_return errorResponse(installationIdError, "Missing repository installation", callback);
            }

            const auto [installationToken, error3](co_await github_.getInstallationToken(installationId.value()));
            if (!installationToken) {
                co_return errorResponse(installationIdError, "GitHub authentication failure", callback);
            }

            const auto resourceLocation = ResourceLocation::parse(location);
            if (!resourceLocation) {
                co_return errorResponse(Error::ErrBadRequest, "Invalid location specified", callback);
            }

            const auto [asset, assetError](co_await documentation_.getAssetResource(*modResult, *resourceLocation, *installationToken));
            if (!asset) {
                co_return errorResponse(assetError, "Asset not found", callback);
            }

            Json::Value root;
            root["data"] = *asset;

            const auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k200OK);
            callback(resp);
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }

        co_return;
    }
}
