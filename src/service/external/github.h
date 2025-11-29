#pragma once

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <service/util.h>

namespace service {
    class GitHub {
    public:
        explicit GitHub();

        drogon::Task<TaskResult<Json::Value>> getAuthenticatedUser(std::string token) const;
    };
}

namespace global {
    extern std::shared_ptr<service::GitHub> github;
}