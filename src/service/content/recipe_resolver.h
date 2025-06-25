#pragma once

#include <drogon/utils/coroutine.h>
#include <models/Recipe.h>
#include <optional>
#include <service/resolved.h>
#include "game_recipes.h"

using namespace drogon_model::postgres;

namespace content {
    drogon::Task<std::optional<ResolvedGameRecipe>> resolveRecipe(const service::ResolvedProject &project, Recipe recipe,
                                                                  const std::optional<std::string> &locale);
}
