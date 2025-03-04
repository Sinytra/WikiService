#pragma once

#include <string>
#include <vector>

struct RecipeIngredient {
    std::string itemId;
    int slot;
    int count;
    bool input;
    bool isTag;
};

struct ModRecipe {
    std::string id;
    std::string type;
    std::vector<RecipeIngredient> ingredients;
};
