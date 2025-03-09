#include "ingestor_recipes.h"
#include <content/builtin_recipe_type.h>
#include <database.h>
#include <drogon/drogon.h>
#include <resolved_db.h>
#include <schemas.h>
#include "game_content.h"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

using RecipeProcessor = std::function<std::optional<ModRecipe>(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                               spdlog::logger &logger)>;

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
std::optional<ModRecipe> readShapedCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json, spdlog::logger &logger) {
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
                logger.info("Invalid ingredient, skipping recipe");
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

std::optional<ModRecipe> readShapelessCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json, spdlog::logger &logger) {
    ModRecipe recipe{.id = id, .type = type};

    const auto ingredients = json["ingredients"];
    for (int i = 0; i < ingredients.size(); ++i) {
        const auto ingredient = readRecipeIngredient(ingredients[i], i + 1);
        if (!ingredient) {
            logger.info("Invalid ingredient, skipping recipe");
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

    std::optional<ModRecipe> readRecipe(const std::string &namespace_, const fs::path &root, const fs::path &path, spdlog::logger &projectLogger) {
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
            return knownType->second.processor(id, type, *json, projectLogger);
        }

        return std::nullopt;
    }

    RecipesSubIngestor::RecipesSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) :
        SubIngestor(proj, log) {}

    Task<PreparationResult> RecipesSubIngestor::prepare() {
        PreparationResult result;

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto modid = project_.getProject().getValueOfModid();
        const auto recipesRoot = docsRoot / ".data" / modid / "recipe";

        if (!exists(recipesRoot)) {
            co_return result;
        }

        for (const auto &entry: fs::recursive_directory_iterator(recipesRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (const auto recipe = readRecipe(modid, recipesRoot, entry.path(), *logger_)) {
                recipes_.push_back(*recipe);

                for (const auto &item: recipe->ingredients) {
                    result.items.insert(item.itemId);
                }
            } else {
                logger_->warn("Skipping recipe file: '{}'", entry.path().filename().string());
            }
        }

        co_return result;
    }

    Task<Error> RecipesSubIngestor::addRecipe(const ModRecipe recipe) const {
        const auto clientPtr = app().getFastDbClient();
        CoroMapper<Recipe> recipeMapper(clientPtr);

        const auto [id, type, ingredients] = recipe;
        const auto projectId = project_.getProject().getValueOfId();

        // language=postgresql
        const auto row =
            co_await clientPtr->execSqlCoro("INSERT INTO recipe VALUES (DEFAULT, $1, $2, $3) RETURNING id", projectId, id, type);
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
            } catch ([[maybe_unused]] const std::exception &e) {
                const auto ingredientName = isTag ? "#" + ingredientId : ingredientId;
                logger_->warn("Skipping recipe '{}' due to missing ingredient '{}'", id, ingredientName);
            }
            co_await recipeMapper.deleteByPrimaryKey(recipeId);
            co_return Error::ErrNotFound;
        }

        co_return Error::Ok;
    }

    Task<Error> RecipesSubIngestor::execute() {
        logger_->info("Importing {} recipes", recipes_.size());

        for (const auto &recipe: recipes_) {
            co_await addRecipe(recipe);
        }

        co_return Error::Ok;
    }

}
