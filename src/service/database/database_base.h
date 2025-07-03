#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/orm/CoroMapper.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <nlohmann/json.hpp>
#include <service/error.h>

namespace service {
    template<class T>
    struct PaginatedData {
        int total;
        int pages;
        int size;
        std::vector<T> data;

        friend void to_json(nlohmann::json &j, const PaginatedData &obj) {
            j = nlohmann::json{{"total", obj.total}, {"pages", obj.pages}, {"size", obj.size}, {"data", obj.data}};
        }
    };

    std::string paginatedQuery(const std::string &dataQuery, int pageSize, int page);

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

        template<typename Ret, typename... Arguments>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQueryWithArgs(const std::string query, const int page, Arguments &&...args) const {
            co_return co_await handlePaginatedQuery<Ret, Arguments...>(
                query, page, [](const drogon::orm::Row &row) { return Ret(row); }, std::forward<Arguments>(args)...);
        }

        template<typename Ret, typename... Arguments>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQuery(
            const std::string query, const int page,
            const std::function<Ret(const drogon::orm::Row &)> callback = [](const drogon::orm::Row &row) { return Ret(row); },
            Arguments &&...args) const {
            const auto [res, err] = co_await handleDatabaseOperation<PaginatedData<Ret>>(
                [&](const drogon::orm::DbClientPtr &client) -> drogon::Task<PaginatedData<Ret>> {
                    constexpr int size = 20;
                    const auto actualQuery = paginatedQuery(query, size, page);

                    const auto results = co_await client->execSqlCoro(actualQuery, std::forward<Arguments>(args)...);

                    if (results.empty()) {
                        throw drogon::orm::DrogonDbException{};
                    }

                    const int totalRows = results[0]["total_rows"].template as<int>();
                    const int totalPages = results[0]["total_pages"].template as<int>();

                    std::vector<Ret> contents;
                    for (const auto &row: results) {
                        contents.emplace_back(callback(row));
                    }

                    co_return PaginatedData{.total = totalRows, .pages = totalPages, .size = size, .data = contents};
                });
            co_return res.value_or(PaginatedData<Ret>{});
        }

    public:
        template<typename T>
        drogon::Task<std::optional<T>> addModel(T entity) const {
            const auto [res, err] =
                co_await handleDatabaseOperation<T>([&entity](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                    drogon::orm::CoroMapper<T> mapper(client);
                    co_return co_await mapper.insert(entity);
                });
            co_return res;
        }

        template<typename T>
        drogon::Task<std::optional<T>> updateModel(const T deployment) const {
            const auto [res, err] =
                co_await handleDatabaseOperation<T>([&deployment](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                    drogon::orm::CoroMapper<T> mapper(client);
                    if (const auto rows = co_await mapper.update(deployment); rows < 1) {
                        throw drogon::orm::DrogonDbException();
                    }
                    co_return deployment;
                });
            co_return res;
        }

        template<typename T>
        drogon::Task<std::optional<T>> getModel(typename drogon::orm::Mapper<T>::TraitsPKType id) const {
            const auto [res, err] = co_await handleDatabaseOperation<T>([id](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findByPrimaryKey(id);
            });
            co_return res;
        }

        template<typename T>
        drogon::Task<size_t> getTotalModelCount() const {
            const auto [res, err] =
                co_await handleDatabaseOperation<size_t>([](const drogon::orm::DbClientPtr &client) -> drogon::Task<size_t> {
                    drogon::orm::CoroMapper<T> mapper(client);
                    co_return co_await mapper.count();
                });
            co_return res.value_or(-1);
        }

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
