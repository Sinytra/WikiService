#pragma once

#include <drogon/orm/CoroMapper.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <nlohmann/json.hpp>
#include <service/error.h>
#include <service/util.h>

#define DEFAULT_ROW_CB [](const drogon::orm::Row &row) { return Ret(row); }

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
    public:
        virtual drogon::orm::DbClientPtr getDbClientPtr() const;

        template<typename F, typename Ret = WrapperInnerType_T<std::invoke_result_t<F, const drogon::orm::DbClientPtr &>>,
                 typename Type = std::conditional_t<std::is_void_v<Ret>, std::monostate, Ret>>
        drogon::Task<TaskResult<Type>> handleDatabaseOperation(F &&func) const {
            try {
                const auto clientPtr = getDbClientPtr();
                if (!clientPtr) {
                    co_return Error::ErrInternal;
                }

                if constexpr (std::is_void_v<Ret>) {
                    co_await func(clientPtr);
                    co_return Error::Ok;
                } else {
                    Ret result = co_await func(clientPtr);
                    co_return result;
                }
            } catch (const drogon::orm::Failure &e) {
                logging::logger.error("Error querying database: {}", e.what());
                co_return Error::ErrInternal;
            } catch ([[maybe_unused]] const drogon::orm::DrogonDbException &e) {
                co_return Error::ErrNotFound;
            }
        }

        template<typename Ret, typename... Arguments>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQueryWithArgs(const std::string query, const int page, Arguments &&...args) const {
            co_return co_await handlePaginatedQuery<Ret, Arguments...>(query, page, DEFAULT_ROW_CB, std::forward<Arguments>(args)...);
        }

        template<typename Ret, typename... Arguments>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQuery(const std::string query, const int page,
                                                              const std::function<Ret(const drogon::orm::Row &)> callback = DEFAULT_ROW_CB,
                                                              Arguments &&...args) const {
            const auto res =
                co_await handleDatabaseOperation([&](const drogon::orm::DbClientPtr &client) -> drogon::Task<PaginatedData<Ret>> {
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
            co_return res.value_or({});
        }

        template<typename T>
        drogon::Task<TaskResult<T>> addModel(T entity) const {
            co_return co_await handleDatabaseOperation([&entity](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.insert(entity);
            });
        }

        template<typename T>
        drogon::Task<TaskResult<T>> updateModel(const T entity) const {
            co_return co_await handleDatabaseOperation([&entity](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                if (const auto rows = co_await mapper.update(entity); rows < 1) {
                    throw drogon::orm::DrogonDbException();
                }
                co_return entity;
            });
        }

        template<typename T>
        drogon::Task<TaskResult<T>> findByPrimaryKey(drogon::orm::Mapper<T>::TraitsPKType id) const {
            co_return co_await handleDatabaseOperation([id](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findByPrimaryKey(id);
            });
        }

        template<typename T>
        drogon::Task<TaskResult<T>> findOne(drogon::orm::Criteria criteria) const {
            co_return co_await handleDatabaseOperation([criteria](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findOne(criteria);
            });
        }

        template<typename T>
        drogon::Task<std::vector<T>> findBy(drogon::orm::Criteria criteria) const {
            const auto result = co_await handleDatabaseOperation([criteria](const drogon::orm::DbClientPtr &client) -> drogon::Task<std::vector<T>> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findBy(criteria);
            });
            co_return result.value_or({});
        }

        template<typename T>
        drogon::Task<size_t> getTotalModelCount() const {
            const auto res = co_await handleDatabaseOperation([](const drogon::orm::DbClientPtr &client) -> drogon::Task<size_t> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.count();
            });
            co_return res.value_or(-1);
        }

        template<typename T, typename U>
        drogon::Task<std::vector<T>> getRelated(const std::string col, const U id) const {
            const auto res =
                co_await handleDatabaseOperation([col, id](const drogon::orm::DbClientPtr &client) -> drogon::Task<std::vector<T>> {
                    drogon::orm::CoroMapper<T> mapper(client);
                    const auto results = co_await mapper.findBy(drogon::orm::Criteria(col, drogon::orm::CompareOperator::EQ, id));
                    co_return results;
                });
            co_return res.value_or({});
        }

        template<typename T>
        drogon::Task<T> getByPrimaryKey(int64_t id) const {
            const auto res = co_await handleDatabaseOperation([id](const drogon::orm::DbClientPtr &client) -> drogon::Task<T> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_return co_await mapper.findByPrimaryKey(id);
            });
            if (!res) {
                const auto name = typeid(T).name();
                throw std::runtime_error(std::format("Failed to get {} with id {}", name, std::to_string(id)));
            }
            co_return *res;
        }

        template<typename T>
        drogon::Task<TaskResult<>> deleteByPrimaryKey(T::PrimaryKeyType id) const {
            co_return co_await handleDatabaseOperation([id](const drogon::orm::DbClientPtr &client) -> drogon::Task<> {
                drogon::orm::CoroMapper<T> mapper(client);
                co_await mapper.deleteByPrimaryKey(id);
            });
        }
    };
}
