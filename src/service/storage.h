#pragma once

#include "cache.h"
#include "content/game_content.h"
#include "error.h"
#include "models/Project.h"
#include "resolved.h"

#include <drogon/WebSocketConnection.h>
#include <nlohmann/json_fwd.hpp>

using namespace drogon_model::postgres;

namespace service {
    enum class ProjectStatus {
        UNKNOWN,
        LOADING,
        HEALTHY,
        AT_RISK,
        ERROR
    };
    std::string enumToStr(ProjectStatus status);

    class RealtimeConnectionStorage {
    public:
        void connect(const std::string &project, const drogon::WebSocketConnectionPtr &connection);

        void disconnect(const drogon::WebSocketConnectionPtr &connection);

        void broadcast(const std::string &project, const std::string &message);

        void shutdown(const std::string &project);
    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::vector<std::string>> pending_;
        std::unordered_map<std::string, std::vector<drogon::WebSocketConnectionPtr>> connections_;
    };

    class Storage : public CacheableServiceBase {
    public:
        explicit Storage(const std::string &);

        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> getProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) const;

        drogon::Task<Error> invalidateProject(const Project &project) const;

        drogon::Task<std::optional<ResolvedProject>> maybeGetProject(const Project &project) const;

        drogon::Task<ProjectStatus> getProjectStatus(const Project &project) const;

        std::optional<std::string> getProjectLog(const Project &project) const;

        drogon::Task<std::tuple<std::optional<nlohmann::json>, ProjectError, std::string>> setupValidateTempProject(const Project &project) const;

        drogon::Task<std::tuple<std::optional<Deployment>, Error>> deployProject(const Project &project, std::string userId);
    private:
        drogon::Task<ProjectError> setupProject(const Project &project, Deployment& deployment, std::filesystem::path clonePath) const;
        drogon::Task<std::tuple<std::optional<Deployment>, ProjectError>> setupProjectCached(const Project &project, std::string userId);
        std::filesystem::directory_entry getBaseDir() const;
        std::filesystem::path getProjectLogPath(const Project &project) const;
        std::filesystem::path getProjectDirPath(const Project &project, const std::string &version) const;
        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>> findProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) const;
        std::shared_ptr<spdlog::logger> getProjectLogger(const Project &project, bool file = true) const;

        const std::string &basePath_;
    };
}

namespace global {
    extern std::shared_ptr<service::Storage> storage;
    extern std::shared_ptr<service::RealtimeConnectionStorage> connections;
}