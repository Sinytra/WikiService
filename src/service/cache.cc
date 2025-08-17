#include "cache.h"

#include <drogon/drogon.h>
#include "util.h"

#define INVALID_SET_MEMBER "_empty_"

using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    MemoryCache::MemoryCache() = default;

    Task<bool> MemoryCache::exists(std::string key) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("EXISTS %s", key.data());
        co_return !resp.isNil() && resp.asInteger() == true;
    }

    Task<bool> MemoryCache::isSetMember(std::string key, std::string value) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("SISMEMBER %s %s", key.data(), value.data());
        co_return !resp.isNil() && resp.asInteger() == true;
    }

    Task<std::optional<std::string>> MemoryCache::getFromCache(std::string key) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("GET %s", key.data());
        co_return resp.isNil() ? std::nullopt : std::optional{resp.asString()};
    }

    Task<std::optional<std::string>> MemoryCache::getHashMember(std::string key, std::string value) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("HGET %s %s", key.data(), value.data());
        co_return resp.isNil() ? std::nullopt : std::optional{resp.asString()};
    }

    Task<> MemoryCache::updateCache(std::string key, std::string value, std::chrono::duration<long> expire) const {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();
        if (expireSeconds > 0) {
            co_await client->execCommandCoro("SET %s %s EX %ld", key.data(), value.data(), expireSeconds);
        } else {
            co_await client->execCommandCoro("SET %s %s", key.data(), value.data());
        }
    }

    Task<> MemoryCache::updateCacheHash(std::string key, std::unordered_map<std::string, std::string> values,
                                        std::chrono::duration<long> expire) const {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();

        const auto trans = co_await client->newTransactionCoro();
        for (const auto& [field, value] : values) {
            co_await trans->execCommandCoro("HSET %s %s %s", key.data(), field.data(), value.data());
        }
        co_await trans->execCommandCoro("EXPIRE %s %ld", key.data(), expireSeconds);
        co_await trans->executeCoro();
    }

    Task<> MemoryCache::updateCacheSet(std::string key, const std::vector<std::string> value,
                                       const std::chrono::duration<long> expire) const {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();

        std::vector valueCopy(value);
        if (valueCopy.empty()) {
            valueCopy.push_back(INVALID_SET_MEMBER);
        }

        const auto trans = co_await client->newTransactionCoro();
        for (const auto &item: valueCopy) {
            co_await trans->execCommandCoro("SADD %s %s", key.data(), item.data());
        }
        co_await trans->execCommandCoro("EXPIRE %s %ld", key.data(), expireSeconds);
        co_await trans->executeCoro();
    }

    Task<> MemoryCache::erase(std::string key) const {
        const auto client = app().getFastRedisClient();
        co_await client->execCommandCoro("DEL %s", key.data());
    }
}
