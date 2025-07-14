#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>

namespace content {
    nlohmann::json parseItemProperties(const std::filesystem::path &path, const std::string &id);
}