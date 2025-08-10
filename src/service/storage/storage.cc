#include "storage.h"

#include <filesystem>
#include <fstream>
#include <git2/repository.h>
#include <service/content/ingestor.h>
#include <service/database/resolved_db.h>
#include <service/deployment.h>
#include <service/storage/gitops.h>
#include <service/util.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define TEMP_DIR ".temp"
#define LATEST_VERSION "latest"

using namespace logging;
using namespace drogon;
using namespace service;
namespace fs = std::filesystem;

const std::set<std::string> allowedFileExtensions = {".mdx", ".json", ".png", ".jpg", ".jpeg", ".webp", ".gif"};

Error copyProjectFiles(const fs::path &root, const fs::path &docsRoot, const fs::path &dest,
                       const std::shared_ptr<spdlog::logger> &projectLog) {
    const auto gitPath = root / ".git";

    projectLog->info("Copying project files for version '{}'", dest.filename().string());

    try {
        create_directories(dest);

        for (const auto &entry: fs::recursive_directory_iterator(root)) {
            if (!isSubpath(entry.path(), gitPath) && entry.is_regular_file() && isSubpath(entry.path(), docsRoot)) {
                fs::path relative_path = relative(entry.path(), docsRoot);

                if (const auto extension = entry.path().extension().string(); !allowedFileExtensions.contains(extension)) {
                    projectLog->warn("Ignoring file {}", relative_path.string());
                    continue;
                }

                if (fs::path destination_path = dest / relative_path.parent_path(); !exists(destination_path)) {
                    create_directories(destination_path);
                }

                copy(entry, dest / relative_path, fs::copy_options::overwrite_existing);
            }
        }
    } catch (std::exception e) {
        projectLog->error("FS copy error: {}", e.what());
        return Error::ErrInternal;
    }

    projectLog->info("Done copying files");

    return Error::Ok;
}

inline std::string createProjectSetupKey(const Project &project) { return "setup:" + project.getValueOfId(); }
inline std::string createProjectIssueKey(const std::string &project, const std::string &level, const std::string &type,
                                         const std::string &subject, const std::string &path) {
    return std::format("page_error:{}:{}:{}:{}:[{}]", project, level, type, subject, path);
}

namespace service {
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

    fs::path Storage::getProjectDirPath(const Project &project, const std::string &version = "") const {
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

    std::unordered_map<std::string, std::string> readVersionsFromMetadata(const nlohmann::json &metadata,
                                                                          const std::string &defaultBranch) {
        std::unordered_map<std::string, std::string> versions;

        if (metadata.contains("versions") && metadata["versions"].is_object()) {
            for (const auto &[key, val]: metadata["versions"].items()) {
                if (val.is_string() && val.get<std::string>() != defaultBranch) {
                    versions[key] = val;
                }
            }
        }

        return versions;
    }

    Task<std::vector<ProjectVersion>> setupProjectVersions(const ResolvedProject &resolved,
                                                           const std::unordered_map<std::string, std::string> &branches,
                                                           const std::shared_ptr<spdlog::logger> logger) {
        const auto projectId = resolved.getProject().getValueOfId();
        const auto [json, err, det] = resolved.validateProjectMetadata();
        if (!json) {
            co_return std::vector<ProjectVersion>{};
        }
        const auto defaultBranch = resolved.getProject().getValueOfSourceBranch();
        const auto versions(readVersionsFromMetadata(*json, defaultBranch));

        std::vector<ProjectVersion> resolvedVersions;
        for (auto &[key, val]: versions) {
            if (!branches.contains(val)) {
                logger->warn("Ignoring version '{}' with unknown branch '{}'", key, val);
                continue;
            }

            ProjectVersion version;
            version.setProjectId(projectId);
            version.setName(key);
            version.setBranch(val);
            if (const auto [result, err] = co_await global::database->createProjectVersion(version); result) {
                resolvedVersions.push_back(version);
            }
        }
        co_return resolvedVersions;
    }

    Task<std::optional<ProjectVersion>> createDefaultVersion(const Project &project) {
        logger.debug("Adding default version for project {}", project.getValueOfId());

        ProjectVersion defaultVersion;
        defaultVersion.setProjectId(project.getValueOfId());
        defaultVersion.setBranch(project.getValueOfSourceBranch());
        auto [persistedDefaultVersion, dbErr] = co_await global::database->createProjectVersion(defaultVersion);
        co_return persistedDefaultVersion;
    }

    Task<std::optional<ProjectVersion>> getOrCreateDefaultVersion(const Project &project) {
        auto resolvedDefaultVersion = co_await global::database->getDefaultProjectVersion(project.getValueOfId());
        if (!resolvedDefaultVersion) {
            co_return co_await createDefaultVersion(project);
        }
        co_return resolvedDefaultVersion;
    }

    Task<Error> setActiveDeployment(const std::string projectId, Deployment &deployment) {
        const auto [res, err] = co_await executeTransaction<Error>([projectId, &deployment](const Database &client) -> Task<Error> {
            if (const auto dbErr = co_await client.deactivateDeployments(projectId); dbErr != Error::Ok) {
                co_return dbErr;
            }

            deployment.setActive(true);
            co_await client.updateModel(deployment);

            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<ProjectError> Storage::setupProject(const Project &project, Deployment &deployment, const fs::path clonePath) const {
        const auto logger = getProjectLogger(project);
        logger->info("Setting up project");

        deployment.setStatus(enumToStr(DeploymentStatus::LOADING));
        co_await global::database->updateModel(deployment);

        ProjectIssueCallback issues{deployment.getValueOfId(), logger};

        const auto [repo, cloneError] =
            co_await git::cloneRepository(project.getValueOfSourceRepo(), clonePath, project.getValueOfSourceBranch(), logger);
        if (!repo || cloneError.error != ProjectError::OK) {
            co_await issues.addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::GIT_CLONE, cloneError.error, cloneError.message);
            co_return cloneError.error;
        }

        // TODO Validate metadata

        auto defaultVersion = co_await getOrCreateDefaultVersion(project);
        if (!defaultVersion) {
            logger->error("Project version creation database error.");
            co_await issues.addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::INTERNAL, ProjectError::UNKNOWN);
            co_return ProjectError::UNKNOWN;
        }

        const auto revision = git::getLatestRevision(repo);
        if (!revision) {
            logger->error("Error getting commit information");
            co_await issues.addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::GIT_INFO, ProjectError::UNKNOWN);
            co_return ProjectError::UNKNOWN;
        }

        deployment.setRevision(nlohmann::json(*revision).dump());
        co_await global::database->updateModel(deployment);

        const auto cloneDocsRoot = clonePath / removeLeadingSlash(project.getValueOfSourcePath());
        ResolvedProject resolved{project, cloneDocsRoot, *defaultVersion};

        // TODO Ingest from other versions?
        content::Ingestor ingestor{resolved, logger, issues};
        if (const auto result = co_await ingestor.runIngestor(); result != Error::Ok) {
            logger->error("Error ingesting project data");
            co_return ProjectError::UNKNOWN;
        }

        if (issues.hasErrors()) {
            logger->error("Encountered issues during project setup, aborting");
            co_return ProjectError::UNKNOWN;
        }

        const auto branches = git::listBranches(repo);
        auto versions = co_await resolved.getProjectDatabase().getVersions();
        if (versions.empty()) {
            versions = co_await setupProjectVersions(resolved, branches, logger);
        }

        // TODO Remove first

        const auto dest = getProjectDirPath(project);
        copyProjectFiles(clonePath, cloneDocsRoot, dest, logger);

        for (const auto &version: versions) {
            const auto name = version.getValueOfName();
            const auto branch = version.getValueOfBranch();
            logger->info("Setting up version '{}' on branch '{}'", name, branch);

            const auto branchRef = branches.at(branch);
            if (const auto err = git::checkoutBranch(repo, branchRef, logger); err != Error::Ok) {
                continue;
            }

            const auto versionDest = getProjectDirPath(project, name);
            copyProjectFiles(clonePath, cloneDocsRoot, versionDest, logger);
        }

        git_repository_free(repo);

        if (const auto err = co_await setActiveDeployment(project.getValueOfId(), deployment); err != Error::Ok) {
            logger->error("Error setting active deployment");
            co_return ProjectError::UNKNOWN;
        }

        co_return ProjectError::OK;
    }

    Task<std::tuple<std::optional<Deployment>, ProjectError>> Storage::setupProjectCached(const Project &project,
                                                                                          const std::string userId) {
        const auto taskKey = createProjectSetupKey(project);

        if (const auto pending = co_await getOrStartTask<std::tuple<std::optional<Deployment>, ProjectError>>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        const auto projectId = project.getValueOfId();
        Deployment tmpDep;
        tmpDep.setProjectId(projectId);
        tmpDep.setStatus(enumToStr(DeploymentStatus::CREATED));
        tmpDep.setSourceRepo(project.getValueOfSourceRepo());
        tmpDep.setSourceBranch(project.getValueOfSourceBranch());
        tmpDep.setSourcePath(project.getValueOfSourcePath());
        if (!userId.empty())
            tmpDep.setUserId(userId);
        const auto dbResult = co_await global::database->addModel(tmpDep);
        if (!dbResult) {
            co_return {std::nullopt, ProjectError::UNKNOWN};
        }
        auto deployment = *dbResult;

        // Clean logs
        fs::remove(getProjectLogPath(project));

        const auto clonePath = getBaseDir().path() / TEMP_DIR / projectId;
        remove_all(clonePath);

        const auto projectLogger = getProjectLogger(project);
        ProjectError result;
        try {
            result = co_await setupProject(project, deployment, clonePath);
        } catch (std::exception e) {
            result = ProjectError::UNKNOWN;
            logger.error("Unexpected error during deployment: {}", e.what());
        }

        if (result == ProjectError::OK) {
            projectLogger->info("====================================");
            projectLogger->info("==     Project setup complete     ==");
            projectLogger->info("====================================");

            global::connections->complete(projectId, true);

            deployment.setStatus(enumToStr(DeploymentStatus::SUCCESS));
        } else {
            projectLogger->error("!!================================!!");
            projectLogger->error("!!      Project setup failed      !!");
            projectLogger->error("!!================================!!");

            global::connections->complete(projectId, false);

            deployment.setStatus(enumToStr(DeploymentStatus::ERROR));
        }

        remove_all(clonePath);
        co_await global::database->updateModel(deployment);

        co_return co_await completeTask<std::tuple<std::optional<Deployment>, ProjectError>>(taskKey, {deployment, result});
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

        const auto defaultVersion = co_await getOrCreateDefaultVersion(project);
        if (!defaultVersion) {
            co_return {std::nullopt, Error::ErrNotFound};
        }

        ResolvedProject resolved{project, rootDir, *defaultVersion};
        resolved.setLocale(locale);
        co_return {resolved, Error::Ok};
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

    Task<Error> Storage::invalidateProject(const Project &project) const {
        logger.debug("Invalidating project '{}'", project.getValueOfId());

        const auto basePath = getBaseDir().path() / project.getValueOfId();
        remove_all(basePath);

        co_return Error::Ok;
    }

    Task<std::optional<ResolvedProject>> Storage::maybeGetProject(const Project &project) const {
        const auto [resolved, error] = co_await findProject(project, std::nullopt, std::nullopt);
        co_return resolved;
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

    Task<std::tuple<std::optional<nlohmann::json>, ProjectError, std::string>>
    Storage::setupValidateTempProject(const Project &project) const {
        const auto baseDir = getBaseDir();
        const auto clonePath = baseDir.path() / TEMP_DIR / project.getValueOfId();
        remove_all(clonePath);

        const auto logger = getProjectLogger(project, false);

        // Clone project - validates repo and branch
        if (const auto [repo, cloneError] =
                co_await git::cloneRepository(project.getValueOfSourceRepo(), clonePath, project.getValueOfSourceBranch(), logger);
            !repo || cloneError.error != ProjectError::OK)
        {
            remove_all(clonePath);
            co_return {std::nullopt, cloneError.error, cloneError.message};
        }

        // Validate path
        const auto docsPath = clonePath / removeLeadingSlash(project.getValueOfSourcePath());
        if (!exists(docsPath)) {
            remove_all(clonePath);
            co_return {std::nullopt, ProjectError::NO_PATH, ""};
        }

        const ResolvedProject resolved{project, docsPath, ProjectVersion{}};

        // Validate metadata
        const auto [json, error, details] = resolved.validateProjectMetadata();
        if (error != ProjectError::OK) {
            remove_all(clonePath);
            co_return {std::nullopt, error, details};
        }

        remove_all(clonePath);
        co_return {*json, ProjectError::OK, ""};
    }

    Task<std::tuple<std::optional<Deployment>, Error>> Storage::deployProject(const Project &project, const std::string userId) {
        if (hasPendingTask(createProjectSetupKey(project))) {
            co_return {std::nullopt, Error::ErrInternal};
        }

        const auto [deployment, error] = co_await setupProjectCached(project, userId);
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
