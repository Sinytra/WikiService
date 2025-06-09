#include "deployment.h"

namespace service {
    std::string deploymentStatusToString(const DeploymentStatus status) {
        switch (status) {
            case DeploymentStatus::CREATED:
                return "create";
            case DeploymentStatus::LOADING:
                return "loading";
            case DeploymentStatus::SUCCESS:
                return "success";
            case DeploymentStatus::ERROR:
                return "error";
            default:
                return "unknown";
        }
    }
}