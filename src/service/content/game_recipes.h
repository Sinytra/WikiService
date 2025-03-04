#pragma once

#include <json/json.h>
#include <optional>

namespace content {
    std::optional<Json::Value> getRecipeType(const std::string &type);
}
