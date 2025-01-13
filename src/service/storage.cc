#include "storage.h"
#include "util.h"

#include <filesystem>
#include <fstream>

#include <git2.h>
#include <spdlog/sinks/basic_file_sink.h>

#define TEMP_DIR ".temp"
#define LATEST_VERSION "latest"

using namespace logging;
using namespace drogon;
namespace fs = std::filesystem;

const std::set<std::string> allowedFileExtensions = {".mdx", ".json", ".png", ".jpg", ".jpeg", ".webp", ".gif"};

struct progress_data {
    std::shared_ptr<spdlog::logger> logger;
    size_t tick;
};

int fetch_progress(const git_indexer_progress *stats, void *payload) {
    const auto pd = static_cast<progress_data *>(payload);
    if (const auto done = stats->received_objects == stats->total_objects; pd->tick != -1 && (done || pd->tick++ % 1000 == 0)) {
        if (done) {
            pd->tick = -1;
        }

        const auto progress = stats->received_objects / static_cast<double>(stats->total_objects);
        pd->logger->trace("Fetch progress: {0:.2f}%", progress * 100);
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

std::shared_ptr<spdlog::logger> gerProjectLogger(const std::string &id, const fs::path &dir) {
    const auto file = dir / "project.log";

    auto projectLog = spdlog::basic_logger_mt(id, absolute(file).string());
    projectLog->set_level(spdlog::level::trace);
    projectLog->set_pattern("[%^%L%$] [%H:%M:%S %z] [%n] %v");
    return projectLog;
}

namespace service {
    Storage::Storage(const std::string &p, MemoryCache &c) : basePath_(p), cache_(c) {
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

    fs::path Storage::getProjectDirPath(const Project &project, const std::string &version = "") const {
        const fs::directory_entry baseDir = getBaseDir();
        const auto targetPath = baseDir.path() / project.getValueOfId() / (version.empty() ? LATEST_VERSION : version);
        return targetPath;
    }

    Task<std::tuple<git_repository *, Error>> gitCloneProject(const Project &project, const fs::path &projectPath,
                                                              const std::string &branch, const std::string &installationToken,
                                                              const std::shared_ptr<spdlog::logger> logger) {
        // TODO Support for non-github projects
        const auto url = std::format("https://oauth2:{}@github.com/{}", installationToken, project.getValueOfSourceRepo());
        const auto path = absolute(projectPath);

        logger->info("Cloning git repository");

        progress_data d = {.tick = 0, .logger = logger};
        git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
        clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
        clone_opts.checkout_opts.progress_cb = checkout_progress;
        clone_opts.checkout_opts.progress_payload = &d;
        clone_opts.fetch_opts.callbacks.transfer_progress = fetch_progress;
        clone_opts.fetch_opts.callbacks.payload = &d;
        clone_opts.checkout_branch = branch.c_str();
        // clone_opts.fetch_opts.depth = 1;

        git_repository *repo = nullptr;
        const int error = git_clone(&repo, url.c_str(), path.c_str(), &clone_opts);

        if (error < 0) {
            const git_error *e = git_error_last();
            logger->error("Git clone error: {}/{}: {}", error, e->klass, e->message);

            if (error == GIT_ERROR_REFERENCE) {
                logger->error("Docs branch '{}' does not exist!", project.getValueOfSourceBranch());
            }

            git_repository_free(repo);
            co_return {nullptr, Error::ErrInternal};
        }

        logger->info("Git clone successful");

        co_return {repo, Error::Ok};
    }

    Task<Error> Storage::setupProject(const Project &project, std::string installationToken) const {
        const auto baseDir = getBaseDir();
        const auto clonePath = baseDir.path() / TEMP_DIR / project.getValueOfId();
        remove_all(clonePath);

        const auto logger = gerProjectLogger(project.getValueOfId(), baseDir.path() / project.getValueOfId());

        logger->info("Setting up project");

        const auto [repo, cloneError] =
            co_await gitCloneProject(project, clonePath, project.getValueOfSourceBranch(), installationToken, logger);
        if (!repo || cloneError != Error::Ok) {
            co_return cloneError;
        }

        ResolvedProject resolved{project, installationToken, clonePath, clonePath / removeLeadingSlash(project.getValueOfSourcePath())};

        auto versions(resolved.getAvailableVersions());
        const auto branches = listRepoBranches(repo);

        for (auto it = versions.begin(); it != versions.end();) {
            if (!branches.contains(it->second)) {
                logger->warn("Ignoring version '{}' with unknown branch '{}'", it->first, it->second);
                it = versions.erase(it);
            } else {
                ++it;
            }
        }

        const auto dest = getProjectDirPath(project);
        copyProjectFiles(project, clonePath, dest, logger);

        for (const auto &[key, branch]: versions) {
            logger->info("Setting up version '{}' on branch '{}'", key, branch);

            const auto branchRef = branches.at(branch);
            if (const auto err = checkoutGitBranch(repo, branchRef, logger); err != Error::Ok) {
                continue;
            }

            const auto versionDest = getProjectDirPath(project, key);
            copyProjectFiles(project, clonePath, versionDest, logger);
        }

        remove_all(clonePath);

        git_repository_free(repo);

        logger->info("Project setup complete");

        co_return Error::Ok;
    }

    Task<Error> Storage::setupProjectCached(const Project &project, const std::string installationToken) {
        const auto taskKey = "setup:" + project.getValueOfId();

        if (const auto pending = co_await getOrStartTask<Error>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        auto result = co_await setupProject(project, installationToken);

        co_return co_await completeTask<Error>(taskKey, std::move(result));
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>>
    Storage::findProject(const Project &project, const std::string installationToken, const std::optional<std::string> &version,
                         const std::optional<std::string> &locale, const bool setup) {
        const auto branch = project.getValueOfSourceBranch();
        const auto rootDir = getProjectDirPath(project, version.value_or(""));

        if (!exists(rootDir)) {
            if (!setup) {
                co_return {std::nullopt, Error::ErrNotFound};
            }

            if (const auto res = co_await setupProjectCached(project, installationToken); res != Error::Ok) {
                logger.error("Failed to setup project '{}'", project.getValueOfId());
                co_return {std::nullopt, res};
            }
        }

        const auto docsDir = rootDir / removeLeadingSlash(project.getValueOfSourcePath());

        ResolvedProject resolved{project, installationToken, rootDir, docsDir};
        resolved.setLocale(locale);

        co_return {resolved, Error::Ok};
    }

    Task<std::tuple<std::optional<ResolvedProject>, Error>> Storage::getProject(const Project &project, const std::string installationToken,
                                                                                const std::optional<std::string> &version,
                                                                                const std::optional<std::string> &locale) {
        const auto [defaultProject, error] = co_await findProject(project, installationToken, std::nullopt, locale, true);

        if (defaultProject && version) {
            const auto [vProject, vError] = co_await findProject(project, installationToken, version, locale, false);
            if (!vProject && defaultProject->getAvailableVersions().contains(*version)) {
                logger.error("Failed to find existing version '{}' for '{}'", *version, project.getValueOfId());
            }
            co_return {vProject, vError};
        }

        co_return {defaultProject, error};
    }

    Task<std::tuple<std::vector<std::string>, Error>> Storage::getRepositoryBranches(const Project &project) const {
        const auto repoPath = getProjectDirPath(project);
        std::vector<std::string> repoBranches;

        git_repository *repo = nullptr;
        if (git_repository_open(&repo, repoPath.c_str()) != 0) {
            logger.error("Failed to open repository: {}", repoPath.string());
            co_return {repoBranches, Error::ErrInternal};
        }

        for (const auto branches = listRepoBranches(repo); const auto &key: branches | std::views::keys) {
            repoBranches.emplace_back(key);
        }

        git_repository_free(repo);

        co_return {repoBranches, Error::Ok};
    }

    Task<Error> Storage::invalidateProject(const Project &project) {
        const auto [resolved, error] = co_await findProject(project, "", std::nullopt, std::nullopt, false);
        if (!resolved) {
            co_return error;
        }

        logger.debug("Invalidating project '{}'", project.getValueOfId());

        const auto repoPath = getBaseDir().path() / project.getValueOfId();
        remove_all(repoPath);

        co_return Error::Ok;
    }
}
