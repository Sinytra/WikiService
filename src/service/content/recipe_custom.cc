#include "recipe_custom.h"

#include <drogon/drogon.h>
#include <service/schemas.h>

using namespace drogon;
using namespace drogon::orm;
using namespace service;

namespace content {
    std::optional<StubRecipeIngredient> readCustomIngredient(const std::string &slot, const bool input, const nlohmann::json &data) {
        std::string rawId;
        int count;

        if (data.is_string()) {
            rawId = data;
            count = 1;
        } else if (data.is_object()) {
            rawId = data["id"];
            count = data["count"];
        } else {
            return std::nullopt;
        }

        const auto isTag = rawId.starts_with("#");
        const auto id = isTag ? rawId.substr(1) : rawId;

        return StubRecipeIngredient{id, slot, count, input, isTag};
    }

    std::optional<StubRecipeIngredient> parseCustomIngredient(const std::string &slot, const bool input, const nlohmann::json &data,
                                                          const ProjectFileIssueCallback &issues) {
        const auto ingredient = readCustomIngredient(slot, input, data);
        if (!ingredient) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_INGREDIENT, data.dump());
            return std::nullopt;
        }
        return ingredient;
    }

    bool CustomRecipeParser::handlesType(const ResourceLocation type) { return type.namespace_ != ResourceLocation::DEFAULT_NAMESPACE; }

    std::optional<GameRecipeType> CustomRecipeParser::getType(const ResolvedProject &project, const ResourceLocation type) {
        if (auto result = project.getRecipeType(type)) {
            result->id = type;
            const auto langKey = std::format("recipe_type.{}.{}", type.namespace_, type.path_);
            // TODO Locale
            if (const auto localName = project.readLangKey("en_en", langKey)) {
                result->localizedName = *localName;
            }
            return result;
        }
        return std::nullopt;
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
                const auto ingredient = parseCustomIngredient(key, slot == "input", val, issues);
                if (!ingredient) {
                    return std::nullopt;
                }
                recipe.ingredients.push_back(*ingredient);
            }
        }

        return recipe;
    }
}
