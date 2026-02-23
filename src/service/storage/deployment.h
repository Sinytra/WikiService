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
    DECLARE_ENUM(DeploymentStatus);
}