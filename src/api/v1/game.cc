#include "game.h"

#include <models/RecipeIngredientItem.h>
#include <models/RecipeIngredientTag.h>
#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    GameController::GameController(Database &db, Storage &s) : database_(db), storage_(s) {}

    // TODO Use universal field for displayed items
    template<typename T>
    Task<Json::Value> appendSources(const std::string col, const std::string item, const Database &db) {
        const auto items = co_await db.getRelated<T>(col, item);
        Json::Value root(Json::arrayValue);
        for (const auto &res: items) {
            if (!res.getValueOfProjectId().empty()) {
                root.append(res.getValueOfProjectId());
            }
        }
        co_return root;
    }

    Task<Json::Value> ingredientToJson(const int slot, const std::vector<RecipeIngredientItem> items, const Database &db) {
        Json::Value root;
        root["slot"] = slot;
        Json::Value itemsJson(Json::arrayValue);
        for (const auto &item: items) {
            Json::Value itemJson;
            itemJson["item"] = item.getValueOfItemId();
            itemJson["count"] = item.getValueOfCount();
            itemJson["sources"] = co_await appendSources<Item>(RecipeIngredientItem::Cols::_item_id, item.getValueOfItemId(), db);
            itemsJson.append(itemJson);
        }
        root["items"] = itemsJson;
        co_return root;
    }

    Task<Json::Value> ingredientToJson(const RecipeIngredientTag &tag, const Database &db) {
        Json::Value root;
        {
            Json::Value tagJson;
            tagJson["id"] = tag.getValueOfTagId();
            {
                Json::Value itemsJson(Json::arrayValue);
                for (const auto tagItems = co_await db.getTagItemsFlat(tag.getValueOfTagId()); const auto &[itemId]: tagItems) {
                    Json::Value itemJson;
                    itemJson["id"] = itemId;
                    itemJson["sources"] = co_await appendSources<Item>(RecipeIngredientItem::Cols::_item_id, itemId, db);
                    itemsJson.append(itemJson);
                }
                tagJson["items"] = itemsJson;
            }
            root["tag"] = tagJson;
        }
        root["count"] = tag.getValueOfCount();
        root["slot"] = tag.getValueOfSlot();
        co_return root;
    }

    std::map<int, std::vector<RecipeIngredientItem>> groupIngredients(const std::vector<RecipeIngredientItem> &ingredients, const bool input) {
        std::map<int, std::vector<RecipeIngredientItem>> slots;
        for (auto &ingredient: ingredients) {
            if (ingredient.getValueOfInput() == input) {
                slots[ingredient.getValueOfSlot()].emplace_back(ingredient);
            }
        }
        return slots;
    }

    // TODO have projects include game version info (merge with versions?)
    // TODO version param
    // TODO How about nonexistent items, tags or so?
    Task<> GameController::recipe(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                  const std::string project, const std::string recipe) const {
        if (project.empty() || recipe.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto result = co_await database_.getProjectRecipe(project, recipe);
        if (!result) {
            errorResponse(Error::ErrNotFound, "Project or recipe not found", callback);
            co_return;
        }
        const auto itemIngredients =
            co_await database_.getRelated<RecipeIngredientItem>(RecipeIngredientItem::Cols::_recipe_id, result->getValueOfId());
        const auto tagIngredients =
            co_await database_.getRelated<RecipeIngredientTag>(RecipeIngredientTag::Cols::_recipe_id, result->getValueOfId());

        Json::Value json;
        json["type"] = result->getValueOfType();
        {
            Json::Value inputs;
            Json::Value outputs;
            std::map<int, std::vector<RecipeIngredientItem>> itemInputSlots = groupIngredients(itemIngredients, true);
            std::map<int, std::vector<RecipeIngredientItem>> itemOutputSlots = groupIngredients(itemIngredients, false);

            for (const auto &[slot, items]: itemInputSlots) {
                const auto itemJson = co_await ingredientToJson(slot, items, database_);
                inputs.append(itemJson);
            }
            for (const auto &tag : tagIngredients) {
                const auto tagJson = co_await ingredientToJson(tag, database_);
                inputs.append(tagJson);
            }
            for (const auto &[slot, items]: itemOutputSlots) {
                const auto itemJson = co_await ingredientToJson(slot, items, database_);
                outputs.append(itemJson);
            }
            json["inputs"] = inputs;
            json["outputs"] = outputs;
        }
        const auto response = HttpResponse::newHttpJsonResponse(json);
        callback(response);

        co_return;
    }

    Task<> GameController::itemUsage(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                     const std::string project, const std::string item) const {
        if (project.empty() || item.empty()) {
            errorResponse(Error::ErrBadRequest, "Insufficient parameters", callback);
            co_return;
        }

        const auto recipes = co_await database_.getItemUsageInRecipes(item);
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
