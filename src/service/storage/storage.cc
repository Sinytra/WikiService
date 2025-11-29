#include "storage.h"

#include <fstream>
#include <service/storage/deployment.h>
#include <service/util.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "project/virtual/virtual.h"

#define LATEST_VERSION "latest"
#define LOG_FILE "project.log"

using namespace logging;
using namespace drogon;
using namespace service;
namespace fs = std::filesystem;

inline std::string createProjectIssueKey(const std::string &project, const std::string &level, const std::string &type,
                                         const std::string &subject, const std::string &path) {
    return std::format("project_issue:{}:{}:{}:{}:[{}]", project, level, type, subject, path);
}

namespace service {
    inline std::string createProjectSetupKey(const Project &project) { return "setup:" + project.getValueOfId(); }

    // clang-format off
    ENUM_TO_STR(ProjectStatus,
        {ProjectStatus::LOADING, "loading"},
        {ProjectStatus::HEALTHY, "healthy"},
        {ProjectStatus::AT_RISK, "at_risk"},
        {ProjectStatus::ERROR, "error"}
    )
    // clang-format on

    Storage::Storage(const std::string &basePath) : basePath_(basePath) {
        if (!fs::exists(basePath_)) {
            fs::create_directories(basePath_);
        }
        // Verify base path
        getBaseDir();
    }

    fs::directory_entry Storage::getBaseDir() const {
        fs::directory_entry baseDir{basePath_};
        if (!baseDir.exists()) {
            throw std::runtime_error(std::format("Base project storage directory does not exist at {}", basePath_));
        }
        if (!baseDir.is_directory()) {
            throw std::runtime_error(std::format("Base project storage path does not point to a directory: {}", basePath_));
        }
        return baseDir;
    }

    fs::path Storage::getDeploymentRootDir(const Deployment &deployment) const {
        const auto baseDir = getBaseDir();
        return baseDir.path() / deployment.getValueOfProjectId() / deployment.getValueOfId();
    }

    fs::path Storage::getDeploymentVersionedDir(const Deployment &deployment, const std::string &version) const {
        const auto rootDir = getDeploymentRootDir(deployment);
        return rootDir / (version.empty() ? LATEST_VERSION : version);
    }

    std::shared_ptr<spdlog::logger> Storage::getProjectLoggerImpl(const std::string &id, const std::optional<fs::path> &file) const {
        static std::shared_mutex loggersMapLock;

        {
            std::shared_lock<std::shared_mutex> readLock;
            // Get existing logger
            if (const auto existing = spdlog::get(id)) {
                return existing;
            }
        }

        std::unique_lock<std::shared_mutex> writeLock;
        // Re-check
        if (const auto existing = spdlog::get(id)) {
            return existing;
        }

        std::vector<spdlog::sink_ptr> sinks;

        const auto callbackSink = global::connections->createBroadcastSink(id);
        sinks.push_back(callbackSink);

        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%^%L%$] [%T %z] [thread %t] [%n] %v");
        consoleSink->set_level(spdlog::level::trace);
        sinks.push_back(consoleSink);

        if (file) {
            const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(absolute(*file).string());
            fileSink->set_pattern("[%^%L%$] [%Y-%m-%d %T] [%n] %v");
            sinks.push_back(fileSink);
        }

        const auto projectLog = std::make_shared<spdlog::logger>(id, begin(sinks), end(sinks));
        projectLog->set_level(spdlog::level::trace);

        return projectLog;
    }

    std::shared_ptr<spdlog::logger> Storage::getProjectLogger(const Project &project, const bool file) const {
        const auto id = project.getValueOfId();
        const auto path = file ? std::make_optional(getBaseDir().path() / project.getValueOfId() / LOG_FILE) : std::nullopt;
        return getProjectLoggerImpl(id, path);
    }

    std::shared_ptr<spdlog::logger> Storage::getDeploymentLogger(const Deployment &deployment) const {
        const auto id = deployment.getValueOfProjectId() + "-" + deployment.getValueOfId().substr(0, 9);
        const auto path = getBaseDir().path() / deployment.getValueOfProjectId() / deployment.getValueOfId() / LOG_FILE;
        return getProjectLoggerImpl(id, path);
    }

    Task<TaskResult<ResolvedProject>> Storage::findProject(const Project &project, const std::optional<std::string> &version,
                                                           const std::optional<std::string> &locale) const {
        const auto branch = project.getValueOfSourceBranch();

        const auto activeDeployment(co_await global::database->getActiveDeployment(project.getValueOfId()));
        if (!activeDeployment) {
            co_return Error::ErrNotFound;
        }

        const auto rootDir = getDeploymentVersionedDir(*activeDeployment, version.value_or(""));
        if (!exists(rootDir)) {
            co_return Error::ErrNotFound;
        }

        if (version) {
            const auto resolvedVersion = co_await global::database->getProjectVersion(project.getValueOfId(), *version);
            if (!resolvedVersion) {
                co_return Error::ErrNotFound;
            }

            const auto logger = getProjectLogger(project, false);
            const auto issues =
                std::make_shared<ProjectIssueCallback>(activeDeployment->getValueOfId(), getDeploymentLogger(*activeDeployment));
            ResolvedProject resolved{project, rootDir, *resolvedVersion, issues, logger};
            resolved.setLocale(locale);
            co_return resolved;
        }

        const auto defaultVersion = co_await getDefaultVersion(project);
        if (!defaultVersion) {
            co_return Error::ErrNotFound;
        }

        const auto issues =
            std::make_shared<ProjectIssueCallback>(activeDeployment->getValueOfId(), getDeploymentLogger(*activeDeployment));
        const auto logger = getProjectLogger(project, false);
        ResolvedProject resolved{project, rootDir, *defaultVersion, issues, logger};
        resolved.setLocale(locale);
        co_return resolved;
    }

    Task<TaskResult<ProjectBasePtr>> Storage::getProject(const Project &project, const std::optional<std::string> &version,
                                                         const std::optional<std::string> &locale) const {
        if (project.getValueOfIsVirtual()) {
            if (project.getValueOfId() == VIRTUAL_PROJECT_ID) {
                co_return global::virtualProject;
            }
        }

        const auto defaultProject = co_await findProject(project, std::nullopt, locale);

        if (defaultProject && version) {
            if (auto vProject = co_await findProject(project, version, locale); vProject) {
                vProject->setDefaultVersion(*defaultProject);
                co_return std::make_shared<ResolvedProject>(*vProject);
            }
            if (!co_await defaultProject->hasVersion(*version)) {
                logger.error("Failed to find existing version '{}' for '{}'", *version, project.getValueOfId());
            }
        }

        co_return defaultProject ? std::make_shared<ResolvedProject>(*defaultProject) : nullptr;
    }

    // TODO Cache
    Task<TaskResult<ProjectBasePtr>> Storage::getProject(const std::string projectId, const std::optional<std::string> &version,
                                                         const std::optional<std::string> &locale) const {
        const auto proj = co_await global::database->getProjectSource(projectId);
        if (!proj) {
            co_return Error::ErrNotFound;
        }
        co_return co_await getProject(*proj, version, locale);
    }

    Task<TaskResult<ResolvedProject>> Storage::maybeGetProject(const Project &project) const {
        return findProject(project, std::nullopt, std::nullopt);
    }

    Error Storage::removeDeployment(const Deployment &deployment) const {
        logger.debug("Deleting deployment dir '{}'", deployment.getValueOfId());

        const auto path = getDeploymentRootDir(deployment);
        remove_all(path);

        return Error::Ok;
    }

    Error Storage::invalidateProject(const Project &project) const {
        logger.debug("Invalidating project '{}'", project.getValueOfId());

        const auto basePath = getBaseDir().path() / project.getValueOfId();
        remove_all(basePath);

        return Error::Ok;
    }

    Task<ProjectStatus> Storage::getProjectStatus(const Project &project) const {
        if (hasPendingTask(createProjectSetupKey(project))) {
            co_return ProjectStatus::LOADING;
        }

        if (const auto resolved = co_await maybeGetProject(project); !resolved) {
            co_return ProjectStatus::UNKNOWN;
        }

        if (co_await global::database->hasFailingDeployment(project.getValueOfId())) {
            co_return ProjectStatus::AT_RISK;
        }

        if (const auto issues = co_await global::database->getActiveProjectIssueStats(project.getValueOfId());
            issues.contains(enumToStr(ProjectIssueLevel::ERROR)))
        {
            co_return ProjectStatus::AT_RISK;
        }

        co_return ProjectStatus::HEALTHY;
    }

    Task<TaskResult<Deployment>> Storage::deployProject(const Project &project, const std::string userId) {
        if (hasPendingTask(createProjectSetupKey(project))) {
            co_return Error::ErrInternal;
        }

        const auto [deployment, error] = co_await deployProjectCached(project, userId);
        if (error != ProjectError::OK) {
            co_return Error::ErrInternal;
        }

        co_return deployment;
    }

    Task<Error> Storage::addProjectIssue(const ProjectBasePtr project, const ProjectIssueLevel level, const ProjectIssueType type,
                                         const ProjectError subject, const std::string details, const std::string path) {
        const auto projectId = project->getId();

        const auto deployment(co_await global::database->getActiveDeployment(projectId));
        if (!deployment) {
            co_return Error::ErrNotFound;
        }

        const auto taskKey = createProjectIssueKey(projectId, enumToStr(level), enumToStr(type), enumToStr(subject), path);

        if (const auto pending = co_await getOrStartTask<Error>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        if (const auto existing = co_await global::database->getProjectIssue(deployment->getValueOfId(), level, type, path)) {
            co_return co_await completeTask<Error>(taskKey, Error::ErrBadRequest);
        }

        ProjectIssue issue;
        issue.setDeploymentId(deployment->getValueOfId());
        issue.setLevel(enumToStr(level));
        issue.setType(enumToStr(type));
        issue.setSubject(enumToStr(subject));
        issue.setDetails(details);
        if (!path.empty()) {
            issue.setFile(path);
        }
        if (const auto versionName = project->getProjectVersion().getName()) {
            issue.setVersionName(*versionName);
        }

        co_await global::database->addModel(issue);

        co_return co_await completeTask<Error>(taskKey, Error::Ok);
    }
}
