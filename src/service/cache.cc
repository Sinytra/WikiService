#include "cache.h"

#include <drogon/drogon.h>

using namespace drogon;
using namespace std::chrono_literals;

std::string join(const std::vector<std::string> &lst, const std::string &delim)
{
    std::string ret;
    for(const auto &s : lst) {
        if(!ret.empty())
            ret += delim;
        ret += s;
    }
    return ret;
}

namespace service {
    MemoryCache::MemoryCache() = default;

    Task<bool> MemoryCache::exists(std::string key) {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("EXISTS %s", key.data());
        co_return !resp.isNil() && resp.asInteger() == true;
    }

    Task<std::optional<std::string> > MemoryCache::getFromCache(std::string key) {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("GET %s", key.data());
        co_return resp.isNil() ? std::nullopt : std::optional{resp.asString()};
    }

    Task<bool> MemoryCache::isSetMember(std::string key, std::string value) {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("SISMEMBER %s %s", key.data(), value.data());
        co_return !resp.isNil() && resp.asInteger() == true;
    }

    Task<> MemoryCache::updateCache(std::string key, std::string value, std::chrono::duration<long> expire) {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();
        co_await client->execCommandCoro("SET %s %s EX %ld", key.data(), value.data(), expireSeconds);
    }

    Task<> MemoryCache::updateCacheSet(std::string key, std::vector<std::string> value,
                                       std::chrono::duration<long> expire) {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();
        std::string entries = join(value, " ");

        const auto trans = co_await client->newTransactionCoro();
        co_await client->execCommandCoro("SADD %s %s", key.data(), entries.data());
        co_await client->execCommandCoro("EXPIRE %s %ld", key.data(), expireSeconds);
        co_await trans->executeCoro();
    }
}
