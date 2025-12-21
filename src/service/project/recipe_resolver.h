#pragma once

#include <nlohmann/json.hpp>
#include <drogon/utils/coroutine.h>
#include <models/Recipe.h>
#include <optional>
#include <util.h>

using namespace drogon_model::postgres;

namespace content {
    struct ResolvedItem {
        std::string id;
        std::string name;
        std::string project;
        bool has_page;

        friend void to_json(nlohmann::json &j, const ResolvedItem &obj) {
            j = nlohmann::json{{"id", obj.id},
                               {"name", emptyStrNullable(obj.name)},
                               {"project", emptyStrNullable(obj.project)},
                               {"has_page", obj.has_page}};
        }

        friend void from_json(const nlohmann::json &j, ResolvedItem &obj) {
            j.at("id").get_to(obj.id);
            if (j.contains("name") && j["name"].is_string()) {
                j.at("name").get_to(obj.name);
            }
            if (j.contains("project") && j["project"].is_string()) {
                j.at("project").get_to(obj.project);
            }
            j.at("has_page").get_to(obj.has_page);
        }
    };

    struct ResolvedSlot {
        bool input;
        std::string slot;
        int32_t count;
        std::vector<ResolvedItem> items;
        std::string tag;

        friend void to_json(nlohmann::json &j, const ResolvedSlot &obj) {
            j = nlohmann::json{{"input", obj.input},
                               {"slot", obj.slot},
                               {"count", obj.count},
                               {"items", obj.items},
                               {"tag", emptyStrNullable(obj.tag)}};
        }

        friend void from_json(const nlohmann::json &j, ResolvedSlot &obj) {
            j.at("input").get_to(obj.input);
            j.at("slot").get_to(obj.slot);
            j.at("count").get_to(obj.count);
            j.at("items").get_to(obj.items);
            if (j.contains("tag") && j["tag"].is_string()) {
                j.at("tag").get_to(obj.tag);
            }
        }
    };

    struct RecipeIngredientSummary {
        int32_t count;
        ResolvedItem item;
        std::string tag;

        friend void to_json(nlohmann::json &j, const RecipeIngredientSummary &obj) {
            j = nlohmann::json{
                {"count", obj.count}, {"item", obj.item}, {"tag", emptyStrNullable(obj.tag)}};
        }

        friend void from_json(const nlohmann::json &j, RecipeIngredientSummary &obj) {
            j.at("count").get_to(obj.count);
            if (j.contains("tag") && j["tag"].is_string()) {
                j.at("tag").get_to(obj.tag);
            }
            j.at("item").get_to(obj.item);
        }
    };

    struct RecipeSummary {
        std::vector<RecipeIngredientSummary> inputs;
        std::vector<RecipeIngredientSummary> outputs;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(RecipeSummary, inputs, outputs)
    };

    struct ResolvedGameRecipe {
        std::string id;
        std::string type;
        std::vector<ResolvedSlot> inputs;
        std::vector<ResolvedSlot> outputs;
        RecipeSummary summary;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ResolvedGameRecipe, id, type, inputs, outputs, summary)
    };

    drogon::Task<std::optional<ResolvedGameRecipe>> resolveRecipe(Recipe recipe, const std::optional<std::string> &locale);
}
