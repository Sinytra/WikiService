#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <drogon/orm/CoroMapper.h>
#include <log/log.h>
#include "error.h"

namespace service {
    class DatabaseBase {
    protected:
        virtual drogon::orm::DbClientPtr getDbClientPtr() const;

        template<typename Ret>
        drogon::Task<std::tuple<std::optional<Ret>, Error>>
        handleDatabaseOperation(const std::function<drogon::Task<Ret>(const drogon::orm::DbClientPtr &client)> &func) const {
            try {
                const auto clientPtr = getDbClientPtr();
                if (!clientPtr) {
                    co_return {std::nullopt, Error::ErrInternal};
                }

                const Ret result = co_await func(clientPtr);
                co_return {result, Error::Ok};
            } catch (const drogon::orm::Failure &e) {
                logging::logger.error("Error querying database: {}", e.what());
                co_return {std::nullopt, Error::ErrInternal};
            } catch ([[maybe_unused]] const drogon::orm::DrogonDbException &e) {
                co_return {std::nullopt, Error::ErrNotFound};
            }
        }

    public:
        template<typename T, typename U>
        drogon::Task<std::vector<T>> getRelated(const std::string col, const U id) const {
            const auto [res, err] = co_await handleDatabaseOperation<std::vector<T>>(
                [col, id](const drogon::orm::DbClientPtr &client) -> drogon::Task<std::vector<T>> {
                    drogon::orm::CoroMapper<T> mapper(client);
                    const auto results = co_await mapper.findBy(drogon::orm::Criteria(col, drogon::orm::CompareOperator::EQ, id));
                    co_return results;
                });
            co_return res.value_or(std::vector<T>{});
        }

        template<typename T>
        drogon::Task<T> getByPrimaryKey(int64_t id) const {
            const auto [res, err] = co_await handleDatabaseOperation<T>([id](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findByPrimaryKey(id);
            });
            if (!res) {
                const auto name = typeid(T).name();
                throw std::runtime_error(std::format("Failed to get {} with id {}", name, std::to_string(id)));
            }
            co_return *res;
        }
    };
}