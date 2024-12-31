#pragma once

#include "cache.h"

#include <drogon/utils/coroutine.h>
#include <vector>

namespace service {
    class CloudFlare : public CacheableServiceBase {
    public:
        explicit CloudFlare(const std::string &, const std::string &, const std::string &, MemoryCache &);

        drogon::Task<std::vector<std::string>> getMostVisitedProjectIDs();
    private:
        drogon::Task<std::vector<std::string>> computeMostVisitedProjectIDs() const;

        MemoryCache &cache_;
        const std::string &apiToken_;
        const std::string &accountTag_;
        const std::string &siteTag_;
    };
}