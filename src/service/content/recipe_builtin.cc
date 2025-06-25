#include <content/builtin_recipe_type.h>
#include <database/database.h>
#include <drogon/drogon.h>
#include <schemas.h>
#include "game_content.h"
#include "ingestor_recipes.h"
#include "recipe_builtin.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;

namespace content {
    using RecipeProcessor = std::function<std::optional<ModRecipe>(const std::string &id, const std::string &type,
                                                                   const nlohmann::json &json, const ProjectFileIssueCallback &issues)>;

    struct RecipeType {
        GameRecipeType displaySchema;
        RecipeProcessor processor;
    };

    std::optional<RecipeIngredient> readRecipeIngredient(const nlohmann::json &json, const std::string &slot) {
        std::string itemId;
        bool isTag;
        if (json.is_string()) {
            if (const auto value = json.get<std::string>(); value.starts_with('#')) {
                itemId = value.substr(1);
                isTag = true;
            } else {
                itemId = value;
                isTag = false;
            }
        } else if (json.is_object()) {
            if (json.contains("item")) {
                itemId = json["item"];
                isTag = false;
            } else {
                itemId = json["tag"];
                isTag = true;
            }
        } else {
            return std::nullopt;
        }
        return RecipeIngredient{itemId, slot, 1, true, isTag};
    }

    std::optional<RecipeIngredient> parseRecipeIngredient(const nlohmann::json &json, const std::string &slot,
                                                          const ProjectFileIssueCallback &issues) {
        const auto ingredient = readRecipeIngredient(json, slot);
        if (!ingredient) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_INGREDIENT, json.dump());
            return std::nullopt;
        }
        return ingredient;
    }

    std::optional<ModRecipe> readShapedCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                      const ProjectFileIssueCallback &issues) {
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
                    if (const auto ingredient = parseRecipeIngredient(item, std::to_string(i + 1), issues)) {
                        recipe.ingredients.push_back(*ingredient);
                    } else {
                        return std::nullopt;
                    }
                }
            } else {
                if (const auto ingredient = parseRecipeIngredient(keyValue, std::to_string(i + 1), issues)) {
                    recipe.ingredients.push_back(*ingredient);
                } else {
                    return std::nullopt;
                }
            }
        }

        const auto result = json.at("result");
        const auto resultId = result["id"].get<std::string>();
        const auto count = result["count"].get<int>();
        recipe.ingredients.emplace_back(resultId, "1", count, false, false);

        return recipe;
    }

    std::optional<ModRecipe> readShapelessCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                         const ProjectFileIssueCallback &issues) {
        ModRecipe recipe{.id = id, .type = type};

        const auto ingredients = json["ingredients"];
        for (int i = 0; i < ingredients.size(); ++i) {
            const auto ingredient = parseRecipeIngredient(ingredients[i], std::to_string(i + 1), issues);
            if (!ingredient) {
                return std::nullopt;
            }
            recipe.ingredients.push_back(*ingredient);
        }

        const auto result = json["result"];
        const auto resultId = result["id"].get<std::string>();
        const auto count = result["count"].get<int>();
        recipe.ingredients.emplace_back(resultId, "1", count, false, false);

        return recipe;
    }

    std::unordered_map<std::string, RecipeType> builtinRecipeTypes;

    void loadRecipeType(const std::string &id, const nlohmann::json &json, const RecipeProcessor &processor) {
        if (const auto error = validateJson(schemas::gameRecipeType, json)) {
            logging::logger.error("Invalid recipe type format.");
            logging::logger.error(error->format());
            throw std::runtime_error("Failed to load recipe type");
        }
        GameRecipeType type = json;
        type.id = id;

        builtinRecipeTypes.emplace(id, RecipeType{.displaySchema = type, .processor = processor});
    }

    void loadBuiltinRecipeTypes() {
        loadRecipeType("minecraft:crafting_shaped", recipe_type::builtin::shapedCrafting, readShapedCraftingRecipe);
        loadRecipeType("minecraft:crafting_shapeless", recipe_type::builtin::shapedCrafting, readShapelessCraftingRecipe);
    }

    bool VanillaRecipeParser::handlesType(const ResourceLocation type) {
        return type.namespace_ == ResourceLocation::DEFAULT_NAMESPACE;
    }

    std::optional<GameRecipeType> VanillaRecipeParser::getType(const ResolvedProject &project, const ResourceLocation type) {
        const auto it = builtinRecipeTypes.find(type);
        return it == builtinRecipeTypes.end() ? std::nullopt : std::make_optional(it->second.displaySchema);
    }

    std::optional<ModRecipe> VanillaRecipeParser::parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                                              const ProjectFileIssueCallback &issues) {
        if (const auto error = validateJson(schemas::gameRecipe, data)) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
            return std::nullopt;
        }

        if (const auto knownType = builtinRecipeTypes.find(type); knownType != builtinRecipeTypes.end()) {
            return knownType->second.processor(id, type, data, issues);
        }

        issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN_RECIPE_TYPE, type);

        return std::nullopt;
    }
}
