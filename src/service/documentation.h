#pragma once

#include "cache.h"
#include "github.h"
#include "models/Project.h"
#include "util.h"

#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>
#include <vector>

using namespace drogon_model::postgres;

namespace service {
    class Documentation : public CacheableServiceBase {
    public:
        explicit Documentation(service::GitHub &, service::MemoryCache &);

        drogon::Task<std::tuple<bool, Error>> hasAvailableLocale(const Project &project, std::string locale, std::string installationToken);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getDocumentationPage(const Project &project, std::string path,
                                                                                         std::optional<std::string> locale,
                                                                                         std::string version,
                                                                                         std::string installationToken);
        drogon::Task<std::tuple<Json::Value, Error>> getDirectoryTree(const Project &project, std::string version,
                                                                      std::optional<std::string> locale, std::string installationToken);
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getAssetResource(const Project &project, ResourceLocation location,
                                                                                     std::string version, std::string installationToken);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getAvailableVersionsFiltered(const Project &project, std::string installationToken);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getAvailableVersions(const Project &project, std::string installationToken);
        drogon::Task<> invalidateCache(const Project &project);
    private:
        drogon::Task<std::tuple<std::optional<std::vector<std::string>>, Error>> computeAvailableLocales(const Project &project,
                                                                                                         std::string installationToken);
        drogon::Task<std::tuple<Json::Value, Error>> computeAvailableVersions(const Project &project, std::string installationToken);

        service::GitHub &github_;
        service::MemoryCache &cache_;
    };
}
