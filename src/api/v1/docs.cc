#include "docs.h"

#include <drogon/HttpClient.h>
#include <models/Mod.h>

#include "log/log.h"

#include <fstream>
#include <string>

#include <service/util.h>
#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<std::optional<std::string>> assertLocale(const std::optional<std::string> &locale, Documentation &documentation, const Mod &mod,
                                                  const std::string &installationToken) {
        if (locale) {
            const auto [hasLocale, hasLocaleError](co_await documentation.hasAvailableLocale(mod, *locale, installationToken));
            if (hasLocale) {
                co_return locale;
            }
        }
        co_return std::nullopt;
    }

    Task<std::optional<std::string>> getDocsVersion(const std::optional<std::string> &version, Documentation &documentation, const Mod &mod,
                                                    const std::string &installationToken) {
        if (version) {
            const auto [versions, versionsError](co_await documentation.getAvailableVersionsFiltered(mod, installationToken));
            if (versions) {
                for (auto it = versions->begin(); it != versions->end(); it++) {
                    if (it.key().isString() && it->isString() && it.key().asString() == *version) {
                        co_return it->asString();
                    }
                }
            }
        }
        co_return std::nullopt;
    }

    DocsController::DocsController(GitHub &g, Database &db, Documentation &d) : github_(g), database_(db), documentation_(d) {}

    Task<> DocsController::mod(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
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

            const auto [isPublic, publicError](co_await github_.isPublicRepository(modResult->getValueOfSourceRepo(), *installationToken));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(*modResult, *installationToken));

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
                if (versions) {
                    modJson["versions"] = *versions;
                }
                root["mod"] = modJson;
            }

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

    Task<> DocsController::page(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string mod) const {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
            }

            std::string prefix = std::format("/api/v1/mod/{}/page/", mod);
            std::string path = req->getPath().substr(prefix.size());

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

            const auto locale =
                    co_await assertLocale(req->getOptionalParameter<std::string>("locale"), documentation_, *modResult, *installationToken);
            const auto ref = (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, *modResult, *installationToken))
                                     .value_or(modResult->getValueOfSourceBranch());
            const auto prefixedPath = *modResult->getSourcePath() + '/' + path;
            const auto [contents,
                        contentsError](co_await documentation_.getDocumentationPage(*modResult, path, locale, ref, *installationToken));
            if (!contents) {
                co_return errorResponse(contentsError, "File not found", callback);
            }

            const auto [updatedAt,
                        updatedAtError](co_await github_.getFileLastUpdateTime(repo, ref, prefixedPath, installationToken.value()));

            const auto [isPublic, publicError](co_await github_.isPublicRepository(modResult->getValueOfSourceRepo(), *installationToken));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(*modResult, *installationToken));

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
                if (versions) {
                    modJson["versions"] = *versions;
                }
                root["mod"] = modJson;
            }
            root["content"] = (*contents)["content"];
            if (isPublic) {
                root["edit_url"] = (*contents)["html_url"];
            }
            if (updatedAt) {
                root["updated_at"] = *updatedAt;
            }

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

    Task<> DocsController::tree(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string mod) const {
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

            const auto locale =
                    co_await assertLocale(req->getOptionalParameter<std::string>("locale"), documentation_, *modResult, *installationToken);
            const auto version = (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, *modResult, *installationToken))
                                         .value_or(modResult->getValueOfSourceBranch());
            const auto [contents, contentsError](co_await documentation_.getDirectoryTree(*modResult, version, locale, *installationToken));
            if (!contents) {
                co_return errorResponse(contentsError, "Error getting dir tree", callback);
            }

            const auto [isPublic, publicError](co_await github_.isPublicRepository(modResult->getValueOfSourceRepo(), *installationToken));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(*modResult, *installationToken));

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
                if (versions) {
                    modJson["versions"] = *versions;
                }
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

    Task<> DocsController::asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const {
        try {
            if (mod.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
            }

            std::string prefix = std::format("/api/v1/mod/{}/asset/", mod);
            std::string location = req->getPath().substr(prefix.size());

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

            const auto version = (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, *modResult, *installationToken))
                                         .value_or(modResult->getValueOfSourceBranch());
            const auto [asset,
                        assetError](co_await documentation_.getAssetResource(*modResult, *resourceLocation, version, *installationToken));
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

    // TODO Invalidation rate limit
    Task<> DocsController::invalidate(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const {
        if (mod.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing mod parameter", callback);
        }

        const auto [modResult, modError] = co_await database_.getModSource(mod);
        if (!modResult) {
            co_return errorResponse(modError, "Mod not found", callback);
        }

        co_await github_.invalidateCache(modResult->getValueOfSourceRepo());
        co_await documentation_.invalidateCache(*modResult);

        Json::Value root;
        root["message"] = "Caches for mod invalidated successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
