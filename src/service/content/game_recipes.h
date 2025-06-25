#pragma once

#include <nlohmann/json.hpp>

namespace content {
    struct ItemSlot {
        int32_t x;
        int32_t y;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ItemSlot, x, y)
    };

    struct GameRecipeType {
        std::string id;
        std::string localizedName;
        std::string background;
        std::unordered_map<std::string, ItemSlot> inputSlots;
        std::unordered_map<std::string, ItemSlot> outputSlots;

        friend void to_json(nlohmann::json &j, const GameRecipeType &obj) {
            j = nlohmann::json{{"id", obj.id.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.id)},
                               {"localizedName", obj.localizedName.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.localizedName)},
                               {"background", obj.background},
                               {"inputSlots", obj.inputSlots},
                               {"outputSlots", obj.outputSlots}};
        }

        friend void from_json(const nlohmann::json &j, GameRecipeType &obj) {
            j.at("background").get_to(obj.background);
            j.at("inputSlots").get_to(obj.inputSlots);
            j.at("outputSlots").get_to(obj.outputSlots);
        }
    };

    struct ResolvedItem {
        std::string id;
        std::string name;
        std::string project;
        bool has_page;

        friend void to_json(nlohmann::json &j, const ResolvedItem &obj) {
            j = nlohmann::json{{"id", obj.id},
                               {"name", obj.name.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.name)},
                               {"project", obj.project.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.project)},
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
            j = nlohmann::json{
                    {"input", obj.input},
                    {"slot", obj.slot},
                    {"count", obj.count},
                    {"items", obj.items},
                    {"tag", obj.tag.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.tag)}
            };
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
                {"count", obj.count},
                {"item", obj.item},
                {"tag", obj.tag.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.tag)}
            };
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
        GameRecipeType type;
        std::vector<ResolvedSlot> inputs;
        std::vector<ResolvedSlot> outputs;
        RecipeSummary summary;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ResolvedGameRecipe, id, type, inputs, outputs, summary)
    };
}
