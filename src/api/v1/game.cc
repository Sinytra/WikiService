#include "game.h"

#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

// TODO Have projects include game version info (merge with versions?)
namespace api::v1 {
    Task<> GameController::contents(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                    const std::string project) const {
        const auto resolved = co_await getProjectWithParams(req, callback, project);
        if (!resolved) {
            co_return;
        }

        const auto [contents, contentsErr] = co_await resolved->getProjectContents();
        if (!contents) {
            errorResponse(contentsErr, "Contents directory not found", callback);
            co_return;
        }

        const auto response = HttpResponse::newHttpJsonResponse(unparkourJson(*contents));
        callback(response);

        co_return;
    }

    Task<> GameController::contentItem(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                       const std::string project, const std::string id) const {
        if (id.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto resolved = co_await getProjectWithParams(req, callback, project);
        if (!resolved) {
            co_return;
        }

        const auto [page, pageErr] = co_await resolved->readContentPage(id);
        if (pageErr != Error::Ok) {
            errorResponse(Error::ErrBadRequest, "Content ID not found", callback);
            co_return;
        }

        Json::Value root;
        root["project"] = co_await resolved->toJson();
        root["content"] = page.content;
        if (resolved->getProject().getValueOfIsPublic() && !page.editUrl.empty()) {
            root["edit_url"] = page.editUrl;
        }

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        callback(resp);
    }

    // TODO DB content pages not tied to version
    Task<> GameController::contentItemUsage(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string project, const std::string item) const {
        if (item.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto resolved = co_await getProjectWithParams(req, callback, project);
        if (!resolved) {
            co_return;
        }

        const auto obtainable = co_await global::database->getObtainableItemsBy(item);
        Json::Value root(Json::arrayValue);
        for (const auto &[id, loc, project, path]: obtainable) {
            // TODO
            const auto dbItem = co_await global::database->getByPrimaryKey<Item>(id);
            const auto name = co_await resolved->getItemName(dbItem);

            Json::Value itemJson;
            itemJson["project"] = project;
            itemJson["id"] = loc;
            if (name) {
                itemJson["name"] = *name;
            }
            itemJson["has_page"] = !path.empty();
            root.append(itemJson);
        }
        const auto response = HttpResponse::newHttpJsonResponse(root);
        callback(response);

        co_return;
    }

    Task<> GameController::recipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (recipe.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto resolved = co_await getVersionedProject(req, callback, project);
        if (!resolved) {
            co_return;
        }

        const auto projectRecipe = co_await resolved->getRecipe(recipe);
        if (!projectRecipe) {
            errorResponse(Error::ErrNotFound, "Recipe not found", callback);
            co_return;
        }

        const auto response = HttpResponse::newHttpJsonResponse(*projectRecipe);
        callback(response);

        co_return;
    }
}
