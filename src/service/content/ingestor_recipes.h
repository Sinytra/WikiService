#pragma once

#include <string>
#include <vector>

namespace content {
    struct RecipeIngredient {
        std::string itemId;
        std::string slot;
        int count;
        bool input;
        bool isTag;
    };

    struct ModRecipe {
        std::string id;
        std::string type;
        std::vector<RecipeIngredient> ingredients;
    };
}
