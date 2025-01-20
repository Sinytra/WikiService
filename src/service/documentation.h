#pragma once

#include "cache.h"
#include "github.h"
#include "models/Project.h"

#include <drogon/utils/coroutine.h>

using namespace drogon_model::postgres;

namespace service {
    class Documentation : public CacheableServiceBase {
    public:
        explicit Documentation(GitHub &, MemoryCache &);

        drogon::Task<std::tuple<std::string, Error>> getIntallationToken(const Project &project) const;
        drogon::Task<bool> isPubliclyEditable(const Project &project);
        drogon::Task<> invalidateCache(const Project &project) const;
    private:
        GitHub &github_;
        MemoryCache &cache_;
    };
}
