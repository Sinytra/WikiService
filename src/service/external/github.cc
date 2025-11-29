#include "github.h"

#include <log/log.h>
#include <service/util.h>

#define GITHUB_API_URL "https://api.github.com/"

using namespace logging;
using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    GitHub::GitHub() = default;

    Task<TaskResult<Json::Value>> GitHub::getAuthenticatedUser(const std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        auto user = co_await sendAuthenticatedRequest(client, Get, "/user", token);
        if (user && user->isObject()) {
            co_return *user;
        }
        co_return user.error();
    }
}
