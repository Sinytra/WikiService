#pragma once

#include "cache.h"
#include "config.h"

#include <drogon/utils/coroutine.h>
#include <vector>

namespace service {
    class CloudFlare : public CacheableServiceBase {
    public:
        explicit CloudFlare(const config::CloudFlare &, MemoryCache &);

        drogon::Task<std::vector<std::string>> getMostVisitedProjectIDs();
    private:
        drogon::Task<std::vector<std::string>> computeMostVisitedProjectIDs() const;

        MemoryCache &cache_;
        const config::CloudFlare &config_;
    };
}