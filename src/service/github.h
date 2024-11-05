#pragma once

#include "error.h"

#include <tuple>
#include <optional>
#include <string>
#include <drogon/utils/coroutine.h>

#include "cache.h"

namespace service
{
    struct RepositoryContentsResponse
    {
        std::string body;
    };

    class GitHub
    {
    public:
        explicit GitHub(service::MemoryCache&);

        drogon::Task<std::tuple<std::optional<std::string>, Error>> getApplicationJWTToken();
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getRepositoryInstallation(std::string repo, std::string jwt);
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getInstallationToken(std::string installationId, std::string jwt);
        drogon::Task<std::tuple<std::optional<RepositoryContentsResponse>, Error>> getRepositoryContents(std::string repo, std::string path, std::string installationToken);
    private:
        service::MemoryCache& cache_;
    };
}
