#pragma once

#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

namespace schemas {
    extern nlohmann::json systemConfig;
    extern nlohmann::json projectRegister;
    extern nlohmann::json projectUpdate;
    extern nlohmann::json projectMetadata;
}
