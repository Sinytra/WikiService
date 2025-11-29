#pragma once

#include <service/project/resolved.h>
#include <models/Project.h>
#include <nlohmann/json_fwd.hpp>
#include <service/cache.h>
#include <service/error.h>
#include <service/storage/realtime.h>

using namespace drogon_model::postgres;

namespace service {
    enum class ProjectStatus { UNKNOWN, LOADING, HEALTHY, AT_RISK, ERROR };
    std::string enumToStr(ProjectStatus status);

    std::string createProjectSetupKey(const Project &project);

    class Storage : public CacheableServiceBase {
    public:
        explicit Storage(const std::string &);

        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>>
        getProject(std::string projectId, const std::optional<std::string> &version, const std::optional<std::string> &locale) const;
        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>>
        getProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) const;
        drogon::Task<std::optional<ResolvedProject>> maybeGetProject(const Project &project) const;

        [[maybe_unused]] Error invalidateProject(const Project &project) const;
        [[maybe_unused]] Error removeDeployment(const Deployment &deployment) const;

        drogon::Task<ProjectStatus> getProjectStatus(const Project &project) const;

        drogon::Task<std::tuple<std::optional<nlohmann::json>, ProjectError, std::string>>
        setupValidateTempProject(const Project &project) const;

        drogon::Task<std::tuple<std::optional<Deployment>, Error>> deployProject(const Project &project, std::string userId);

        drogon::Task<Error> addProjectIssue(const ResolvedProject &resolved, ProjectIssueLevel level, ProjectIssueType type,
                                            ProjectError subject, std::string details, std::string path);

        void addProjectIssueAsync(const ResolvedProject &resolved, ProjectIssueLevel level, ProjectIssueType type, ProjectError subject,
                                  const std::string &details, const std::string &path) const;

    private:
        drogon::Task<std::optional<ProjectVersion>> getDefaultVersion(const Project &project) const;

        drogon::Task<ProjectError> deployProject(const Project &project, Deployment &deployment, std::filesystem::path clonePath) const;
        drogon::Task<std::tuple<std::optional<Deployment>, ProjectError>> deployProjectCached(const Project &project, std::string userId);

        drogon::Task<std::tuple<std::optional<ResolvedProject>, Error>>
        findProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) const;

        std::filesystem::directory_entry getBaseDir() const;
        std::filesystem::path getDeploymentRootDir(const Deployment &deployment) const;
        std::filesystem::path getDeploymentVersionedDir(const Deployment &deployment, const std::string &version = "") const;
        std::shared_ptr<spdlog::logger> getProjectLogger(const Project &project, bool file = true) const;
        std::shared_ptr<spdlog::logger> getDeploymentLogger(const Deployment &deployment) const;
        std::shared_ptr<spdlog::logger> getProjectLoggerImpl(const std::string &id, const std::optional<std::filesystem::path> &file) const;

        const std::string &basePath_;
    };
}

namespace global {
    extern std::shared_ptr<service::Storage> storage;
    extern std::shared_ptr<realtime::ConnectionManager> connections;
}
