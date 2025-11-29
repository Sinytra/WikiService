#include "recipe_builtin.h"

#include <schemas/schemas.h>
#include <service/content/recipe/builtin_recipe_type.h>

using namespace drogon;
using namespace drogon::orm;
using namespace service;

namespace content {
    using RecipeProcessor = std::function<std::optional<StubRecipe>(const std::string &id, const std::string &type,
                                                                    const nlohmann::json &json, const ProjectFileIssueCallback &issues)>;

    struct RecipeType {
        GameRecipeType displaySchema;
        RecipeProcessor processor;
    };

    std::optional<StubRecipeIngredient> readRecipeIngredient(const nlohmann::json &json, const std::string &slot) {
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
        return StubRecipeIngredient{itemId, slot, 1, true, isTag};
    }

    std::optional<StubRecipeIngredient> parseRecipeIngredient(const nlohmann::json &json, const std::string &slot,
                                                              const ProjectFileIssueCallback &issues) {
        const auto ingredient = readRecipeIngredient(json, slot);
        if (!ingredient) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_INGREDIENT, json.dump());
            return std::nullopt;
        }
        return ingredient;
    }

    std::vector<StubRecipeIngredient> parseArrayIngredient(const nlohmann::json &json, const std::string &slot,
                                                           const ProjectFileIssueCallback &issues) {
        std::vector<StubRecipeIngredient> result;

        for (const auto items = json.is_array() ? json : nlohmann::json{json}; const auto &item: items) {
            const auto parsed = parseRecipeIngredient(item, slot, issues);
            if (!parsed) {
                return {};
            }
            result.push_back(*parsed);
        }

        return result;
    }

    StubRecipeIngredient parseResult(nlohmann::json json, const std::string &slot) {
        const auto resultId = json["id"].get<std::string>();
        const auto count = json["count"].get<int>();
        return {resultId, slot, count, false, false};
    }

    std::optional<StubRecipe> readShapedCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                       const ProjectFileIssueCallback &issues) {
        const auto keys = json.at("key");
        const auto pattern = json.at("pattern");
        std::string concat = "";
        for (auto &row: pattern) {
            concat += row;
        }
        StubRecipe recipe{.id = id, .type = type};

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

        recipe.ingredients.push_back(parseResult(json["result"], "1"));

        return recipe;
    }

    std::optional<StubRecipe> readShapelessCraftingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                          const ProjectFileIssueCallback &issues) {
        StubRecipe recipe{.id = id, .type = type};

        const auto ingredients = json["ingredients"];
        for (int i = 0; i < ingredients.size(); ++i) {
            const auto ingredient = parseRecipeIngredient(ingredients[i], std::to_string(i + 1), issues);
            if (!ingredient) {
                return std::nullopt;
            }
            recipe.ingredients.push_back(*ingredient);
        }

        recipe.ingredients.push_back(parseResult(json["result"], "1"));

        return recipe;
    }

    std::optional<StubRecipe> readSmeltingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                 const ProjectFileIssueCallback &issues) {
        StubRecipe recipe{.id = id, .type = type};

        const auto ingredientsJson = json["ingredient"];
        const auto ingredients = parseArrayIngredient(ingredientsJson, "1", issues);
        if (ingredients.empty()) {
            return std::nullopt;
        }
        recipe.ingredients = ingredients;

        recipe.ingredients.push_back(parseResult(json["result"], "1"));

        return recipe;
    }

    std::optional<StubRecipe> readStonecuttingRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                     const ProjectFileIssueCallback &issues) {
        StubRecipe recipe{.id = id, .type = type};

        const auto ingredientsJson = json["ingredient"];
        const auto ingredients = parseArrayIngredient(ingredientsJson, "1", issues);
        if (ingredients.empty()) {
            return std::nullopt;
        }
        recipe.ingredients = ingredients;

        recipe.ingredients.push_back(parseResult(json["result"], "1"));

        return recipe;
    }

    std::optional<StubRecipe> readSmithingTransformRecipe(const std::string &id, const std::string &type, const nlohmann::json &json,
                                                          const ProjectFileIssueCallback &issues) {
        StubRecipe recipe{.id = id, .type = type};
        static const std::vector<std::string> ingredientNames{"template", "base", "addition"};

        for (int i = 0; i < ingredientNames.size(); ++i) {
            const auto key = ingredientNames[i];
            const auto ingredients = parseArrayIngredient(json[key], std::to_string(i), issues);
            if (ingredients.empty()) {
                return std::nullopt;
            }
            recipe.ingredients.insert(recipe.ingredients.end(), ingredients.begin(), ingredients.end());
        }

        recipe.ingredients.push_back(parseResult(json["result"], "1"));

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
        loadRecipeType("minecraft:crafting_shaped", recipe_type::builtin::crafting, readShapedCraftingRecipe);
        loadRecipeType("minecraft:crafting_shapeless", recipe_type::builtin::crafting, readShapelessCraftingRecipe);
        loadRecipeType("minecraft:smelting", recipe_type::builtin::smelting, readSmeltingRecipe);
        loadRecipeType("minecraft:blasting", recipe_type::builtin::blasting, readSmeltingRecipe);
        loadRecipeType("minecraft:campfire_cooking", recipe_type::builtin::campfireCooking, readSmeltingRecipe);
        loadRecipeType("minecraft:smoking", recipe_type::builtin::smoking, readSmeltingRecipe);
        loadRecipeType("minecraft:stonecutting", recipe_type::builtin::stonecutting, readStonecuttingRecipe);
        loadRecipeType("minecraft:smithing_transform", recipe_type::builtin::smithingTransform, readSmithingTransformRecipe);
    }

    bool VanillaRecipeParser::handlesType(const ResourceLocation type) { return type.namespace_ == ResourceLocation::DEFAULT_NAMESPACE; }

    Task<std::optional<GameRecipeType>> VanillaRecipeParser::getType(std::shared_ptr<ProjectBase> project,
                                                                     const ResourceLocation type) const {
        const auto it = builtinRecipeTypes.find(type);
        co_return it == builtinRecipeTypes.end() ? std::nullopt : std::make_optional(it->second.displaySchema);
    }

    std::optional<StubRecipe> VanillaRecipeParser::parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
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
