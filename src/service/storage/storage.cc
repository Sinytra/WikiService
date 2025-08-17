#include "storage.h"

#include <fstream>
#include <service/deployment.h>
#include <service/util.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define LATEST_VERSION "latest"

using namespace logging;
using namespace drogon;
using namespace service;
namespace fs = std::filesystem;

inline std::string createProjectIssueKey(const std::string &project, const std::string &level, const std::string &type,
                                         const std::string &subject, const std::string &path) {
    return std::format("page_error:{}:{}:{}:{}:[{}]", project, level, type, subject, path);
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

    std::filesystem::path Storage::getProjectLogPath(const Project &project) const {
        return getBaseDir().path() / project.getValueOfId() / "project.log";
    }

    fs::path Storage::getProjectDirPath(const Project &project, const std::string &version) const {
        const fs::directory_entry baseDir = getBaseDir();
        const auto targetPath = baseDir.path() / project.getValueOfId() / (version.empty() ? LATEST_VERSION : version);
        return targetPath;
    }

    std::shared_ptr<spdlog::logger> Storage::getProjectLogger(const Project &project, const bool file) const {
        static std::shared_mutex loggersMapLock;

        const auto id = project.getValueOfId();
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
            const auto filePath = getProjectLogPath(project);

            const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(absolute(filePath).string());
            fileSink->set_pattern("[%^%L%$] [%Y-%m-%d %T] [%n] %v");
            sinks.push_back(fileSink);
        }

        const auto projectLog = std::make_shared<spdlog::logger>(id, begin(sinks), end(sinks));
        projectLog->set_level(spdlog::level::trace);

        return projectLog;
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>> Storage::findProject(const Project &project,
                                                                                 const std::optional<std::string> &version,
                                                                                 const std::optional<std::string> &locale) const {
        const auto branch = project.getValueOfSourceBranch();
        const auto rootDir = getProjectDirPath(project, version.value_or(""));

        if (!exists(rootDir)) {
            co_return {std::nullopt, Error::ErrNotFound};
        }

        if (version) {
            const auto resolvedVersion = co_await global::database->getProjectVersion(project.getValueOfId(), *version);
            if (!resolvedVersion) {
                co_return {std::nullopt, Error::ErrNotFound};
            }

            ResolvedProject resolved{project, rootDir, *resolvedVersion};
            resolved.setLocale(locale);
            co_return {resolved, Error::Ok};
        }

        // TODO Check for deployment instead
        const auto defaultVersion = co_await getDefaultVersion(project);
        if (!defaultVersion) {
            co_return {std::nullopt, Error::ErrNotFound};
        }

        ResolvedProject resolved{project, rootDir, *defaultVersion};
        resolved.setLocale(locale);
        co_return {resolved, Error::Ok};
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>>
    Storage::getProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) const {
        const auto [defaultProject, error] = co_await findProject(project, std::nullopt, locale);

        if (defaultProject && version) {
            if (auto [vProject, vError] = co_await findProject(project, version, locale); vProject) {
                vProject->setDefaultVersion(*defaultProject);
                co_return {vProject, vError};
            }
            if (!co_await defaultProject->hasVersion(*version)) {
                logger.error("Failed to find existing version '{}' for '{}'", *version, project.getValueOfId());
            }
        }

        co_return {defaultProject, error};
    }

    // TODO Cache
    Task<std::tuple<std::optional<ResolvedProject>, Error>> Storage::getProject(const std::string projectId,
                                                                                const std::optional<std::string> &version,
                                                                                const std::optional<std::string> &locale) const {
        const auto [proj, projErr] = co_await global::database->getProjectSource(projectId);
        if (!proj) {
            co_return {std::nullopt, Error::ErrNotFound};
        }
        co_return co_await getProject(*proj, version, locale);
    }

    Task<std::optional<ResolvedProject>> Storage::maybeGetProject(const Project &project) const {
        const auto [resolved, error] = co_await findProject(project, std::nullopt, std::nullopt);
        co_return resolved;
    }

    Task<Error> Storage::invalidateProject(const Project &project) const {
        logger.debug("Invalidating project '{}'", project.getValueOfId());

        const auto basePath = getBaseDir().path() / project.getValueOfId();
        remove_all(basePath);

        co_return Error::Ok;
    }

    Task<ProjectStatus> Storage::getProjectStatus(const Project &project) const {
        if (hasPendingTask(createProjectSetupKey(project))) {
            co_return ProjectStatus::LOADING;
        }

        if (const auto resolved = co_await maybeGetProject(project); !resolved) {
            if (const auto filePath = getProjectLogPath(project); exists(filePath)) {
                co_return ProjectStatus::ERROR;
            }
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

    std::optional<std::string> Storage::getProjectLog(const Project &project) const {
        const auto filePath = getProjectLogPath(project);

        std::ifstream file(filePath);

        if (!file) {
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        file.close();

        return buffer.str();
    }

    Task<std::tuple<std::optional<Deployment>, Error>> Storage::deployProject(const Project &project, const std::string userId) {
        if (hasPendingTask(createProjectSetupKey(project))) {
            co_return {std::nullopt, Error::ErrInternal};
        }

        const auto [deployment, error] = co_await deployProjectCached(project, userId);
        if (error != ProjectError::OK) {
            co_return {std::nullopt, Error::ErrInternal};
        }

        co_return {deployment, Error::Ok};
    }

    Task<Error> Storage::addProjectIssue(const ResolvedProject &resolved, const ProjectIssueLevel level, const ProjectIssueType type,
                                         const ProjectError subject, const std::string details, const std::string path) {
        const auto projectId = resolved.getProject().getValueOfId();

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
        if (const auto versionName = resolved.getProjectVersion().getName()) {
            issue.setVersionName(*versionName);
        }

        co_await global::database->addModel(issue);

        co_return co_await completeTask<Error>(taskKey, Error::Ok);
    }

    void Storage::addProjectIssueAsync(const ResolvedProject &resolved, const ProjectIssueLevel level, const ProjectIssueType type,
                                       const ProjectError subject, const std::string &details, const std::string &path) const {
        trantor::EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            async_func([=]() -> Task<> { co_await global::storage->addProjectIssue(resolved, level, type, subject, details, path); }));
    }
}
