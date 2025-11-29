#include "recipe_custom.h"

#include <schemas/schemas.h>

using namespace drogon;
using namespace drogon::orm;
using namespace service;

namespace content {
    StubRecipeIngredient readSingleIngredient(const std::string &slot, const bool input, const std::string &id, const int count) {
        const auto isTag = id.starts_with("#");
        const auto actualId = isTag ? id.substr(1) : id;

        return StubRecipeIngredient{id, slot, count, input, isTag};
    }

    void readIngredientArray(std::vector<StubRecipeIngredient> &results, const std::string &slot, const bool input,
                             const nlohmann::json &data, const int count) {
        for (auto &item: data) {
            results.push_back(readSingleIngredient(slot, input, item, count));
        }
    }

    std::vector<StubRecipeIngredient> readCustomIngredient(const std::string &slot, const bool input, const nlohmann::json &data) {
        std::vector<StubRecipeIngredient> results;

        if (data.is_string()) {
            results.push_back(readSingleIngredient(slot, input, data, 1));
        } else if (data.is_object()) {
            const int count = data["count"];
            if (data["id"].is_array()) {
                readIngredientArray(results, slot, input, data["id"], count);
            } else {
                results.push_back(readSingleIngredient(slot, input, data["id"], count));
            }
        } else if (data.is_array()) {
            readIngredientArray(results, slot, input, data, 1);
        }

        return results;
    }

    std::vector<StubRecipeIngredient> parseCustomIngredient(const std::string &slot, const bool input, const nlohmann::json &data,
                                                              const ProjectFileIssueCallback &issues) {
        const auto ingredient = readCustomIngredient(slot, input, data);
        if (ingredient.empty()) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_INGREDIENT, data.dump());
        }
        return ingredient;
    }

    bool CustomRecipeParser::handlesType(const ResourceLocation type) { return type.namespace_ != ResourceLocation::DEFAULT_NAMESPACE; }

    Task<std::optional<GameRecipeType>> CustomRecipeParser::getType(const std::shared_ptr<ProjectBase> project, const ResourceLocation type) const {
        if (auto result = co_await project->getRecipeType(type)) {
            result->id = type;
            const auto langKey = std::format("recipe_type.{}.{}", type.namespace_, type.path_);
            if (const auto localName = project->readLangKey(type.namespace_, langKey)) {
                result->localizedName = *localName;
            }
            co_return result;
        }
        co_return std::nullopt;
    }

    std::optional<StubRecipe> CustomRecipeParser::parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                                              const ProjectFileIssueCallback &issues) {
        static const std::vector<std::string> slots{"input", "output"};

        if (const auto error = validateJson(schemas::gameRecipeCustom, data)) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
            return std::nullopt;
        }

        StubRecipe recipe{.id = id, .type = type};

        for (const auto &slot: slots) {
            for (const auto &[key, val]: data[slot].items()) {
                const auto ingredients = parseCustomIngredient(key, slot == "input", val, issues);
                if (ingredients.empty()) {
                    return std::nullopt;
                }
                recipe.ingredients.insert(recipe.ingredients.end(), ingredients.begin(), ingredients.end());
            }
        }

        return recipe;
    }
}
