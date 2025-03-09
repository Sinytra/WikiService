#include "storage.h"

#include <database.h>

#include "util.h"

#include <filesystem>
#include <fstream>

#include <git2.h>
#include <models/ProjectVersion.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define TEMP_DIR ".temp"
#define LATEST_VERSION "latest"
#define BYTE_RECEIVE_LIMIT 2.5e+8

#define WS_SUCCESS "<<success"
#define WS_ERROR "<<error"

using namespace logging;
using namespace drogon;
using namespace service;
namespace fs = std::filesystem;

const std::set<std::string> allowedFileExtensions = {".mdx", ".json", ".png", ".jpg", ".jpeg", ".webp", ".gif"};

struct progress_data {
    size_t tick;
    std::string error;
    std::shared_ptr<spdlog::logger> logger;
};

int fetch_progress(const git_indexer_progress *stats, void *payload) {
    const auto pd = static_cast<progress_data *>(payload);

    if (stats->received_bytes > BYTE_RECEIVE_LIMIT) {
        if (pd->tick != -1) {
            pd->tick = -1;
            pd->error = "size";
            pd->logger->error("Terminating fetch as it exceeded maximum size limit of 250MB");
        }
        return 1;
    }

    const auto progress = stats->received_objects / static_cast<double>(stats->total_objects) * 100;
    const auto done = stats->received_objects == stats->total_objects;

    if (const auto mark = static_cast<long>(progress); pd->tick != -1 && (done || mark % 5 == 0 && mark > pd->tick)) {
        if (done) {
            pd->tick = -1;
        } else {
            pd->tick = mark;
        }

        pd->logger->trace("Fetch progress: {0:.2f}%", progress);
    }
    return 0;
}

void checkout_progress(const char *path, const size_t cur, const size_t tot, void *payload) {
    auto pd = static_cast<progress_data *>(payload);
    if (cur == tot || pd->tick++ % 1000 == 0) {
        const auto progress = cur / static_cast<double>(tot);
        pd->logger->trace("Checkout progress: {0:.2f}%", progress * 100);
    }
}

bool isSubpath(const fs::path &path, const fs::path &base) {
    const auto [fst, snd] = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
    return snd == base.end();
}

Error checkoutGitBranch(git_repository *repo, const std::string &branch, const std::shared_ptr<spdlog::logger> &logger) {
    git_object *treeish = nullptr;

    progress_data d = {.tick = 0, .logger = logger};
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    opts.progress_cb = checkout_progress;
    opts.progress_payload = &d;

    if (git_revparse_single(&treeish, repo, branch.c_str()) != 0) {
        logger->error("Failed to parse tree '{}'", branch);
        return Error::ErrInternal;
    }
    if (git_checkout_tree(repo, treeish, &opts) != 0) {
        logger->error("Failed to checkout tree '{}'", branch);
        return Error::ErrInternal;
    }

    const auto head = "refs/heads/" + branch;
    git_repository_set_head(repo, head.c_str());

    git_object_free(treeish);

    return Error::Ok;
}

Error copyProjectFiles(const Project &project, const fs::path &src, const fs::path &dest, const std::shared_ptr<spdlog::logger> &logger) {
    const auto docsPath = src / removeLeadingSlash(project.getValueOfSourcePath());
    const auto gitPath = src / ".git";

    logger->info("Copying project files for version '{}'", dest.filename().string());

    create_directories(dest);

    for (const auto &entry: fs::recursive_directory_iterator(src)) {
        if (const auto isGitSubdir = isSubpath(entry.path(), gitPath);
            entry.is_regular_file() && (isSubpath(entry.path(), docsPath) || isGitSubdir))
        {
            fs::path relative_path = relative(entry.path(), src);

            if (const auto extension = entry.path().extension().string(); !isGitSubdir && !allowedFileExtensions.contains(extension)) {
                logger->warn("Ignoring file {}", relative_path.string());
                continue;
            }

            if (fs::path destination_path = dest / relative_path.parent_path(); !exists(destination_path)) {
                create_directories(destination_path);
            }

            copy(entry, dest / relative_path, fs::copy_options::overwrite_existing);
        }
    }

    logger->info("Done copying files");

    return Error::Ok;
}

std::unordered_map<std::string, std::string> listRepoBranches(git_repository *repo) {
    std::unordered_map<std::string, std::string> branches;

    git_branch_iterator *it;
    if (!git_branch_iterator_new(&it, repo, GIT_BRANCH_ALL)) {
        git_reference *ref;
        git_branch_t type;
        while (!git_branch_next(&ref, &type, it)) {
            const char *branch_name = nullptr;
            if (!git_reference_symbolic_target(ref) && !git_branch_name(&branch_name, ref) && branch_name) {
                const auto refName = git_reference_name(ref);
                if (const auto str = std::string(branch_name); !branches.contains(str)) {
                    const auto shortName = str.substr(str.find('/') + 1);
                    branches.try_emplace(shortName, refName);
                }
            }
            git_reference_free(ref);
        }
        git_branch_iterator_free(it);
    }

    return branches;
}

class FormattedCallbackSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit FormattedCallbackSink(const std::function<void(const std::string &msg)> &callback) : callback_(callback) {}

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        callback_(fmt::to_string(formatted));
    }
    void flush_() override {}

private:
    const std::function<void(const std::string &msg)> callback_;
};

std::string createProjectSetupKey(const Project &project) { return "setup:" + project.getValueOfId(); }

namespace service {
    std::string projectStatusToString(const ProjectStatus status) {
        switch (status) {
            case LOADING:
                return "loading";
            case LOADED:
                return "loaded";
            case ERROR:
                return "error";
            default:
                return "unknown";
        }
    }

    void RealtimeConnectionStorage::connect(const std::string &project, const WebSocketConnectionPtr &connection) {
        std::unique_lock lock(mutex_);

        connections_[project].push_back(connection);
        if (const auto messages = pending_.find(project); messages != pending_.end()) {
            for (const auto &message: messages->second) {
                connection->send(message);
            }
        }
    }

    void RealtimeConnectionStorage::disconnect(const WebSocketConnectionPtr &connection) {
        std::unique_lock lock(mutex_);

        for (auto it = connections_.begin(); it != connections_.end();) {
            auto &vec = it->second;

            std::erase(vec, connection);

            if (vec.empty()) {
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void RealtimeConnectionStorage::broadcast(const std::string &project, const std::string &message) {
        {
            std::shared_lock lock(mutex_);

            if (const auto conns = connections_.find(project); conns != connections_.end()) {
                for (const WebSocketConnectionPtr &conn: conns->second) {
                    conn->send(message);
                }
                return;
            }
        }

        std::unique_lock lock(mutex_);
        pending_[project].push_back(message);
    }

    void RealtimeConnectionStorage::shutdown(const std::string &project) {
        std::unique_lock lock(mutex_);

        if (const auto it = connections_.find(project); it != connections_.end()) {
            for (const std::vector<WebSocketConnectionPtr> copy = it->second; const WebSocketConnectionPtr &conn: copy) {
                conn->shutdown();
            }
        }
        connections_.erase(project);
        pending_.erase(project);
    }

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

    std::shared_ptr<spdlog::logger> Storage::getProjectLogger(const Project &project, bool file) const {
        const auto id = project.getValueOfId();

        std::vector<spdlog::sink_ptr> sinks;

        const auto callbackSink =
            std::make_shared<FormattedCallbackSink>([&, id](const std::string &msg) { global::connections->broadcast(id, msg); });
        callbackSink->set_pattern("[%^%L%$] [%Y-%m-%d %T] [%n] %v");
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

    Task<std::tuple<git_repository *, ProjectError>> gitCloneProject(const Project &project, const fs::path &projectPath,
                                                                     const std::string &branch,
                                                                     const std::shared_ptr<spdlog::logger> logger) {
        const auto url = project.getValueOfSourceRepo();
        const auto path = absolute(projectPath);

        logger->info("Cloning git repository at {}", url);

        progress_data d = {.tick = 0, .error = "", .logger = logger};
        git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
        clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
        clone_opts.checkout_opts.progress_cb = checkout_progress;
        clone_opts.checkout_opts.progress_payload = &d;
        clone_opts.fetch_opts.callbacks.transfer_progress = fetch_progress;
        clone_opts.fetch_opts.callbacks.payload = &d;
        clone_opts.checkout_branch = branch.c_str();
        clone_opts.fetch_opts.depth = 1;

        git_repository *repo = nullptr;
        const int error = git_clone(&repo, url.c_str(), path.c_str(), &clone_opts);

        if (error < 0) {
            const git_error *e = git_error_last();
            logger->error("Git clone error: {}/{}: {}", error, e->klass, e->message);

            git_repository_free(repo);

            if (error == GIT_EAUTH) {
                logger->error("Authentication required but none was provided", project.getValueOfSourceBranch());
                co_return {nullptr, ProjectError::NO_REPOSITORY};
            }
            if (e->klass == GIT_ERROR_REFERENCE) {
                logger->error("Docs branch '{}' does not exist!", project.getValueOfSourceBranch());
                co_return {nullptr, ProjectError::NO_BRANCH};
            }
            if (d.error == "size") {
                co_return {nullptr, ProjectError::REPO_TOO_LARGE};
            }

            co_return {nullptr, ProjectError::UNKNOWN};
        }

        logger->info("Git clone successful");

        co_return {repo, ProjectError::OK};
    }

    std::unordered_map<std::string, std::string> readVersionsFromMetadata(const nlohmann::json &metadata, const std::string &defaultBranch) {
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

    Task<ProjectError> Storage::setupProject(const Project &project) const {
        const auto baseDir = getBaseDir();
        const auto clonePath = baseDir.path() / TEMP_DIR / project.getValueOfId();
        remove_all(clonePath);

        const auto projectBaseDir = getBaseDir().path() / project.getValueOfId();
        remove_all(projectBaseDir);

        const auto logger = getProjectLogger(project);

        logger->info("Setting up project");

        const auto [repo, cloneError] = co_await gitCloneProject(project, clonePath, project.getValueOfSourceBranch(), logger);
        if (!repo || cloneError != ProjectError::OK) {
            co_return cloneError;
        }

        ResolvedProject resolved{project, clonePath, clonePath / removeLeadingSlash(project.getValueOfSourcePath())};

        const auto branches = listRepoBranches(repo);
        const auto versions = co_await setupProjectVersions(resolved, branches, logger);

        const auto dest = getProjectDirPath(project);
        copyProjectFiles(project, clonePath, dest, logger);

        for (const auto &version: versions) {
            const auto name = version.getValueOfName();
            const auto branch = version.getValueOfBranch();
            logger->info("Setting up version '{}' on branch '{}'", name, branch);

            const auto branchRef = branches.at(branch);
            if (const auto err = checkoutGitBranch(repo, branchRef, logger); err != Error::Ok) {
                continue;
            }

            const auto versionDest = getProjectDirPath(project, name);
            copyProjectFiles(project, clonePath, versionDest, logger);
        }

        remove_all(clonePath);

        git_repository_free(repo);

        logger->info("Project import complete");

        // TODO Does not work on first import
        ResolvedProject finalResolved{project, dest, dest / removeLeadingSlash(project.getValueOfSourcePath())};
        content::Ingestor ingestor{finalResolved, logger};
        try {
            co_await ingestor.ingestGameContentData();
        } catch (std::exception e) {
            logger->error("Error ingesting project data");
            logging::logger.error("Error ingesting project data: {}", e.what());
        }

        logger->info("====================================");
        logger->info("Project setup complete");
        logger->info("====================================");

        co_return ProjectError::OK;
    }

    Task<ProjectError> Storage::setupProjectCached(const Project &project) {
        const auto taskKey = createProjectSetupKey(project);

        if (const auto pending = co_await getOrStartTask<ProjectError>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        auto result = co_await setupProject(project);
        if (result == ProjectError::OK) {
            global::connections->broadcast(project.getValueOfId(), WS_SUCCESS);
        } else {
            global::connections->broadcast(project.getValueOfId(), WS_ERROR);
        }

        global::connections->shutdown(project.getValueOfId());

        co_return co_await completeTask<ProjectError>(taskKey, std::move(result));
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>> Storage::findProject(const Project &project,
                                                                                 const std::optional<std::string> &version,
                                                                                 const std::optional<std::string> &locale,
                                                                                 const bool setup) {
        const auto branch = project.getValueOfSourceBranch();
        const auto rootDir = getProjectDirPath(project, version.value_or(""));

        if (!exists(rootDir)) {
            if (!setup) {
                co_return {std::nullopt, Error::ErrNotFound};
            }

            if (const auto res = co_await setupProjectCached(project); res != ProjectError::OK) {
                logger.error("Failed to setup project '{}'", project.getValueOfId());
                co_return {std::nullopt, Error::ErrInternal};
            }
        }

        const auto docsDir = rootDir / removeLeadingSlash(project.getValueOfSourcePath());

        if (version) {
            const auto resolvedVersion = co_await global::database->getProjectVersion(project.getValueOfId(), *version);
            if (!resolvedVersion) {
                co_return {std::nullopt, Error::ErrNotFound};
            }

            ResolvedProject resolved{project, rootDir, docsDir, *resolvedVersion};
            resolved.setLocale(locale);
            co_return {resolved, Error::Ok};
        }

        ResolvedProject resolved{project, rootDir, docsDir};
        resolved.setLocale(locale);
        co_return {resolved, Error::Ok};
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>>
    Storage::getProject(const Project &project, const std::optional<std::string> &version, const std::optional<std::string> &locale) {
        const auto [defaultProject, error] = co_await findProject(project, std::nullopt, locale, true);

        if (defaultProject && version) {
            if (auto [vProject, vError] = co_await findProject(project, version, locale, false); vProject) {
                vProject->setDefaultVersion(*defaultProject);
                co_return {vProject, vError};
            }
            if (!co_await defaultProject->hasVersion(*version)) {
                logger.error("Failed to find existing version '{}' for '{}'", *version, project.getValueOfId());
            }
        }

        co_return {defaultProject, error};
    }

    Task<Error> Storage::invalidateProject(const Project &project) {
        const auto [resolved, error] = co_await findProject(project, std::nullopt, std::nullopt, false);
        if (!resolved) {
            co_return error;
        }

        logger.debug("Invalidating project '{}'", project.getValueOfId());

        const auto basePath = getBaseDir().path() / project.getValueOfId();
        remove_all(basePath);

        co_return Error::Ok;
    }

    Task<std::optional<ResolvedProject>> Storage::maybeGetProject(const Project &project) {
        const auto [resolved, error] = co_await findProject(project, std::nullopt, std::nullopt, false);
        co_return resolved;
    }

    Task<ProjectStatus> Storage::getProjectStatus(const Project &project) {
        const auto taskKey = createProjectSetupKey(project);

        if (const auto pending = getPendingTask<ProjectError>(taskKey)) {
            co_return LOADING;
        }

        if (const auto resolved = co_await maybeGetProject(project); !resolved) {
            if (const auto filePath = getProjectLogPath(project); exists(filePath)) {
                co_return ERROR;
            }

            co_return UNKNOWN;
        }

        co_return LOADED;
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
        if (const auto [repo, cloneError] = co_await gitCloneProject(project, clonePath, project.getValueOfSourceBranch(), logger);
            !repo || cloneError != ProjectError::OK)
        {
            remove_all(clonePath);
            co_return {std::nullopt, cloneError, ""};
        }

        // Validate path
        const auto docsPath = clonePath / removeLeadingSlash(project.getValueOfSourcePath());
        if (!exists(docsPath)) {
            remove_all(clonePath);
            co_return {std::nullopt, ProjectError::NO_PATH, ""};
        }

        const ResolvedProject resolved{project, clonePath, docsPath};

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
