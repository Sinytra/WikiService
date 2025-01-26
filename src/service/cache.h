#pragma once

#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThreadPool.h>

#include <any>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>
#include "log/log.h"

namespace service {
    // TODO Per-task pool
    extern trantor::EventLoopThreadPool cacheAwaiterThreadPool;

    template<class T>
    drogon::Task<T> patientlyAwaitTaskResult(const std::shared_future<T> task) {
        using namespace logging;

        const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        co_await drogon::switchThreadCoro(cacheAwaiterThreadPool.getNextLoop());
        const auto result = task.get();
        co_await drogon::switchThreadCoro(currentLoop);
        co_return result;
    }

    class MemoryCache {
    public:
        MemoryCache();

        drogon::Task<bool> exists(std::string key) const;
        drogon::Task<std::optional<std::string>> getFromCache(std::string key) const;

        drogon::Task<bool> isSetMember(std::string key, std::string value) const;
        drogon::Task<std::set<std::string>> getSetMembers(std::string key) const;
        drogon::Task<std::optional<std::string>> getHashMember(std::string key, std::string value) const;

        drogon::Task<> updateCache(std::string key, std::string value, std::chrono::duration<long> expire) const;
        drogon::Task<> updateCacheSet(std::string key, std::vector<std::string> value, std::chrono::duration<long> expire) const;
        drogon::Task<> updateCacheHash(std::string key, std::unordered_map<std::string, std::string> values, std::chrono::duration<long> expire) const;

        drogon::Task<> erase(std::string key) const;
        drogon::Task<> erase(std::vector<std::string> keys) const;
        drogon::Task<> eraseNamespace(std::string key) const;
    };

    class CacheableServiceBase {
    protected:
        template<class T>
        drogon::Task<std::optional<std::shared_future<T>>> startTask(const std::string &key) {
            auto guard = co_await mutex_.scoped_lock();

            if (pendingTasks_.contains(key)) {
                auto pair = std::any_cast<std::pair<std::shared_ptr<std::promise<T>>, std::shared_future<T>> &>(pendingTasks_[key]);
                co_return std::optional{pair.second};
            }

            auto promise = std::make_shared<std::promise<T>>();
            auto future = promise->get_future().share();
            pendingTasks_[key] = std::pair<std::shared_ptr<std::promise<T>>, std::shared_future<T>>(promise, future);

            co_return std::nullopt;
        }

        template<class T>
        std::optional<std::shared_future<T>> getPendingTask(const std::string &key) {
            if (pendingTasks_.contains(key)) {
                auto pair = std::any_cast<std::pair<std::shared_ptr<std::promise<T>>, std::shared_future<T>> &>(pendingTasks_[key]);
                return std::optional{pair.second};
            }
            return std::nullopt;
        }

        template<class T>
        drogon::Task<std::optional<std::shared_future<T>>> getOrStartTask(const std::string &key) {
            if (const auto pending = getPendingTask<T>(key)) {
                co_return pending;
            }
            if (const auto pending = co_await startTask<T>(key)) {
                co_return pending;
            }
            co_return std::nullopt;
        }

        template<class T>
        drogon::Task<T> completeTask(const std::string &key, T &&value) {
            if (!pendingTasks_.contains(key)) {
                throw std::invalid_argument("No task for key " + key);
            }

            auto pair = std::any_cast<std::pair<std::shared_ptr<std::promise<T>>, std::shared_future<T>> &>(pendingTasks_[key]);
            pair.first->set_value(value);

            auto guard = co_await mutex_.scoped_lock();
            pendingTasks_.erase(key);

            co_return value;
        }

    private:
        drogon::Mutex mutex_;
        std::shared_mutex pendingTasks_mtx_;
        std::unordered_map<std::string, std::any> pendingTasks_;
    };
}
