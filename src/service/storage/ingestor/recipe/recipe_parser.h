#pragma once

#include "game_recipes.h"
#include <service/project/resolved.h>
#include <service/storage/issues.h>
#include <service/util.h>

namespace content {
    struct StubRecipeType {
        std::string id;
    };

    struct StubRecipeIngredient {
        std::string itemId;
        std::string slot;
        int count;
        bool input;
        bool isTag;
    };

    struct StubRecipe {
        std::string id;
        std::string type;
        std::vector<StubRecipeIngredient> ingredients;
    };

    drogon::Task<std::optional<GameRecipeType>> getRecipeType(service::ProjectBasePtr project, const ResourceLocation &type);

    std::optional<StubRecipe> parseRecipe(const std::string &type, const nlohmann::json &data,
                                         const service::ProjectFileIssueCallback &issues);

    class RecipeParser {
    public:
        virtual ~RecipeParser() = default;

        virtual bool handlesType(ResourceLocation type) = 0;

        virtual drogon::Task<std::optional<GameRecipeType>> getType(service::ProjectBasePtr project, ResourceLocation type) const = 0;

        virtual std::optional<StubRecipe> parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                                     const service::ProjectFileIssueCallback &issues) = 0;
    };
}
