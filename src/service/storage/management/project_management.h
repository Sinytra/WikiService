#pragma once

#include <drogon/utils/coroutine.h>
#include <models/Project.h>
#include <nlohmann/json.hpp>
#include <service/platforms.h>

using namespace drogon_model::postgres;

namespace service {
    struct ValidatedProjectData {
        Project project;
        nlohmann::json platforms;
    };

    nlohmann::json processPlatforms(const nlohmann::json &metadata);

    drogon::Task<PlatformProject> validatePlatform(std::string id, std::string repo, std::string platform, std::string slug,
                                                   bool checkExisting, User user, bool localEnv);

    drogon::Task<ValidatedProjectData> validateProjectData(nlohmann::json json, User user, bool checkExisting, bool localEnv);

    void enqueueDeploy(const Project &project, const std::string &userId);
}
