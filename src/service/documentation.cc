#include "documentation.h"
#include "schemas.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;
using namespace std::chrono_literals;

std::string createPublicAccessCacheKey(const std::string &id) { return "project:" + id + ":public"; }

namespace service {
    Documentation::Documentation(GitHub &g, MemoryCache &c) : github_(g), cache_(c) {}

    Task<std::tuple<std::string, Error>> Documentation::getIntallationToken(const Project &project) const {
        const auto repo = project.getValueOfSourceRepo();

        const auto [installationId, idErr](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            co_return {"", Error::ErrNotFound};
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            co_return {"", Error::ErrInternal};
        }

        co_return {*installationToken, Error::Ok};
    }

    Task<bool> Documentation::isPubliclyEditable(const Project &project) {
        const auto cacheKey = createPublicAccessCacheKey(project.getValueOfId());

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            co_return cached == "true";
        }

        if (const auto pending = co_await getOrStartTask<bool>(cacheKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        bool isPublic = false;

        if (const auto [installationToken, tokenErr](co_await getIntallationToken(project)); tokenErr == Error::Ok) {
            auto [isPublicRepo, error](co_await github_.isPublicRepository(project.getValueOfSourceRepo(), installationToken));
            isPublic = isPublicRepo;
        } else {
            logger.error("Failed to get installation token for project '{}'", project.getValueOfId());
        }

        co_await cache_.updateCache(cacheKey, isPublic ? "true" : "false", 30 * 24h);
        co_return co_await completeTask<bool>(cacheKey, std::move(isPublic));
    }

    Task<> Documentation::invalidateCache(const Project &project) const {
        co_await cache_.eraseNamespace("project:" + project.getValueOfId());
    }
}
