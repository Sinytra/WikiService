#include "game.h"

#include <string>

#include <database/resolved_db.h>
#include "content/game_recipes.h"
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
        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto [contents, contentsErr] = co_await resolved.getProjectContents();
        if (!contents) {
            throw ApiException(contentsErr, "Contents directory not found");
        }

        callback(HttpResponse::newHttpJsonResponse(unparkourJson(*contents)));
    }

    Task<> GameController::contentItem(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                       const std::string project, const std::string id) const {
        if (id.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);

        const auto [page, pageErr] = co_await resolved.readContentPage(id);
        if (pageErr != Error::Ok) {
            throw ApiException(Error::ErrNotFound, "Content ID not found");
        }

        Json::Value root;
        root["project"] = co_await resolved.toJson();
        root["content"] = page.content;
        if (resolved.getProject().getValueOfIsPublic() && !page.editUrl.empty()) {
            root["edit_url"] = page.editUrl;
        }

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> GameController::contentItemRecipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string project, const std::string item) const {
        if (item.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto recipeIds = co_await resolved.getProjectDatabase().getItemRecipes(item);
        nlohmann::json root;
        for (const auto &id : recipeIds) {
            if (const auto recipe = co_await resolved.getRecipe(id)) { // TODO
                root.emplace_back(*recipe);
            } else {
                logger.error("Missing recipe {} for item {} / {}", id, project, item);
            }
        }
        callback(jsonResponse(root));
    }

    Task<> GameController::contentItemUsage(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string project, const std::string item) const {
        if (item.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto obtainable = co_await global::database->getObtainableItemsBy(item);
        Json::Value root(Json::arrayValue);
        for (const auto &[id, loc, project, path]: obtainable) {
            const auto dbItem = co_await global::database->getByPrimaryKey<Item>(id);
            const auto [name, _] = co_await resolved.getItemName(dbItem);

            Json::Value itemJson;
            itemJson["project"] = project;
            itemJson["id"] = loc;
            if (!name.empty()) {
                itemJson["name"] = name;
            }
            itemJson["has_page"] = !path.empty();
            root.append(itemJson);
        }

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> GameController::recipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (recipe.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getVersionedProject(req, project);
        const auto result = co_await resolved.getProjectDatabase().getProjectRecipe(recipe);
        if (!result) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        const auto locale = req->getOptionalParameter<std::string>("locale");
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolvedResult = co_await content::resolveRecipe(*result, version, locale);
        if (!resolvedResult) {
            throw ApiException(Error::ErrBadRequest, "resolution");
        }

        callback(jsonResponse(*resolvedResult));
    }
}
