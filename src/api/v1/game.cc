#include "game.h"

#include <string>

#include <database/resolved_db.h>
#include "content/recipe_parser.h"
#include "error.h"
#include "lang/lang.h"

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

        callback(jsonResponse(*contents));
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
        root["properties"] = unparkourJson(co_await resolved.readItemProperties(id));
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
        nlohmann::json root(nlohmann::json::value_t::array);
        for (const auto &id: recipeIds) {
            if (const auto recipe = co_await resolved.getRecipe(id)) {
                root.emplace_back(*recipe);
            } else {
                logger.error("Missing recipe {} for item {} / {}", id, project, item);
            }
        }
        callback(jsonResponse(root));
    }

    Task<nlohmann::json> resolveContentUsage(const ResolvedProject &resolved, std::vector<ContentUsage> items) {
        nlohmann::json root(nlohmann::json::value_t::array);
        for (const auto &[id, loc, project, path]: items) {
            const auto dbItem = co_await global::database->getByPrimaryKey<Item>(id);
            // TODO Universal locale service
            const auto name = project.empty() ? co_await global::lang->getItemName(resolved.getLocale(), loc)
                : (co_await resolved.getItemName(dbItem)).name;

            nlohmann::json itemJson;
            itemJson["project"] = project.empty() ? nlohmann::json(nullptr) : nlohmann::json(project);
            itemJson["id"] = loc;
            if (!name.empty()) {
                itemJson["name"] = name;
            }
            itemJson["has_page"] = !path.empty();
            root.push_back(itemJson);
        }
        co_return root;
    }

    Task<> GameController::contentItemUsage(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string project, const std::string item) const {
        if (item.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto obtainable = co_await global::database->getObtainableItemsBy(item);

        const auto json = co_await resolveContentUsage(resolved, obtainable);
        callback(jsonResponse(json));
    }

    Task<> GameController::recipe(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (recipe.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }
        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto resolvedResult = co_await resolved.getRecipe(recipe);
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

        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, project);
        const auto recipeType = co_await resolved.getProjectDatabase().getRecipeType(type);
        if (!recipeType) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }
        const auto layout = content::getRecipeType(resolved, unwrap(ResourceLocation::parse(type)));
        if (!layout) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }
        const auto workbenches = co_await global::database->getRecipeTypeWorkbenches(recipeType->getValueOfId());

        const auto workbenchItems = co_await resolveContentUsage(resolved, workbenches);

        nlohmann::json root;
        root["type"] = *layout;
        root["workbenches"] = workbenchItems;

        callback(jsonResponse(root));
    }
}
