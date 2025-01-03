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

    Task<std::optional<std::string>> MemoryCache::getFromCache(std::string key) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("GET %s", key.data());
        co_return resp.isNil() ? std::nullopt : std::optional{resp.asString()};
    }

    Task<bool> MemoryCache::isSetMember(std::string key, std::string value) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("SISMEMBER %s %s", key.data(), value.data());
        co_return !resp.isNil() && resp.asInteger() == true;
    }

    Task<std::set<std::string>> MemoryCache::getSetMembers(std::string key) const {
        const auto client = app().getFastRedisClient();
        const auto resp = co_await client->execCommandCoro("SMEMBERS %s", key.data());
        std::set<std::string> members;
        if (!resp.isNil()) {
            for (const auto &item : resp.asArray()) {
                members.insert(item.asString());
            }
        }
        members.erase(INVALID_SET_MEMBER);
        co_return members;
    }

    Task<> MemoryCache::updateCache(std::string key, std::string value, std::chrono::duration<long> expire) const {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();
        co_await client->execCommandCoro("SET %s %s EX %ld", key.data(), value.data(), expireSeconds);
    }

    Task<> MemoryCache::updateCacheSet(std::string key, const std::vector<std::string> value, const std::chrono::duration<long> expire) const {
        const auto client = app().getFastRedisClient();
        const auto expireSeconds = std::chrono::seconds(expire).count();

        std::vector valueCopy(value);
        if (valueCopy.empty()) {
            valueCopy.push_back(INVALID_SET_MEMBER);
        }

        const auto trans = co_await client->newTransactionCoro();
        for (const auto &item : valueCopy) {
            co_await trans->execCommandCoro("SADD %s %s", key.data(), item.data());
        }
        co_await trans->execCommandCoro("EXPIRE %s %ld", key.data(), expireSeconds);
        co_await trans->executeCoro();
    }

    Task<> MemoryCache::erase(std::string key) const {
        const auto client = app().getFastRedisClient();
        co_await client->execCommandCoro("DEL %s", key.data());
    }

    Task<> MemoryCache::erase(std::vector<std::string> keys) const {
        const auto client = app().getFastRedisClient();
        const auto toErase = join(keys, " ");

        co_await client->execCommandCoro("DEL %s", toErase.data());
    }

    Task<> MemoryCache::eraseNamespace(std::string key) const {
        const auto client = app().getFastRedisClient();
        std::string cursor = "0";
        while (true) {
            if (const auto result = (co_await client->execCommandCoro("SCAN %s MATCH %s:*", cursor.data(), key.data())).asArray(); result.size() > 1) {
                cursor = result[0].asString();
                if (const auto keys = result[1].asArray(); !keys.empty()) {
                    std::string entries;
                    for (const auto &entry: keys) {
                        if (!entries.empty())
                            entries += " ";
                        entries += entry.asString();
                    }
                    co_await client->execCommandCoro("DEL " + entries);
                }
                if (cursor == "0") {
                    break;
                }
            } else {
                break;
            }
        }
        co_return;
    }
}
