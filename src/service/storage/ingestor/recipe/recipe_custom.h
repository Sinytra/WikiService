#pragma once

#include "recipe_parser.h"
#include "game_recipes.h"

namespace content {
    class CustomRecipeParser final : public RecipeParser {
        bool handlesType(ResourceLocation type) override;

        drogon::Task<std::optional<GameRecipeType>> getType(service::ProjectBasePtr project, ResourceLocation type) const override;

        std::optional<StubRecipe> parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                             const service::ProjectFileIssueCallback &issues) override;
    };
}