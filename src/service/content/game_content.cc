#include "game_content.h"

#include <drogon/drogon.h>
#include <fstream>
#include <models/Recipe.h>
#include <schemas.h>

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
namespace fs = std::filesystem;

// TODO Reserved modids
namespace content {
    Ingestor::Ingestor(Database &db) : database_(db) {}

    struct RecipeIngredient {
        std::string itemId;
        int slot;
        int count;
        bool input;
        bool isTag;
    };

    struct ModRecipe {
        std::string id;
        std::string type;
        std::vector<RecipeIngredient> ingredients;
    };

    Task<Error> wipeExistingData(const DbClientPtr clientPtr, const std::string id) {
        logger.info("Wiping existing data for project {}", id);
        co_await clientPtr->execSqlCoro("DELETE FROM recipe WHERE project_id = $1", id);
        co_await clientPtr->execSqlCoro("DELETE FROM item WHERE project_id = $1", id);
        co_await clientPtr->execSqlCoro("DELETE FROM tag WHERE project_id = $1", id);
        // TODO Remove redundant item_id and tag_id
        co_return Error::Ok;
    }

    std::optional<ModRecipe> readRecipe(const std::string &namespace_, const fs::path &path) {
        const auto fileName = path.filename().string();
        const auto id = namespace_ + ":" + fileName.substr(0, fileName.find_last_of('.'));

        // Read and validate JSON
        const auto json = parseJsonFile(path);
        if (!json) {
            logger.warn("Skipping invalid recipe {}", id);
            return std::nullopt;
        }
        if (const auto error = validateJson(schemas::gameRecipe, *json)) {
            logger.warn("Skipping non-matching recipe {}", id);
            return std::nullopt;
        }

        const auto type = json->at("type").get<std::string>();
        const auto keys = json->at("key");
        const auto result = json->at("result");
        const auto pattern = json->at("pattern");
        std::string concat = "";
        for (auto &row: pattern) {
            concat += row;
        }
        ModRecipe recipe{.id = id, .type = type};

        for (int i = 0; i < concat.size(); ++i) {
            std::string key{concat[i]};
            if (key.empty() || !keys.contains(key)) {
                continue;
            }

            if (const auto keyValue = keys[key]; keyValue.is_array()) {
                for (const auto &item: keyValue) {
                    recipe.ingredients.emplace_back(item, i + 1, 1, true, false);
                }
            } else {
                std::string itemId;
                bool isTag;
                if (keyValue.contains("item")) {
                    itemId = keyValue["item"];
                    isTag = false;
                } else {
                    itemId = keyValue["tag"];
                    isTag = true;
                }
                recipe.ingredients.emplace_back(itemId, i + 1, 1, true, isTag);
            }
        }

        const auto resultId = result["id"].get<std::string>();
        const auto count = result["count"].get<int>();
        recipe.ingredients.emplace_back(resultId, 1, count, false, false);

        return recipe;
    }

    Task<Error> Ingestor::ingestGameContentData(const ResolvedProject project) const {
        const auto projectId = project.getProject().getValueOfId();
        logger.info("Ingesting game data for project {}", projectId);

        const auto docsRoot = project.getDocsDirectoryPath();
        const auto modid = project.getProject().getValueOfModid();
        const auto recipesRoot = docsRoot / ".data" / modid / "recipe";

        if (!exists(recipesRoot)) {
            co_return Error::Ok;
        }

        const auto clientPtr = app().getFastDbClient();
        co_await wipeExistingData(clientPtr, projectId);

        std::vector<ModRecipe> recipes;
        // TODO Recursion
        for (const auto &entry: fs::directory_iterator(recipesRoot)) {
            if (const auto recipe = readRecipe(modid, entry.path())) {
                recipes.push_back(*recipe);
            } else {
                logger.warn("Skipping recipe file: {}", entry.path().filename().string());
            }
        }

        logger.info("Importing {} recipes from project {}", recipes.size(), projectId);

        try {
            // 1. Create items
            std::set<std::string> newItems;
            for (auto &recipe: recipes) {
                for (const auto &item: recipe.ingredients) {
                    const auto parsedId = ResourceLocation::parse(item.itemId);
                    if (parsedId && parsedId->namespace_ == modid) {
                        newItems.insert(item.itemId);
                    }
                }
            }

            logger.info("Adding {} items", newItems.size());

            for (auto &item: newItems) {
                co_await clientPtr->execSqlCoro("INSERT INTO item_id VALUES ($1) ON CONFLICT DO NOTHING", item);
                co_await clientPtr->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1, $2) ON CONFLICT DO NOTHING", item, projectId);
            }

            // 2. Create recipes
            CoroMapper<Recipe> recipeMapper(clientPtr);
            for (auto &[id, type, ingredients]: recipes) {
                const auto row = co_await clientPtr->execSqlCoro("INSERT INTO recipe VALUES (DEFAULT, $1, $2, $3) ON CONFLICT DO NOTHING RETURNING id", projectId, id, type);
                const auto recipeId = row.front().at("id").as<int64_t>();

                for (auto &[ingredientId, slot, count, input, isTag]: ingredients) {
                    try {
                        if (isTag) {
                            co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_tag VALUES ($1, $2, $3, $4, $5)", recipeId, ingredientId, slot, count, input);
                        } else {
                            co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_item VALUES ($1, $2, $3, $4, $5)", recipeId, ingredientId, slot, count, input);
                        }
                        continue;
                    } catch (const std::exception &e) {
                        logger.info("Removing recipe {} due to missing ingredient", id);
                    }
                    co_await recipeMapper.deleteByPrimaryKey(recipeId);
                    break;
                }
            }
        } catch (const std::exception &e) {
            logger.error("Failed to add items to transaction: {}", e.what());
        }

        co_return Error::Ok;
    }
}
