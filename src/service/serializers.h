#pragma once

#include <models/ProjectVersion.h>
#include <models/DataImport.h>
#include <nlohmann/json.hpp>

namespace drogon_model::postgres {
    inline void to_json(nlohmann::json &j, const ProjectVersion &version) {
        j = nlohmann::json{{"name", version.getValueOfName()}, {"branch", version.getValueOfBranch()}};
    }

    inline void to_json(nlohmann::json &j, const DataImport &version) {
        j = nlohmann::json{
            {"id", version.getValueOfId()},
            {"game_version", version.getValueOfGameVersion()},
            {"loader", version.getValueOfLoader()},
            {"loader_version", version.getValueOfLoaderVersion()},
            {"user_id", version.getUserId() ? nlohmann::json{version.getValueOfUserId()} : nlohmann::json(nullptr)},
            {"created_at", version.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d")}
        };
    }
}
