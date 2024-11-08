#pragma once

#include "models/Mod.h"
#include "github.h"
#include "cache.h"

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
    private:
        drogon::Task<std::tuple<std::optional<std::vector<std::string>>, Error>> getAvailableLocales(const Mod& mod, std::string installationToken);

        service::GitHub& github_;
        service::MemoryCache& cache_;
    };
}
