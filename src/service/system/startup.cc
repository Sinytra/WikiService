#include "startup.h"

#include <database/database.h>
#include <storage/storage.h>

using namespace drogon;
using namespace logging;

namespace service {
    Task<> cleanupLoadingDeployments() {
        const auto deployments = co_await global::database->getLoadingDeployments();
        if (!deployments.empty()) {
            logger.debug("Cleaning up {} failed deployments", deployments.size());
        }
        for (const auto &dep : deployments) {
            global::storage->removeDeployment(dep);
        }

        co_await global::database->failLoadingDeployments();
    }
}