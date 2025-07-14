#include "gitops.h"

#include <git2.h>
#include <include/uri.h>

#define BYTE_RECEIVE_LIMIT 2.5e+8

using namespace logging;
using namespace service;
using namespace drogon;
namespace fs = std::filesystem;

namespace git {
    bool is_local_url(const std::string &str) {
        try {
            const uri url{str};
            const auto scheme = url.get_scheme();
            return scheme == "file";
        } catch (std::exception e) {
            return false;
        }
    }

    std::string formatISOTime(const git_time_t &time) {
        const std::time_t utc_time = time;
        const std::tm *tm_ptr = std::gmtime(&utc_time);

        std::ostringstream oss;
        oss << std::put_time(tm_ptr, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    int fetch_progress(const git_indexer_progress *stats, void *payload) {
        const auto pd = static_cast<GitProgressData *>(payload);

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
        double megabytes = stats->received_bytes / (1024.0 * 1024.0);

        if (const auto mark = static_cast<long>(progress); pd->tick != -1 && (done || mark % 5 == 0 && mark > pd->tick)) {
            if (done) {
                pd->tick = -1;
            } else {
                pd->tick = mark;
            }

            pd->logger->trace("Fetch progress: {0:.2f}% ({1:.2f} MB)", progress, megabytes);
        }
        return 0;
    }

    // ReSharper disable once CppParameterNeverUsed
    void checkout_progress(const char *path, const size_t cur, const size_t tot, void *payload) {
        auto data = static_cast<GitProgressData *>(payload);
        if (cur == tot || data->tick++ % 1000 == 0) {
            const auto progress = cur / static_cast<double>(tot);
            data->logger->trace("Checkout progress: {0:.2f}%", progress * 100);
        }
    }

    Error checkoutBranch(git_repository *repo, const std::string &branch, const std::shared_ptr<spdlog::logger> &logger) {
        git_object *treeish = nullptr;

        GitProgressData d = {.tick = 0, .logger = logger};
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

    std::unordered_map<std::string, std::string> listBranches(git_repository *repo) {
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

    std::optional<GitRevision> getLatestRevision(const fs::path &path) {
        git_repository *repo = nullptr;
        if (git_repository_open(&repo, absolute(path).c_str()) != 0) {
            logger.error("Failed to open repository: {}", path.string());
            return std::nullopt;
        }
        return getLatestRevision(repo);
    }

    std::optional<GitRevision> getLatestRevision(git_repository *repo) {
        git_oid oid;
        if (git_reference_name_to_id(&oid, repo, "HEAD") != 0) {
            logger.error("Failed to get HEAD commit ID");
            return std::nullopt;
        }

        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, repo, &oid) != 0) {
            logger.error("Failed to lookup commit");
            return std::nullopt;
        }

        // Get full commit hash
        char full_hash[GIT_OID_HEXSZ + 1] = {};
        git_oid_fmt(full_hash, &oid);

        // Get short commit hash
        char short_hash[8] = {};
        git_oid_tostr(short_hash, sizeof(short_hash), &oid);

        // Get commit message
        const char *message = git_commit_summary(commit);

        // Get author details
        const git_signature *author = git_commit_author(commit);
        const char *author_name = author->name;
        const char *author_email = author->email;

        // Get commit date
        const git_time_t commit_time = git_commit_time(commit);
        const auto commitDate = formatISOTime(commit_time);

        git_commit_free(commit);

        return GitRevision{.hash = short_hash,
                           .fullHash = full_hash,
                           .message = message,
                           .authorName = author_name,
                           .authorEmail = author_email,
                           .date = commitDate};
    }

    Task<std::tuple<git_repository *, ProjectErrorInstance>> cloneRepository(const std::string url, const fs::path projectPath,
                                                                             const std::string branch,
                                                                             const std::shared_ptr<spdlog::logger> logger) {
        const auto path = absolute(projectPath);

        logger->info("Cloning git repository at {}", url);

        GitProgressData d = {.tick = 0, .error = "", .logger = logger};
        git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
        clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
        clone_opts.checkout_opts.progress_cb = checkout_progress;
        clone_opts.checkout_opts.progress_payload = &d;
        clone_opts.fetch_opts.callbacks.transfer_progress = fetch_progress;
        clone_opts.fetch_opts.callbacks.payload = &d;
        clone_opts.checkout_branch = branch.c_str();
        if (!is_local_url(url)) {
            clone_opts.fetch_opts.depth = 1;
        }

        git_repository *repo = nullptr;
        const int error = co_await supplyAsync<int>(
            [&repo, &url, &path, &clone_opts]() -> int { return git_clone(&repo, url.c_str(), path.c_str(), &clone_opts); });

        if (error < 0) {
            const git_error *e = git_error_last();
            logger->error("Git clone error: {}/{}: {}", error, e->klass, e->message);

            git_repository_free(repo);

            if (error == GIT_EAUTH) {
                logger->error("Authentication required but none was provided", branch);
                co_return {nullptr, {ProjectError::REQUIRES_AUTH, e->message}};
            }
            if (e->klass == GIT_ERROR_REFERENCE) {
                logger->error("Requested branch '{}' does not exist!", branch);
                co_return {nullptr, {ProjectError::NO_BRANCH, e->message}};
            }
            if (d.error == "size") {
                co_return {nullptr, {ProjectError::REPO_TOO_LARGE, e->message}};
            }

            co_return {nullptr, {ProjectError::UNKNOWN, e->message}};
        }

        logger->info("Git clone successful");

        co_return {repo, {ProjectError::OK}};
    }
}