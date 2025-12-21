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
            j = nlohmann::json{{"id", emptyStrNullable(obj.id)},
                               {"localizedName", emptyStrNullable(obj.localizedName)},
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
}
