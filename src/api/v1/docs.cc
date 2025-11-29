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
        const auto resolved = co_await BaseProjectController::getProject(project, version, std::nullopt);

        if (version && !co_await resolved->hasVersion(*version)) {
            throw ApiException(Error::ErrNotFound, "Version not found");
        }

        const auto json = co_await resolved->toJsonVerbose();
        callback(HttpResponse::newHttpJsonResponse(json));
    }

    Task<> DocsController::page(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            std::string prefix = std::format("/api/v1/docs/{}/page/", project);
            std::string path = req->getPath().substr(prefix.size());

            if (path.empty()) {
                throw ApiException(Error::ErrBadRequest, "Missing path parameter");
            }

            const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);

            const auto page(resolved->readPageFile(path + DOCS_FILE_EXT));
            if (!page) {
                const auto optionalParam = req->getOptionalParameter<std::string>("optional");
                const auto optional = optionalParam.has_value() && optionalParam == "true";

                throw ApiException(optional ? Error::Ok : page.error(), "File not found");
            }

            Json::Value root;
            root["project"] = co_await resolved->toJson();
            root["content"] = page->content;
            if (resolved->getProject().getValueOfIsPublic() && !page->editUrl.empty()) {
                root["edit_url"] = page->editUrl;
            }

            callback(HttpResponse::newHttpJsonResponse(root));
        } catch (const HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
        }

        co_return;
    }

    Task<> DocsController::tree(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                const std::string project) const {
        auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);

        const auto tree(co_await resolved->getDirectoryTree());
        if (!tree) {
            throw ApiException(tree.error(), "Error getting directory tree");
        }

        nlohmann::json root;
        root["project"] = parkourJson(co_await resolved->toJson());
        root["tree"] = tree;

        callback(jsonResponse(root));
    }

    Task<> DocsController::asset(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string project) const {
        try {
            const auto resolved = co_await BaseProjectController::getProject(project, req->getOptionalParameter<std::string>("version"), std::nullopt);

            std::string prefix = std::format("/api/v1/docs/{}/asset/", project);
            std::string location = req->getPath().substr(prefix.size());

            if (location.empty()) {
                throw ApiException(Error::ErrBadRequest, "Missing location parameter");
            }

            const auto resourceLocation = ResourceLocation::parse(location);
            if (!resourceLocation) {
                throw ApiException(Error::ErrBadRequest, "Invalid location specified");
            }

            const auto asset = resolved->getAsset(*resourceLocation);
            if (!asset) {
                const auto optionalParam = req->getOptionalParameter<std::string>("optional");
                const auto optional = optionalParam.has_value() && optionalParam == "true";

                throw ApiException(optional ? Error::Ok : Error::ErrNotFound, "Asset not found");
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
