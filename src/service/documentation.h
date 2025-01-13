#pragma once

#include "cache.h"
#include "github.h"
#include "models/Project.h"
#include "storage.h"

#include <drogon/utils/coroutine.h>

using namespace drogon_model::postgres;

namespace service {
    class Documentation : public CacheableServiceBase {
    public:
        explicit Documentation(GitHub &, MemoryCache &);

        drogon::Task<bool> isPubliclyEditable(const Project &project, std::string installationToken);
        drogon::Task<> invalidateCache(const Project &project) const;
    private:
        GitHub &github_;
        MemoryCache &cache_;
    };
}
