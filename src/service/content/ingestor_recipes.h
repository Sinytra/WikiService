#pragma once

#include <string>
#include <vector>

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
}
