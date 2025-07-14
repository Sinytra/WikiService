#pragma once

#include <drogon/utils/coroutine.h>
#include <nlohmann/json.hpp>
#include <service/error.h>

namespace sys {
    class SystemDataImporter {
    public:
        drogon::Task<service::Error> importData(nlohmann::json rawData) const;
    };

    drogon::Task<service::Error> runInitialDeployments();
}
