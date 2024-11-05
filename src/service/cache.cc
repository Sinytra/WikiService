#include "cache.h"

#include <drogon/drogon.h>

using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    MemoryCache::MemoryCache() = default;

    Task<std::optional<std::string>> MemoryCache::getFromCache(std::string key) {
        const auto client = app().getRedisClient();
        const auto resp = co_await client->execCommandCoro("GET %s", key.data());
        co_return resp.isNil() ? std::nullopt : std::optional{resp.asString()};
    }

    Task<> MemoryCache::updateCache(std::string key, std::string value, std::chrono::duration<long> expire) {
        const auto client = app().getRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();
        co_await client->execCommandCoro("SET %s %s EX %ld", key.data(), value.data(), expireSeconds);
    }
}
