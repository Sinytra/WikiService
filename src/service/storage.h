#pragma once

#include "cache.h"
#include "error.h"
#include "models/Project.h"
#include "resolved.h"

#include <nlohmann/json.hpp>

using namespace drogon_model::postgres;

namespace service {
    class Storage : public CacheableServiceBase {
    public:
        explicit Storage(const std::string &, MemoryCache &);

        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> getProject(const Project &project, std::string installationToken, const std::optional<std::string> &version, const std::optional<std::string> &locale);

        drogon::Task<std::tuple<std::vector<std::string>, Error>> getRepositoryBranches(const Project &project) const;

        drogon::Task<Error> invalidateProject(const Project &project);
    private:
        drogon::Task<Error> setupProject(const Project &project, std::string installationToken) const;
        drogon::Task<Error> setupProjectCached(const Project &project, std::string installationToken);
        std::filesystem::directory_entry getBaseDir() const;
        std::filesystem::path getProjectDirPath(const Project &project, const std::string &version) const;
        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> findProject(const Project &project, std::string installationToken, const std::optional<std::string> &version, const std::optional<std::string> &locale, bool setup);

        const std::string &basePath_;
        MemoryCache &cache_;
    };
}