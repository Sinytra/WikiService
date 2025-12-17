#include "storage.h"

#include <git2/repository.h>
#include <service/storage/ingestor/ingestor.h>
#include <service/database/project_database.h>
#include <service/storage/deployment.h>
#include <service/storage/gitops.h>
#include <service/util.h>

#define TEMP_DIR ".temp"

using namespace logging;
using namespace drogon;
using namespace service;
namespace fs = std::filesystem;

const std::set<std::string> allowedFileExtensions = {".mdx", ".json", ".png", ".jpg", ".jpeg", ".webp", ".gif"};

Error copyProjectFiles(const fs::path &docsRoot, const fs::path &dest, const std::shared_ptr<spdlog::logger> &projectLog) {
    projectLog->info("Copying project files for version '{}'", dest.filename().string());

    try {
        create_directories(dest);

        for (const auto &entry: fs::recursive_directory_iterator(docsRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
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
    } catch (std::exception &e) {
        projectLog->error("FS copy error: {}", e.what());
        return Error::ErrInternal;
    }

    projectLog->info("Done copying files");

    return Error::Ok;
}

namespace service {
    // clang-format off
    ENUM_TO_STR(DeploymentStatus,
        {DeploymentStatus::CREATED, "created"},
        {DeploymentStatus::LOADING, "loading"},
        {DeploymentStatus::SUCCESS, "success"},
        {DeploymentStatus::ERROR, "error"}
    )
    // clang-format on

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
                                                           const std::shared_ptr<spdlog::logger> logger, const std::shared_ptr<ProjectIssueCallback> &issues) {
        const auto path = resolved.getFormat().getWikiMetadataPath();
        const ProjectFileIssueCallback fileIssues(issues, path);

        const auto projectId = resolved.getProject().getValueOfId();
        const auto [json, err, det] = resolved.validateProjectMetadata();
        if (!json) {
            co_return std::vector<ProjectVersion>{};
        }

        std::unordered_map<std::string, ProjectVersion> existingVersions;
        for (const auto existing = co_await resolved.getProjectDatabase().getVersions(); auto &version: existing) {
            existingVersions.try_emplace(version.getValueOfName(), version);
        }

        const auto defaultBranch = resolved.getProject().getValueOfSourceBranch();
        const auto newVersions = readVersionsFromMetadata(*json, defaultBranch);

        std::vector<ProjectVersion> resolvedVersions;
        for (auto &[key, val]: newVersions) {
            if (!branches.contains(val)) {
                logger->warn("Ignoring version '{}' with unknown branch '{}'", key, val);
                co_await fileIssues.addIssue(ProjectIssueLevel::WARNING, ProjectIssueType::META, ProjectError::INVALID_VERSION_BRANCH, val);
                continue;
            }

            if (existingVersions.contains(key)) {
                ProjectVersion version = existingVersions[key];
                version.setBranch(val);
                if (const auto result = co_await global::database->updateModel(version)) {
                    resolvedVersions.push_back(*result);
                }
            } else {
                ProjectVersion version;
                version.setProjectId(projectId);
                version.setName(key);
                version.setBranch(val);
                if (const auto result = co_await global::database->createProjectVersion(version); result) {
                    resolvedVersions.push_back(*result);
                }
            }
        }

        co_return resolvedVersions;
    }

    Task<TaskResult<ProjectVersion>> createDefaultVersion(const Project &project) {
        logger.debug("Adding default version for project {}", project.getValueOfId());

        ProjectVersion defaultVersion;
        defaultVersion.setProjectId(project.getValueOfId());
        defaultVersion.setBranch(project.getValueOfSourceBranch());
        co_return co_await global::database->createProjectVersion(defaultVersion);
    }

    Task<TaskResult<>> setActiveDeployment(const std::string projectId, Deployment &deployment) {
        co_return co_await executeTransaction([projectId, &deployment](const Database &client) -> Task<> {
            if (const auto result = co_await client.deactivateDeployments(projectId); !result) {
                throw orm::Failure("Deployment deactivation failed");
            }

            deployment.setActive(true);
            co_await client.updateModel(deployment);
        });
    }

    Task<TaskResult<ProjectVersion>> Storage::getDefaultVersion(const Project &project) const {
        co_return co_await global::database->getDefaultProjectVersion(project.getValueOfId());
    }

    Task<ProjectError> Storage::deployProject(const Project &project, Deployment &deployment, const fs::path clonePath) const {
        const auto logger = getDeploymentLogger(deployment);
        logger->info("Setting up project");

        deployment.setStatus(enumToStr(DeploymentStatus::LOADING));
        co_await global::database->updateModel(deployment);

        const auto issues = std::make_shared<ProjectIssueCallback>(deployment.getValueOfId(), logger);

        // 1. Clone repository
        const auto [repo, cloneError] =
            co_await git::cloneRepository(project.getValueOfSourceRepo(), clonePath, project.getValueOfSourceBranch(), logger);
        if (!repo || cloneError.error != ProjectError::OK) {
            co_await issues->addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::GIT_CLONE, cloneError.error, cloneError.message);
            co_return cloneError.error;
        }

        // 2. Create default version if not exists
        auto defaultVersion = co_await getDefaultVersion(project);
        if (!defaultVersion) {
            defaultVersion = co_await createDefaultVersion(project);

            if (!defaultVersion) {
                logger->error("Project version creation database error.");
                co_await issues->addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::INTERNAL, ProjectError::UNKNOWN);
                co_return ProjectError::UNKNOWN;
            }
        }

        // 3. Assign revision info to deployment
        const auto revision = git::getLatestRevision(repo);
        if (!revision) {
            logger->error("Error getting commit information");
            co_await issues->addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::GIT_INFO, ProjectError::UNKNOWN);
            co_return ProjectError::UNKNOWN;
        }
        deployment.setRevision(nlohmann::json(*revision).dump());
        co_await global::database->updateModel(deployment);

        // 4. Ingest game content
        const auto projectLog = getProjectLogger(project, false);
        const auto cloneDocsRoot = clonePath / removeLeadingSlash(project.getValueOfSourcePath());
        ResolvedProject resolved{project, cloneDocsRoot, *defaultVersion, issues, projectLog};

        // 5. Validate metadata
        if (const auto [c, e, details] = resolved.validateProjectMetadata(); !details.empty()) {
            logger->error("Invalid project metadata found.");
            co_await issues->addIssue(ProjectIssueLevel::ERROR, ProjectIssueType::META, ProjectError::INVALID_META, details);
            co_return ProjectError::INVALID_META;
        }

        // Validate pages
        co_await resolved.validatePages();
        if (issues->hasErrors()) {
            logger->error("Found invalid page, aborting");
            co_return ProjectError::UNKNOWN;
        }

        // TODO Ingest from other versions?
        // TODO Move transaction scope up
        content::Ingestor ingestor{resolved, logger, issues, {}, true};
        if (const auto result = co_await ingestor.runIngestor(); result != Error::Ok) {
            logger->error("Error ingesting project data");
            co_return ProjectError::UNKNOWN;
        }

        if (issues->hasErrors()) {
            logger->error("Encountered issues during project setup, aborting");
            co_return ProjectError::UNKNOWN;
        }

        // 6. Setup versions
        const auto branches = git::listBranches(repo);
        const auto versions = co_await setupProjectVersions(resolved, branches, logger, issues);

        // 7. Copy default version
        const auto dest = getDeploymentVersionedDir(deployment);
        copyProjectFiles(cloneDocsRoot, dest, logger);

        // 8. Copy other versions
        // FIXME Error if wiki metadata does not exist in version / has errors
        for (const auto &version: versions) {
            const auto name = version.getValueOfName();
            const auto branch = version.getValueOfBranch();
            logger->info("Setting up version '{}' on branch '{}'", name, branch);

            const auto branchRef = branches.at(branch);
            if (const auto err = git::checkoutBranch(repo, branchRef, logger); err != Error::Ok) {
                continue;
            }

            const auto versionDest = getDeploymentVersionedDir(deployment, name);
            copyProjectFiles(cloneDocsRoot, versionDest, logger);
        }

        git_repository_free(repo);

        // 9. Set active
        if (const auto result = co_await setActiveDeployment(project.getValueOfId(), deployment); !result) {
            logger->error("Error setting active deployment");
            co_return ProjectError::UNKNOWN;
        }

        // 10. Free redundant versions
        std::vector<std::string> versionNames;
        for (auto &version: versions) {
            versionNames.push_back(version.getValueOfName());
        }
        if (const auto result = co_await resolved.getProjectDatabase().deleteUnusedVersions(versionNames); !result) {
            logger->warn("Error deleting old versions");
        }

        co_return ProjectError::OK;
    }

    Task<std::tuple<std::optional<Deployment>, ProjectError>> Storage::deployProjectCached(const Project &project,
                                                                                           const std::string userId) {
        const auto taskKey = createProjectSetupKey(project);

        if (const auto pending = co_await getOrStartTask<std::tuple<std::optional<Deployment>, ProjectError>>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        const auto activeDeployment(co_await global::database->getActiveDeployment(project.getValueOfId()));

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

        const auto deploymentDir = getDeploymentRootDir(deployment);
        remove_all(deploymentDir);
        fs::create_directories(deploymentDir);

        const auto clonePath = getBaseDir().path() / TEMP_DIR / (projectId + "-" + deployment.getValueOfId().substr(0, 9));
        remove_all(clonePath);

        const auto deployLog = getDeploymentLogger(deployment);
        ProjectError result;
        try {
            result = co_await deployProject(project, deployment, clonePath);
        } catch (std::exception &e) {
            result = ProjectError::UNKNOWN;
            logger.error("Unexpected error during deployment: {}", e.what());
        }

        if (result == ProjectError::OK) {
            deployLog->info("====================================");
            deployLog->info("==   Project deployment complete  ==");
            deployLog->info("====================================");

            deployment.setStatus(enumToStr(DeploymentStatus::SUCCESS));

            // Cleanup previous data
            try {
                if (activeDeployment) {
                    const auto oldPath = getDeploymentRootDir(*activeDeployment);
                    deployLog->info("Cleaning up previous deployment");
                    remove_all(oldPath);
                }
            } catch (std::exception &e) {
                const auto id = activeDeployment ? activeDeployment->getValueOfId() : "";
                logger.error("Failed to cleanup previous deployment '{}': {}", id, e.what());
            }
        } else {
            deployLog->error("!!================================!!");
            deployLog->error("!!   Project deployment failed    !!");
            deployLog->error("!!================================!!");

            deployment.setStatus(enumToStr(DeploymentStatus::ERROR));

            remove_all(deploymentDir);
        }

        remove_all(clonePath);
        co_await global::database->updateModel(deployment);

        global::connections->complete(projectId, result == ProjectError::OK);

        co_return co_await completeTask<std::tuple<std::optional<Deployment>, ProjectError>>(taskKey, {deployment, result});
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

        const auto issues = std::make_shared<ProjectIssueCallback>("", logger);
        const ResolvedProject resolved{project, docsPath, ProjectVersion{}, issues, logger};

        // Validate metadata
        const auto [json, error, details] = resolved.validateProjectMetadata();
        if (error != ProjectError::OK) {
            remove_all(clonePath);
            co_return {std::nullopt, error, details};
        }

        remove_all(clonePath);
        co_return {*json, ProjectError::OK, ""};
    }
}
