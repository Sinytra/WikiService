#include "github.h"

#include <log/log.h>
#include <service/util.h>

#define GITHUB_API_URL "https://api.github.com/"

using namespace logging;
using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    GitHub::GitHub() = default;

    Task<std::tuple<std::optional<Json::Value>, Error>> GitHub::getAuthenticatedUser(const std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        auto [user, err] = co_await sendAuthenticatedRequest(client, Get, "/user", token);
        if (user && user->isObject()) {
            co_return {*user, err};
        }
        co_return {std::nullopt, err};
    }
}
