#pragma once

#include <models/ProjectVersion.h>
#include <models/DataImport.h>
#include <nlohmann/json.hpp>

namespace drogon_model::postgres {
    inline void to_json(nlohmann::json &j, const ProjectVersion &version) {
        j = nlohmann::json{{"name", version.getValueOfName()}, {"branch", version.getValueOfBranch()}};
    }

    inline void to_json(nlohmann::json &j, const DataImport &import) {
        j = nlohmann::json{
            {"id", import.getValueOfId()},
            {"game_version", import.getValueOfGameVersion()},
            {"loader", import.getValueOfLoader()},
            {"loader_version", import.getValueOfLoaderVersion()},
            {"user_id", import.getUserId() ? nlohmann::json{import.getValueOfUserId()} : nlohmann::json(nullptr)},
            {"created_at", import.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d")}
        };
    }

    inline void to_json(nlohmann::json &j, const Report &report) {
        j = nlohmann::json{
            {"id", report.getValueOfId()},
            {"type", report.getValueOfType()},
            {"reason", report.getValueOfReason()},
            {"body", report.getValueOfBody()},
            {"status", report.getValueOfStatus()},
            {"project_id", report.getValueOfProjectId()},
            {"path", report.getValueOfPath()},
            {"submitter_id", report.getValueOfSubmitterId()},
            {"created_at", report.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d %H:%M:%S")}
        };
    }
}
