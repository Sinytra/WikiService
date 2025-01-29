#pragma once

#include "error.h"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <set>
#include <tuple>

namespace service {
    class GitHub {
    public:
        explicit GitHub();

        drogon::Task<std::tuple<std::optional<Json::Value>, Error>> getAuthenticatedUser(std::string token) const;
    };
}
