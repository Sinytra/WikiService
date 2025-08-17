#pragma once

#include <drogon/utils/coroutine.h>
#include <git2/indexer.h>
#include <memory>
#include <service/error.h>
#include <service/resolved.h>
#include <spdlog/spdlog.h>
#include <string>

namespace git {
    struct GitProgressData {
        size_t tick;
        std::string error;
        std::shared_ptr<spdlog::logger> logger;
    };

    struct GitRevision {
        std::string hash;
        std::string fullHash;
        std::string message;
        std::string authorName;
        std::string authorEmail;
        std::string date;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GitRevision, hash, fullHash, message, authorName, authorEmail, date)
    };

    service::Error checkoutBranch(git_repository *repo, const std::string &branch, const std::shared_ptr<spdlog::logger> &logger);

    std::unordered_map<std::string, std::string> listBranches(git_repository *repo);

    std::optional<GitRevision> getLatestRevision(const std::filesystem::path &path);

    std::optional<GitRevision> getLatestRevision(git_repository *repo);

    drogon::Task<std::tuple<git_repository *, service::ProjectErrorInstance>> cloneRepository(std::string url,
                                                                                              std::filesystem::path projectPath,
                                                                                              std::string branch,
                                                                                              std::shared_ptr<spdlog::logger> logger);
}
