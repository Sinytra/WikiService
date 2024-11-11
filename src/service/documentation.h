#pragma once

#include "models/Mod.h"
#include "github.h"
#include "cache.h"
#include "util.h"

#include <optional>
#include <string>
#include <vector>
#include <drogon/utils/coroutine.h>

using namespace drogon_model::postgres;

namespace service
{
    class Documentation : public CacheableServiceBase {
    public:
        explicit Documentation(service::GitHub&, service::MemoryCache&);

        drogon::Task<std::tuple<bool, Error>> hasAvailableLocale(const Mod& mod, std::string locale, std::string installationToken);
        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getDocumentationPage(const Mod& mod, std::string path, std::optional<std::string> locale, std::optional<std::string> version, std::string installationToken);
        drogon::Task<std::tuple<Json::Value, Error>> getDirectoryTree(const Mod& mod, std::optional<std::string> locale, std::string installationToken);
        drogon::Task<std::tuple<std::optional<std::string>, Error>> getAssetResource(const Mod& mod, ResourceLocation location, std::string installationToken);
    private:
        drogon::Task<std::tuple<std::optional<std::vector<std::string>>, Error>> getAvailableLocales(const Mod& mod, std::string installationToken);

        service::GitHub& github_;
        service::MemoryCache& cache_;
    };
}
