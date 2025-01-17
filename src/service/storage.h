#pragma once

#include "cache.h"
#include "documentation.h"
#include "error.h"
#include "models/Project.h"
#include "resolved.h"

#include <drogon/WebSocketConnection.h>
#include <nlohmann/json.hpp>

using namespace drogon_model::postgres;

namespace service {
    enum ProjectStatus {
        UNKNOWN,
        LOADING,
        LOADED,
        ERROR
    };

    std::string projectStatusToString(ProjectStatus status);

    class RealtimeConnectionStorage {
    public:
        void connect(const std::string &project, const drogon::WebSocketConnectionPtr &connection);

        void disconnect(const drogon::WebSocketConnectionPtr &connection);

        void broadcast(const std::string &project, const std::string &message) const;

        void shutdown(const std::string &project);
    private:
        std::unordered_map<std::string, std::vector<drogon::WebSocketConnectionPtr>> connections_;
    };

    class Storage : public CacheableServiceBase {
    public:
        explicit Storage(const std::string &, MemoryCache &, Documentation &, RealtimeConnectionStorage &);

        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> getProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale);

        drogon::Task<std::tuple<std::vector<std::string>, Error>> getRepositoryBranches(const Project &project) const;

        drogon::Task<Error> invalidateProject(const Project &project);

        drogon::Task<std::optional<ResolvedProject>> maybeGetProject(const Project &project);

        drogon::Task<ProjectStatus> getProjectStatus(const Project &project);

        std::optional<std::string> getProjectLog(const Project &project) const;
    private:
        drogon::Task<Error> setupProject(const Project &project, std::string installationToken) const;
        drogon::Task<Error> setupProjectCached(const Project &project, std::string installationToken);
        std::filesystem::directory_entry getBaseDir() const;
        std::filesystem::path getProjectLogPath(const Project &project) const;
        std::filesystem::path getProjectDirPath(const Project &project, const std::string &version) const;
        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> findProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale, bool setup);
        std::shared_ptr<spdlog::logger> getProjectLogger(const Project &project) const;

        const std::string &basePath_;
        MemoryCache &cache_;
        Documentation &documentation_;
        RealtimeConnectionStorage &connections_;
    };
}