#include "game.h"

#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    // TODO Version and lang
    Task<> GameController::contents(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                    const std::string project) const {
        const auto resolved = co_await getProject(project, std::nullopt, std::nullopt, callback);
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

    Task<> GameController::contentItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, const std::string project,
                                       const std::string id) const {
        if (id.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto resolved = co_await getProject(project, std::nullopt, std::nullopt, callback);
        if (!resolved) {
            co_return;
        }

        const auto [page, pageErr] = co_await resolved->readContentPage(id);
        if (pageErr != Error::Ok) {
            errorResponse(Error::ErrBadRequest, "Content ID not found", callback);
            co_return;
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
        callback(resp);
    }

    // TODO have projects include game version info (merge with versions?)
    // TODO version param
    // TODO How about nonexistent items, tags or so?
    Task<> GameController::recipe(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (recipe.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto resolved = co_await getProject(project, std::nullopt, std::nullopt, callback);
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

    Task<> GameController::itemUsage(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                     const std::string project, const std::string item) const {
        if (project.empty() || item.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto recipes = co_await global::database->getItemUsageInRecipes(item);
        Json::Value root(Json::arrayValue);
        for (const auto &recipe: recipes) {
            Json::Value recipeJson;
            recipeJson["project"] = recipe.getValueOfProjectId();
            recipeJson["id"] = recipe.getValueOfLoc();
            root.append(recipeJson);
        }
        const auto response = HttpResponse::newHttpJsonResponse(root);
        callback(response);

        co_return;
    }
}
