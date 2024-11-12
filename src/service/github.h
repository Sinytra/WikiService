#pragma once

#include "error.h"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <string>
#include <tuple>

#include "cache.h"

namespace service {
    struct RepositoryContentsResponse {
        std::string body;
    };

    class GitHub {
    public:
        explicit GitHub(service::MemoryCache &, const std::string &, const std::string &);

        drogon::Task<std::tuple<std::optional<std::string>, Error>> getApplicationJWTToken();
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getRepositoryInstallation(std::string repo);
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getInstallationToken(std::string installationId);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getRepositoryJSONFile(std::string repo, std::string ref,
                                                                                          std::string path, std::string installationToken);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getRepositoryContents(std::string repo, std::string ref,
                                                                                          std::string path, std::string installationToken);
        drogon::Task<std::tuple<bool, Error>> isPublicRepository(std::string repo, std::string installationToken);
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getFileLastUpdateTime(std::string repo, std::string ref,
                                                                                          std::string path, std::string installationToken);

    private:
        service::MemoryCache &cache_;
        const std::string &appClientId_;
        const std::string &appPrivateKeyPath_;
    };
}
