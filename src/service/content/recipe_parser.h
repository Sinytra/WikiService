#pragma once

#include <service/content/game_recipes.h>
#include <service/content/ingestor_recipes.h>
#include <service/project_issue.h>
#include <service/resolved.h>
#include <service/util.h>

namespace content {
    std::optional<GameRecipeType> getRecipeType(const service::ResolvedProject &project, const ResourceLocation &type);

    std::optional<StubRecipe> parseRecipe(const std::string &type, const nlohmann::json &data,
                                         const service::ProjectFileIssueCallback &issues);

    class RecipeParser {
    public:
        virtual ~RecipeParser() = default;

        virtual bool handlesType(ResourceLocation type) = 0;

        virtual std::optional<GameRecipeType> getType(const service::ResolvedProject &project, ResourceLocation type) = 0;

        virtual std::optional<StubRecipe> parseRecipe(const std::string &id, const std::string &type, const nlohmann::json &data,
                                                     const service::ProjectFileIssueCallback &issues) = 0;
    };
}
