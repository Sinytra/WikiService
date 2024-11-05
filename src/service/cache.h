#pragma once

#include "error.h"

#include <optional>
#include <string>
#include <chrono>
#include <drogon/utils/coroutine.h>

namespace service
{
    class MemoryCache
    {
    public:
        MemoryCache();

    public:
        drogon::Task<std::optional<std::string>> getFromCache(std::string key);
        drogon::Task<> updateCache(std::string key, std::string value, std::chrono::duration<long> expire);
    };
}

// template <typename T>
// drogon::Task<T> getFromCache(
//     const std::string &key,
//     const drogon::nosql::RedisClientPtr &client) noexcept(false)
// {
//     auto value = co_await client->execCommandCoro("get %s", key.data());
//     co_return value;
// }

// template <typename T>
// drogon::Task<> updateCache(
//     const std::string &key,
//     T &&value,
//     const drogon::nosql::RedisClientPtr &client) noexcept(false)
// {
//     std::string vstr = toString(std::forward<T>(value));
//     co_await client->execCommandCoro("set %s %s", key.data(), vstr.data());
// }