#pragma once

#include "error.h"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "cache.h"

namespace service {
    class GitHub : public CacheableServiceBase {
    public:
        explicit GitHub(MemoryCache &, const std::string &, const std::string &, const std::string &);

        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getAuthenticatedUser(std::string token) const;

        drogon::Task<std::tuple<std::optional<std::string>, Error>> getApplicationJWTToken() const;
        drogon::Task<std::tuple<std::vector<std::string>, Error>> getRepositoryBranches(std::string repo,
                                                                                        std::string installationToken) const;
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>>
        getRepositoryJSONFile(std::string repo, std::string ref, std::string path, std::string installationToken) const;
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>>
        getRepositoryContents(std::string repo, std::string ref, std::string path, std::string installationToken) const;
    private:
        MemoryCache &cache_;
        const std::string &appName_;
        const std::string &appClientId_;
        const std::string &appPrivateKeyPath_;
    };
}
