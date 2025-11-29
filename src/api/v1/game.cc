#include "game.h"

#include <string>

#include <service/database/project_database.h>
#include <service/content/recipe/recipe_parser.h>
#include <service/system/lang.h>

#include "project/cached.h"

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
        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        requireNonVirtual(resolved);

        const auto contents = co_await resolved->getProjectContents();
        if (!contents) {
            throw ApiException(contents.error(), "Contents directory not found");
        }

        callback(jsonResponse(*contents));
    }

    Task<> GameController::contentItem(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                       const std::string project, const std::string id) const {
        if (id.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        requireNonVirtual(resolved);

        const auto page = co_await resolved->readContentPage(id);
        if (!page) {
            throw ApiException(Error::ErrNotFound, "Content ID not found");
        }

        Json::Value root;
        root["project"] = co_await resolved->toJson();
        root["content"] = page->content;
        root["properties"] = unparkourJson(co_await resolved->readItemProperties(id));
        if (resolved->getProject().getValueOfIsPublic() && !page->editUrl.empty()) {
            root["edit_url"] = page->editUrl;
        }

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> GameController::contentItemRecipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string project, const std::string item) const {
        if (item.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        requireNonVirtual(resolved);

        const auto recipeIds = co_await resolved->getProjectDatabase().getItemRecipes(item);
        nlohmann::json root(nlohmann::json::value_t::array);
        for (const auto &id: recipeIds) {
            if (const auto recipe = co_await resolved->getRecipe(id)) {
                root.emplace_back(*recipe);
            } else {
                logger.error("Missing recipe {} for item {} / {}", id, project, item);
            }
        }
        callback(jsonResponse(root));
    }

    Task<nlohmann::json> resolveContentUsage(std::vector<ContentUsage> items) {
        nlohmann::json root(nlohmann::json::value_t::array);
        for (const auto &[id, loc, project, path]: items) {
            const auto resolved = co_await global::storage->getProject(project, std::nullopt, std::nullopt);
            if (!resolved) {
                logger.error("Missing project for item {}:{}:{}", id, loc, project);
                continue;
            }
            const auto name = co_await (*resolved)->getItemName(loc);

            nlohmann::json itemJson;
            itemJson["project"] = project.empty() ? nlohmann::json(nullptr) : nlohmann::json(project);
            itemJson["id"] = loc;
            if (!name.name.empty()) {
                itemJson["name"] = name.name;
            }
            itemJson["has_page"] = !path.empty();
            root.push_back(itemJson);
        }
        co_return root;
    }

    // TODO Remove unused param
    Task<> GameController::contentItemUsage(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string project, const std::string item) const {
        if (item.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto obtainable = co_await global::database->getObtainableItemsBy(item);

        const auto json = co_await resolveContentUsage(obtainable);
        callback(jsonResponse(json));
    }

    Task<> GameController::recipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (recipe.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        requireNonVirtual(resolved);

        const auto resolvedResult = co_await resolved->getRecipe(recipe);
        if (!resolvedResult) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        callback(jsonResponse(*resolvedResult));
    }

    Task<> GameController::recipeType(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                      const std::string project, const std::string type) const {
        if (type.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        // TODO Use cached + move type getter to project
        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto recipeType = co_await resolved->getProjectDatabase().getRecipeType(type);
        if (!recipeType) {
            throw ApiException(Error::ErrNotFound, "not_found"); // TODO Shorthand
        }

        const auto layout = co_await content::getRecipeType(resolved, unwrap(ResourceLocation::parse(type)));
        if (!layout) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        const auto workbenches = co_await global::database->getRecipeTypeWorkbenches(recipeType->getValueOfId());
        const auto workbenchItems = co_await resolveContentUsage(workbenches);

        nlohmann::json root;
        root["type"] = *layout;
        root["workbenches"] = workbenchItems;

        callback(jsonResponse(root));
    }
}
