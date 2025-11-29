#pragma once

#include "recipe_parser.h"

namespace content {
    void loadBuiltinRecipeTypes();

    class VanillaRecipeParser final : public RecipeParser {
        bool handlesType(ResourceLocation type) override;

        drogon::Task<std::optional<GameRecipeType>> getType(std::shared_ptr<service::ProjectBase> project, ResourceLocation type) const override;

        std::optional<StubRecipe> parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                             const service::ProjectFileIssueCallback &issues) override;
    };
}
