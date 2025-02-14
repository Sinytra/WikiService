#pragma once

#include "cache.h"
#include "config.h"

#include <drogon/utils/coroutine.h>
#include <vector>

namespace service {
    class CloudFlare : public CacheableServiceBase {
    public:
        explicit CloudFlare(const config::CloudFlare &);

        drogon::Task<std::vector<std::string>> getMostVisitedProjectIDs();
    private:
        drogon::Task<std::vector<std::string>> computeMostVisitedProjectIDs() const;

        const config::CloudFlare &config_;
    };
}