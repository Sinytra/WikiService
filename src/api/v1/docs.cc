#include "docs.h"

#include <drogon/HttpClient.h>
#include <models/Project.h>

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
    Task<std::optional<std::string>> assertLocale(const std::optional<std::string> &locale, Documentation &documentation,
                                                  const Project &project, const std::string &installationToken) {
        if (locale) {
            if (const auto [hasLocale, hasLocaleError](co_await documentation.hasAvailableLocale(project, *locale, installationToken));
                hasLocale)
            {
                co_return locale;
            }
        }
        co_return std::nullopt;
    }

    Task<std::optional<std::string>> getDocsVersion(const std::optional<std::string> &version, Documentation &documentation,
                                                    const Project &project, const std::string &installationToken) {
        if (version) {
            if (const auto [versions, versionsError](co_await documentation.getAvailableVersionsFiltered(project, installationToken));
                versions)
            {
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

    Task<std::optional<ProjectDetails>> DocsController::getProject(const std::string &project,
                                                                   std::function<void(const HttpResponsePtr &)> callback) const {
        if (project.empty()) {
            errorResponse(Error::ErrBadRequest, "Missing project parameter", callback);
            co_return std::nullopt;
        }

        const auto [proj, projErr] = co_await database_.getProjectSource(project);
        if (!proj) {
            errorResponse(projErr, "Project not found", callback);
            co_return std::nullopt;
        }
        const std::string repo = *proj->getSourceRepo();

        const auto [installationId, idErr](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            errorResponse(idErr, "Missing repository installation", callback);
            co_return std::nullopt;
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            errorResponse(tokenErr, "GitHub authentication failure", callback);
            co_return std::nullopt;
        }

        co_return ProjectDetails{*proj, *installationToken};
    }

    Task<> DocsController::project(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto proj = co_await getProject(project, callback);
            if (!proj) {
                co_return;
            }

            const auto [isPublic, publicError](co_await github_.isPublicRepository(proj->project.getValueOfSourceRepo(), proj->token));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(proj->project, proj->token));

            Json::Value root;
            {
                Json::Value projectJson = proj->project.toJson();
                projectJson["is_public"] = isPublic;
                if (versions) {
                    projectJson["versions"] = *versions;
                }
                root["project"] = projectJson;
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

    Task<> DocsController::page(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto proj = co_await getProject(project, callback);
            if (!proj) {
                co_return;
            }

            std::string prefix = std::format("/api/v1/project/{}/page/", project);
            std::string path = req->getPath().substr(prefix.size());

            if (path.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing path parameter", callback);
            }

            const auto locale =
                co_await assertLocale(req->getOptionalParameter<std::string>("locale"), documentation_, proj->project, proj->token);
            const auto ref =
                (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, proj->project, proj->token))
                    .value_or(proj->project.getValueOfSourceBranch());
            const auto prefixedPath = proj->project.getValueOfSourcePath() + '/' + path;
            const auto [contents,
                        contentsError](co_await documentation_.getDocumentationPage(proj->project, path, locale, ref, proj->token));
            if (!contents) {
                co_return errorResponse(contentsError, "File not found", callback);
            }

            const auto [updatedAt, updatedAtError](
                co_await github_.getFileLastUpdateTime(proj->project.getValueOfSourceRepo(), ref, prefixedPath, proj->token));

            const auto [isPublic, publicError](co_await github_.isPublicRepository(proj->project.getValueOfSourceRepo(), proj->token));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(proj->project, proj->token));

            Json::Value root;
            {
                Json::Value projectJson = proj->project.toJson();
                projectJson["is_public"] = isPublic;
                if (versions) {
                    projectJson["versions"] = *versions;
                }
                root["project"] = projectJson;
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

    Task<> DocsController::tree(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto proj = co_await getProject(project, callback);
            if (!proj) {
                co_return;
            }

            const auto locale =
                co_await assertLocale(req->getOptionalParameter<std::string>("locale"), documentation_, proj->project, proj->token);
            const auto version =
                (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, proj->project, proj->token))
                    .value_or(proj->project.getValueOfSourceBranch());
            const auto [contents, contentsError](co_await documentation_.getDirectoryTree(proj->project, version, locale, proj->token));
            if (contentsError != Error::Ok) {
                co_return errorResponse(contentsError, "Error getting dir tree", callback);
            }

            const auto [isPublic, publicError](co_await github_.isPublicRepository(proj->project.getValueOfSourceRepo(), proj->token));
            const auto [versions, versionsError](co_await documentation_.getAvailableVersionsFiltered(proj->project, proj->token));

            nlohmann::json root;
            {
                Json::Value projectJson = proj->project.toJson();
                projectJson["is_public"] = isPublic;
                if (versions) {
                    projectJson["versions"] = *versions;
                }
                root["project"] = parkourJson(projectJson);
            }
            root["tree"] = contents;

            const auto resp = jsonResponse(root);
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

    Task<> DocsController::asset(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto proj = co_await getProject(project, callback);
            if (!proj) {
                co_return;
            }

            std::string prefix = std::format("/api/v1/project/{}/asset/", project);
            std::string location = req->getPath().substr(prefix.size());

            if (location.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing location parameter", callback);
            }

            const auto resourceLocation = ResourceLocation::parse(location);
            if (!resourceLocation) {
                co_return errorResponse(Error::ErrBadRequest, "Invalid location specified", callback);
            }

            const auto version =
                (co_await getDocsVersion(req->getOptionalParameter<std::string>("version"), documentation_, proj->project, proj->token))
                    .value_or(proj->project.getValueOfSourceBranch());
            const auto [asset,
                        assetError](co_await documentation_.getAssetResource(proj->project, *resourceLocation, version, proj->token));
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
