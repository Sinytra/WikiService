#pragma once

#include <optional>
#include <string>
#include <json/json.h>

namespace content {
    std::optional<Json::Value> getRecipeType(const std::string &type);
}