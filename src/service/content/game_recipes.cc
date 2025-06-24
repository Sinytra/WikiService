#include "game_recipes.h"

#include <models/Item.h>
#include <models/RecipeIngredientItem.h>
#include <models/RecipeIngredientTag.h>
#include <models/Tag.h>
#include <service/database/database.h>
#include <service/lang/lang.h>
#include <service/storage/storage.h>

#include "database/resolved_db.h"

using namespace drogon;
using namespace service;

namespace content {
    struct ResolvableItem {
        std::string project_id;
        std::string loc;
    };

    std::vector<ResolvedSlot> mergeSlots(const std::vector<ResolvedSlot> &slots, const bool input) {
        std::unordered_map<std::string, ResolvedSlot> map;
        for (const auto &slot: slots) {
            if (slot.input == input) {
                if (map.contains(slot.slot)) {
                    map[slot.slot].items.insert(map[slot.slot].items.end(), slot.items.begin(), slot.items.end());
                } else {
                    map[slot.slot] = slot;
                }
            }
        }
        const auto values = std::views::values(map);
        return {values.begin(), values.end()};
    }

    std::vector<RecipeIngredientSummary> getIngredientSummary(const std::vector<ResolvedSlot> &slots) {
        std::vector<RecipeIngredientSummary> summary;

        std::unordered_map<std::string, int32_t> count;
        std::unordered_map<std::string, ResolvedItem> items;
        for (const auto &slot: slots) {
            if (!slot.items.empty()) {
                const auto item = slot.items.front();
                if (count[item.id]++ == 0) {
                    items[item.id] = item;
                }
            }
        }

        for (const auto &[key, val]: count) {
            const auto item = items[key];
            summary.emplace_back(val, item);
        }

        return summary;
    }

    class RecipeResolver {
    public:
        RecipeResolver(const std::optional<std::string> &version, const std::optional<std::string> &locale) : version_(version), locale_(locale) {}

        Task<ResolvedItem> resolveItem(const ResolvableItem item) const {
            // Vanilla
            if (item.project_id.empty()) {
                const auto name = co_await global::lang->getItemName(locale_, item.loc);
                co_return ResolvedItem{.id = item.loc, .name = name, .project = "", false};
            }

            // Modded
            if (const auto [resolved, resolveErr] = co_await global::storage->getProject(item.project_id, version_, locale_); resolved)
            {
                const auto [name, path] = co_await resolved->getItemName(item.loc);
                co_return ResolvedItem{.id = item.loc, .name = name, .project = item.project_id, !path.empty()};
            }

            co_return ResolvedItem{.id = item.loc, .name = "", .project = item.project_id, false};
        }

        Task<std::vector<ResolvedItem>> resolveIngredient(const RecipeIngredientItem ingredient) const {
            const auto item = co_await global::database->getModel<Item>(ingredient.getValueOfItemId());
            std::vector<ResolvedItem> result;
            if (item) {
                auto sources = co_await global::database->getItemSourceProjects(item->getValueOfId());
                if (sources.empty()) {
                    // Vanilla
                    sources.emplace_back("");
                }

                const auto loc = item->getValueOfLoc();
                for (const auto &source: sources) {
                    result.emplace_back(co_await resolveItem({.project_id = source, .loc = loc}));
                }
            }
            co_return result;
        }

        // TODO Preserve tag info
        Task<std::vector<ResolvedItem>> resolveIngredient(const RecipeIngredientTag ingredient) const {
            const auto dbTag = co_await global::database->getByPrimaryKey<Tag>(ingredient.getValueOfTagId());

            std::vector<ResolvedItem> result;
            for (const auto tagItems = co_await global::database->getGlobalTagItems(ingredient.getValueOfTagId());
                 const auto &item: tagItems)
            {
                result.emplace_back(co_await resolveItem({.project_id = item.project_id, .loc = item.loc}));
            }
            co_return result;
        }

        template<typename T>
        Task<ResolvedSlot> resolveSlot(const T ingredient) const {
            co_return ResolvedSlot{.input = ingredient.getValueOfInput(),
                                   .slot = ingredient.getValueOfSlot(),
                                   .count = ingredient.getValueOfCount(),
                                   .items = co_await resolveIngredient(ingredient)};
        }

    private:
        const std::optional<std::string> version_;
        const std::optional<std::string> locale_;
    };

    Task<std::optional<ResolvedGameRecipe>> getRecipe(const std::string id, const std::optional<std::string> &version,
                                                      const std::optional<std::string> &locale) {
        const auto [resolved, resErr](co_await global::storage->getProject(id, version, locale));
        if (!resolved) {
            co_return std::nullopt;
        }

        const auto recipe = co_await resolved->getProjectDatabase().getProjectRecipe(id);
        if (!recipe) {
            co_return std::nullopt;
        }

        co_return co_await resolveRecipe(*recipe, version, locale);
    }

    // TODO Cache in redis
    Task<std::optional<ResolvedGameRecipe>> resolveRecipe(const Recipe recipe, const std::optional<std::string> &version,
                                                          const std::optional<std::string> &locale) {
        const auto recipeId = recipe.getValueOfId();
        const auto recipeType = recipe.getValueOfType();
        auto type = getRecipeType(recipeType);
        if (!type) {
            co_return std::nullopt;
        }
        type->id = recipeType;

        const auto itemIngredients =
            co_await global::database->getRelated<RecipeIngredientItem>(RecipeIngredientItem::Cols::_recipe_id, recipeId);
        const auto tagIngredients =
            co_await global::database->getRelated<RecipeIngredientTag>(RecipeIngredientTag::Cols::_recipe_id, recipeId);

        const RecipeResolver resolver{version, locale};

        std::vector<ResolvedSlot> resolvedSlots;
        for (const auto &ingredient: itemIngredients) {
            resolvedSlots.emplace_back(co_await resolver.resolveSlot(ingredient));
        }
        for (const auto &ingredient: tagIngredients) {
            resolvedSlots.emplace_back(co_await resolver.resolveSlot(ingredient));
        }

        const auto mergedInput = mergeSlots(resolvedSlots, true);
        const auto mergedOutput = mergeSlots(resolvedSlots, false);

        const RecipeSummary summary{.inputs = getIngredientSummary(mergedInput), .outputs = getIngredientSummary(mergedOutput)};

        co_return ResolvedGameRecipe{
            .id = recipe.getValueOfLoc(), .type = *type, .inputs = mergedInput, .outputs = mergedOutput, .summary = summary};
    }
}
