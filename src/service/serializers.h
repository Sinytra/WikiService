#pragma once

#include <nlohmann/json.hpp>
#include <models/ProjectVersion.h>

namespace drogon_model::postgres {
    void to_json(nlohmann::json &j, const ProjectVersion &version) {
        j = nlohmann::json{{"name", version.getValueOfName()}, {"branch", version.getValueOfBranch()}};
    }
}
