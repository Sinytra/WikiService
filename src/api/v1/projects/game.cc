#include "game.h"

#include <service/database/project_database.h>
#include <service/storage/ingestor/recipe/recipe_parser.h>
#include <service/system/lang.h>
#include <string>

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> GameController::contents(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                    const std::string project) const {
        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        requireNonVirtual(resolved);

        const auto contents = co_await resolved->getProjectContents();
        assertFound(contents);

        callback(jsonResponse(*contents));
    }

    Task<> GameController::contentItem(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                       const std::string project, const std::string id) const {
        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        requireNonVirtual(resolved);

        const auto page = co_await resolved->readContentPage(id);
        assertFound(page, "Content ID not found");

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
        assertNonEmptyParam(item);

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        requireNonVirtual(resolved);

        const auto recipeIds = co_await resolved->getProjectDatabase().getRecipesForItem(item);
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
            const auto itemName = co_await (*resolved)->getItemName(loc);

            nlohmann::json itemJson;
            itemJson["project"] = project.empty() ? nlohmann::json(nullptr) : nlohmann::json(project);
            itemJson["id"] = loc;
            if (itemName) {
                itemJson["name"] = itemName->name;
            }
            itemJson["has_page"] = !path.empty();
            root.push_back(itemJson);
        }
        co_return root;
    }

    Task<> GameController::contentItemUsage(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string project, const std::string item) const {
        assertNonEmptyParam(item);

        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        const auto obtainable = co_await resolved->getProjectDatabase().getObtainableItemsBy(item);

        const auto json = co_await resolveContentUsage(obtainable);
        callback(jsonResponse(json));
    }

    Task<> GameController::contentItemName(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string project, const std::string id) const {
        assertNonEmptyParam(id);

        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        const auto itemName = co_await resolved->getItemName(id);
        assertFound(itemName);

        Json::Value root;
        root["source"] = resolved->getId();
        root["id"] = id;
        root["name"] = itemName->name;

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> GameController::recipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        assertNonEmptyParam(recipe);

        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        requireNonVirtual(resolved);

        const auto resolvedResult = co_await resolved->getRecipe(recipe);
        assertFound(resolvedResult);

        callback(jsonResponse(*resolvedResult));
    }

    Task<> GameController::recipeType(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                      const std::string project, const std::string type) const {
        assertNonEmptyParam(type);

        const auto resolved = co_await BaseProjectController::getProjectWithParamsCached(req, project);
        const auto recipeType = co_await resolved->getProjectDatabase().getRecipeType(type);
        assertFound(recipeType);

        const auto layout = co_await content::getRecipeType(resolved, unwrap(ResourceLocation::parse(type)));
        assertFound(layout);

        const auto workbenches = co_await resolved->getProjectDatabase().getRecipeTypeWorkbenches(recipeType->getValueOfId());
        const auto workbenchItems = co_await resolveContentUsage(workbenches);

        nlohmann::json root;
        root["type"] = *layout;
        root["workbenches"] = workbenchItems;

        callback(jsonResponse(root));
    }
}
