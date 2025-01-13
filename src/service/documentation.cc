#include "documentation.h"
#include "schemas.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;
using namespace std::chrono_literals;

std::string createPublicAccessCacheKey(const std::string &id) { return "project:" + id + ":public"; }

namespace service {
    Documentation::Documentation(GitHub &g, MemoryCache &c) : github_(g), cache_(c) {}

    Task<bool> Documentation::isPubliclyEditable(const Project &project, const std::string installationToken) {
        const auto cacheKey = createPublicAccessCacheKey(project.getValueOfId());

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            co_return cached == "true";
        }

        if (const auto pending = co_await getOrStartTask<bool>(cacheKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        auto [isPublic, error](co_await github_.isPublicRepository(project.getValueOfSourceRepo(), installationToken));

        co_await cache_.updateCache(cacheKey, isPublic ? "true" : "false", 14 * 24h);
        co_return co_await completeTask<bool>(cacheKey, std::move(isPublic));
    }

    Task<> Documentation::invalidateCache(const Project &project) const {
        co_await cache_.eraseNamespace("project:" + project.getValueOfId());
    }
}
