#pragma once

#include "error.h"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <string>
#include <tuple>

#include "cache.h"

namespace service {
    class GitHub {
    public:
        explicit GitHub(MemoryCache &, const std::string &, const std::string &, const std::string &);

        drogon::Task<std::tuple<std::optional<std::string>, Error>> getUsername(std::string token) const;
        [[nodiscard]] std::string getAppInstallUrl() const;

        drogon::Task<std::tuple<std::optional<std::string>, Error>> getApplicationJWTToken() const;
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getRepositoryInstallation(std::string repo) const;
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getInstallationToken(std::string installationId) const;
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getCollaboratorPermissionLevel(std::string repo, std::string username,
                                                                                                   std::string installationToken) const;
        drogon::Task<std::tuple<std::vector<std::string>, Error>> getRepositoryBranches(std::string repo,
                                                                                        std::string installationToken) const;
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>>
        getRepositoryJSONFile(std::string repo, std::string ref, std::string path, std::string installationToken) const;
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>>
        getRepositoryContents(std::string repo, std::string ref, std::string path, std::string installationToken) const;
        drogon::Task<std::tuple<bool, Error>> isPublicRepository(std::string repo, std::string installationToken) const;
        drogon::Task<std::tuple<std::optional<std::string>, Error>>
        getFileLastUpdateTime(std::string repo, std::string ref, std::string path, std::string installationToken) const;
        drogon::Task<> invalidateCache(std::string repo) const;
        drogon::Task<std::string> getNewRepositoryLocation(std::string repo) const;

    private:
        MemoryCache &cache_;
        const std::string &appName_;
        const std::string &appClientId_;
        const std::string &appPrivateKeyPath_;
    };
}
