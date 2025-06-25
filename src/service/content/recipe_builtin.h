#pragma once

#include "ingestor_recipes.h"
#include "recipe_parser.h"

namespace content {
    void loadBuiltinRecipeTypes();

    class VanillaRecipeParser final : public RecipeParser {
        bool handlesType(ResourceLocation type) override;

        std::optional<GameRecipeType> getType(const service::ResolvedProject &project, ResourceLocation type) override;

        std::optional<StubRecipe> parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                             const service::ProjectFileIssueCallback &issues) override;
    };
}
