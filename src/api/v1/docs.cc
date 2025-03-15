#include "docs.h"
#include "error.h"

#include <drogon/HttpClient.h>
#include <models/Project.h>
#include <service/util.h>
#include <string>

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> DocsController::project(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                   const std::string project) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getProject(project, version, std::nullopt, callback);
        if (!resolved) {
            co_return;
        }

        if (version && !co_await resolved->hasVersion(*version)) {
            errorResponse(Error::ErrNotFound, "Version not found", callback);
            co_return;
        }

        const auto json = co_await resolved->toJsonVerbose();
        const auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }

    Task<> DocsController::page(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            std::string prefix = std::format("/api/v1/docs/{}/page/", project);
            std::string path = req->getPath().substr(prefix.size());

            if (path.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing path parameter", callback);
            }

            const auto resolved = co_await BaseProjectController::getProject(project, req->getOptionalParameter<std::string>("version"),
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
            root["project"] = co_await resolved->toJson();
            root["content"] = page.content;
            if (resolved->getProject().getValueOfIsPublic() && !page.editUrl.empty()) {
                root["edit_url"] = page.editUrl;
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

    Task<> DocsController::tree(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                const std::string project) const {
        const auto resolved = co_await BaseProjectController::getProject(project, req->getOptionalParameter<std::string>("version"),
                                                                         req->getOptionalParameter<std::string>("locale"), callback);
        if (!resolved) {
            co_return;
        }

        const auto [tree, treeError](resolved->getDirectoryTree());
        if (treeError != Error::Ok) {
            co_return errorResponse(treeError, "Error getting directory tree", callback);
        }

        nlohmann::json root;
        root["project"] = parkourJson(co_await resolved->toJson());
        root["tree"] = tree;

        const auto resp = HttpResponse::newHttpJsonResponse(unparkourJson(root));
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }

    Task<> DocsController::asset(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto resolved = co_await BaseProjectController::getProject(project, req->getOptionalParameter<std::string>("version"),
                                                                             std::nullopt, callback);
            if (!resolved) {
                co_return;
            }

            std::string prefix = std::format("/api/v1/docs/{}/asset/", project);
            std::string location = req->getPath().substr(prefix.size());

            if (location.empty()) {
                co_return errorResponse(Error::ErrBadRequest, "Missing location parameter", callback);
            }

            const auto resourceLocation = ResourceLocation::parse(location);
            if (!resourceLocation) {
                co_return errorResponse(Error::ErrBadRequest, "Invalid location specified", callback);
            }

            const auto asset = resolved->getAsset(*resourceLocation);
            if (!asset) {
                const auto optionalParam = req->getOptionalParameter<std::string>("optional");
                const auto optional = optionalParam.has_value() && optionalParam == "true";

                co_return errorResponse(optional ? Error::Ok : Error::ErrNotFound, "Asset not found", callback);
            }

            const auto response = HttpResponse::newFileResponse(absolute(*asset).string());
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
