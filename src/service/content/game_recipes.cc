#include "game_recipes.h"
#include "game_content.h"

#include <content/builtin_recipe_type.h>
#include <drogon/HttpAppFramework.h>
#include <models/Recipe.h>
#include <schemas.h>

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

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

using RecipeProcessor = std::function<std::optional<ModRecipe>(const std::string &id, const std::string &type, const nlohmann::json &json)>;

struct RecipeType {
    nlohmann::json displaySchema;
    RecipeProcessor processor;
};

std::optional<RecipeIngredient> readRecipeIngredient(const nlohmann::json &json, const int slot) {
    std::string itemId;
    bool isTag;
    if (json.contains("item")) {
        itemId = json["item"];
        isTag = false;
    } else {
        itemId = json["tag"];
        isTag = true;
    }
    return RecipeIngredient{itemId, slot, 1, true, isTag};
}

// TODO Support 1.21.4+ recipe ingredients
std::optional<ModRecipe> readShapedCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json) {
    const auto keys = json.at("key");
    const auto pattern = json.at("pattern");
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
            const auto ingredient = readRecipeIngredient(keyValue, i + 1);
            if (!ingredient) {
                logger.info("Invalid ingredient, skipping recipe"); // TODO Project logger
                return std::nullopt;
            }
            recipe.ingredients.push_back(*ingredient);
        }
    }

    const auto result = json.at("result");
    const auto resultId = result["id"].get<std::string>();
    const auto count = result["count"].get<int>();
    recipe.ingredients.emplace_back(resultId, 1, count, false, false);

    return recipe;
}

std::optional<ModRecipe> readShapelessCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json) {
    ModRecipe recipe{.id = id, .type = type};

    const auto ingredients = json["ingredients"];
    for (int i = 0; i < ingredients.size(); ++i) {
        const auto ingredient = readRecipeIngredient(ingredients[i], i + 1);
        if (!ingredient) {
            logger.info("Invalid ingredient, skipping recipe"); // TODO Project logger
            return std::nullopt;
        }
        recipe.ingredients.push_back(*ingredient);
    }

    const auto result = json["result"];
    const auto resultId = result["id"].get<std::string>();
    const auto count = result["count"].get<int>();
    recipe.ingredients.emplace_back(resultId, 1, count, false, false);

    return recipe;
}

std::unordered_map<std::string, RecipeType> recipeTypes{
    {"minecraft:crafting_shaped", {recipe_type::builtin::shapedCrafting, readShapedCraftingRecipe}},
    {"minecraft:crafting_shapeless", {recipe_type::builtin::shapedCrafting, readShapelessCraftingRecipe}}};

namespace content {
    std::optional<Json::Value> getRecipeType(const std::string &type) {
        if (const auto knownType = recipeTypes.find(type); knownType != recipeTypes.end()) {
            return unparkourJson(knownType->second.displaySchema);
        }
        return std::nullopt;
    }

    std::optional<ModRecipe> readRecipe(const std::string &namespace_, const fs::path &root, const fs::path &path) {
        fs::path relativePath = relative(path, root);
        const auto fileName = relativePath.string();
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

        // Ingest recipe
        const auto type = json->at("type").get<std::string>();
        if (const auto knownType = recipeTypes.find(type); knownType != recipeTypes.end()) {
            return knownType->second.processor(id, type, *json);
        }

        return std::nullopt;
    }

    // TODO How about nonexistent items, tags or so?
    Task<Error> Ingestor::ingestRecipes() const {
        const auto clientPtr = app().getFastDbClient();

        const auto projectId = project_.getProject().getValueOfId();
        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto modid = project_.getProject().getValueOfModid();
        const auto recipesRoot = docsRoot / ".data" / modid / "recipe";

        if (!exists(recipesRoot)) {
            co_return Error::Ok;
        }

        std::vector<ModRecipe> recipes;
        for (const auto &entry: fs::recursive_directory_iterator(recipesRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (const auto recipe = readRecipe(modid, recipesRoot, entry.path())) {
                recipes.push_back(*recipe);
            } else {
                logger_->warn("Skipping recipe file: '{}'", entry.path().filename().string());
            }
        }

        logger_->info("Importing {} recipes from project {}", recipes.size(), projectId);

        try {
            // 1. Create items
            std::set<std::string> newItems;
            for (auto &recipe: recipes) {
                for (const auto &item: recipe.ingredients) {
                    if (const auto parsedId = ResourceLocation::parse(item.itemId); parsedId && parsedId->namespace_ == modid) {
                        newItems.insert(item.itemId);
                    }
                }
            }

            logger_->info("Adding {} items found in recipes", newItems.size());

            for (auto &item: newItems) {
                co_await addProjectItem(clientPtr, projectId, item);
            }

            // 2. Create recipes
            CoroMapper<Recipe> recipeMapper(clientPtr);
            for (auto &[id, type, ingredients]: recipes) {
                // language=postgresql
                const auto row = co_await clientPtr->execSqlCoro(
                    "INSERT INTO recipe VALUES (DEFAULT, $1, $2, $3) ON CONFLICT DO NOTHING RETURNING id", projectId, id, type);
                const auto recipeId = row.front().at("id").as<int64_t>();

                for (auto &[ingredientId, slot, count, input, isTag]: ingredients) {
                    try {
                        if (isTag) {
                            // language=postgresql
                            co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_tag (recipe_id, tag_id, slot, count, input) \
                                                             SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                                             FROM tag \
                                                             WHERE tag.loc = $2;",
                                                            recipeId, ingredientId, slot, count, input);
                        } else {
                            // language=postgresql
                            co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_item (recipe_id, item_id, slot, count, input) \
                                                             SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                                             FROM item \
                                                             WHERE item.loc = $2;",
                                                            recipeId, ingredientId, slot, count, input);
                        }
                        continue;
                    } catch (const std::exception &e) {
                        const auto ingredientName = isTag ? "#" + ingredientId : ingredientId;
                        logger_->warn("Removing recipe '{}' due to missing ingredient '{}'", id, ingredientName);
                    }
                    co_await recipeMapper.deleteByPrimaryKey(recipeId);
                    break;
                }
            }
        } catch (const std::exception &e) {
            logger_->error("Failed to add items to transaction: {}", e.what());
        }

        co_return Error::Ok;
    }
}
