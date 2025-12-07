#pragma once

#include <drogon/utils/coroutine.h>
#include <nlohmann/json_fwd.hpp>
#include <service/cache.h>
#include <service/util.h>

namespace service {
    class GameDataService : public CacheableServiceBase {
    public:
        drogon::Task<TaskResult<>> importGameData(bool updateLoader) const;

        std::optional<nlohmann::json> getLang(const std::string &name) const;
    };
}

namespace global {
    extern std::shared_ptr<service::GameDataService> gameData;
}
