#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <models/Recipe.h>
#include <models/User.h>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include "error.h"

#include <models/Item.h>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectSearchResponse {
        int pages;
        int total;
        std::vector<Project> data;
    };
    struct ProjectContent {
        std::string id;
        std::string path;
    };
    struct ContentUsage {
        int64_t id;
        std::string loc;
        std::string project;
        std::string path;
    };

    class Database {
        template<typename Ret>
        drogon::Task<std::tuple<std::optional<Ret>, Error>>
        handleDatabaseOperation(const std::function<drogon::Task<Ret>(const drogon::orm::DbClientPtr &client)> &func) const {
            try {
                const auto clientPtr = drogon::app().getFastDbClient();
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
        drogon::Task<std::vector<std::string>> getProjectIDs() const;
        drogon::Task<std::tuple<std::optional<Project>, Error>> createProject(const Project &project) const;
        drogon::Task<Error> updateProject(const Project &project) const;
        drogon::Task<Error> removeProject(const std::string &id) const;

        drogon::Task<std::tuple<std::optional<Project>, Error>> getProjectSource(std::string id) const;

        drogon::Task<std::tuple<ProjectSearchResponse, Error>> findProjects(std::string query, std::string types, std::string sort,
                                                                            int page) const;

        drogon::Task<bool> existsForRepo(std::string repo, std::string branch, std::string path) const;

        drogon::Task<bool> existsForData(std::string id, nlohmann::json platforms) const;

        drogon::Task<Error> createUserIfNotExists(std::string username) const;
        drogon::Task<Error> deleteUserProjects(std::string username) const;
        drogon::Task<Error> deleteUser(std::string username) const;

        drogon::Task<Error> linkUserModrinthAccount(std::string username, std::string mrAccountId) const;
        drogon::Task<Error> unlinkUserModrinthAccount(std::string username) const;
        drogon::Task<std::optional<User>> getUser(std::string username) const;
        drogon::Task<std::vector<Project>> getUserProjects(std::string username) const;
        drogon::Task<std::optional<Project>> getUserProject(std::string username, std::string id) const;
        drogon::Task<Error> assignUserProject(std::string username, std::string id, std::string role) const;

        // TODO Project-bound db object
        drogon::Task<Error> addProjectItem(std::string project, std::string item) const;
        drogon::Task<Error> addTag(std::string tag, std::optional<std::string> project) const;
        drogon::Task<std::vector<Item>> getTagItemsFlat(int64_t tag, std::string project) const;
        drogon::Task<Error> addTagItemEntry(std::string project, std::string tag, std::string item) const;
        drogon::Task<Error> addTagTagEntry(std::string project, std::string parentTag, std::string childTag) const;
        drogon::Task<Error> refreshFlatTagItemView() const;

        drogon::Task<std::vector<ProjectContent>> getProjectContents(std::string project) const;
        drogon::Task<int> getProjectContentCount(std::string project) const;
        drogon::Task<std::optional<std::string>> getProjectContentPath(std::string project, std::string id) const;
        drogon::Task<Error> addProjectContentPage(std::string project, std::string id, std::string path) const;

        drogon::Task<std::optional<Recipe>> getProjectRecipe(std::string project, std::string recipe) const;
        drogon::Task<std::vector<Recipe>> getItemUsageInRecipes(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getObtainableItemsBy(std::string item) const;

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

namespace global {
    extern std::shared_ptr<service::Database> database;
}
