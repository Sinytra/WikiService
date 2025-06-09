#pragma once

#include <string>

namespace service {
    enum class DeploymentStatus {
        UNKNOWN,
        CREATED,
        LOADING,
        SUCCESS,
        ERROR
    };
    std::string deploymentStatusToString(DeploymentStatus status);
}