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
    DocsController::DocsController(Database &db, Storage &s) : database_(db), storage_(s) {}

    Task<std::optional<ResolvedProject>> DocsController::getProject(const std::string &project, const std::optional<std::string> &version,
                                                                    const std::optional<std::string> &locale,
                                                                    const std::function<void(const HttpResponsePtr &)> callback) const {
        if (project.empty()) {
            errorResponse(Error::ErrBadRequest, "Missing project parameter", callback);
            co_return std::nullopt;
        }

        const auto [proj, projErr] = co_await database_.getProjectSource(project);
        if (!proj) {
            errorResponse(projErr, "Project not found", callback);
            co_return std::nullopt;
        }

        const auto [resolved, resErr](co_await storage_.getProject(*proj, version, locale));
        if (!resolved) {
            errorResponse(resErr, "Resolution failure", callback);
            co_return std::nullopt;
        }

        co_return *resolved;
    }

    Task<> DocsController::project(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto resolved = co_await getProject(project, std::nullopt, std::nullopt, callback);
            if (!resolved) {
                co_return;
            }

            Json::Value root;
            root["project"] = resolved->toJson();

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
            std::string prefix = std::format("/api/v1/project/{}/page/", project);
            std::string path = req->getPath().substr(prefix.size());

            if (path.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing path parameter", callback);
            }

            const auto resolved = co_await getProject(project, req->getOptionalParameter<std::string>("version"),
                                                      req->getOptionalParameter<std::string>("locale"), callback);
            if (!resolved) {
                co_return;
            }

            const auto [page, pageError](resolved->readFile(path));
            if (pageError != Error::Ok) {
                const auto optionalParam = req->getOptionalParameter<std::string>("optional");
                const auto optional = optionalParam.has_value() && optionalParam == "true";

                co_return errorResponse(optional ? Error::Ok : pageError, "File not found", callback);
            }

            Json::Value root;
            root["project"] = resolved->toJson();
            root["content"] = page.content;
            if (resolved->getProject().getValueOfIsPublic() && !page.editUrl.empty()) {
                root["edit_url"] = page.editUrl;
            }
            if (!page.updatedAt.empty()) {
                root["updated_at"] = page.updatedAt;
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

    Task<> DocsController::tree(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                const std::string project) const {
        try {
            const auto resolved = co_await getProject(project, req->getOptionalParameter<std::string>("version"),
                                                      req->getOptionalParameter<std::string>("locale"), callback);
            if (!resolved) {
                co_return;
            }

            const auto [tree, treeError](resolved->getDirectoryTree());
            if (treeError != Error::Ok) {
                co_return errorResponse(treeError, "Error getting directory tree", callback);
            }

            nlohmann::json root;
            root["project"] = parkourJson(resolved->toJson());
            root["tree"] = tree;

            const auto resp = HttpResponse::newHttpJsonResponse(*parseJsonString(root.dump()));
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
            const auto resolved = co_await getProject(project, req->getOptionalParameter<std::string>("version"), std::nullopt, callback);
            if (!resolved) {
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

            const auto assset = resolved->getAsset(*resourceLocation);
            if (!assset) {
                const auto optionalParam = req->getOptionalParameter<std::string>("optional");
                const auto optional = optionalParam.has_value() && optionalParam == "true";

                co_return errorResponse(optional ? Error::Ok : Error::ErrNotFound, "Asset not found", callback);
            }

            const auto response = HttpResponse::newFileResponse(absolute(*assset).string());
            response->setStatusCode(k200OK);
            callback(response);
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }

        co_return;
    }
}
