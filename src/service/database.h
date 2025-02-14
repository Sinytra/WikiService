#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <models/Recipe.h>
#include <models/User.h>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include "error.h"

using namespace drogon_model::postgres;

namespace service {
    struct ProjectSearchResponse {
        int pages;
        int total;
        std::vector<Project> data;
    };
    struct TagItem {
        std::string itemId;
    };
    struct ProjectContent {
        std::string id;
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
        explicit Database();

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

        drogon::Task<std::vector<ProjectContent>> getProjectContents(std::string project) const;
        drogon::Task<int> getProjectContentCount(std::string project) const;
        drogon::Task<std::optional<std::string>> getProjectContentPath(std::string project, std::string id) const;
        drogon::Task<std::optional<Recipe>> getProjectRecipe(std::string project, std::string recipe) const;
        drogon::Task<std::vector<Recipe>> getItemUsageInRecipes(std::string item) const;
        drogon::Task<std::vector<TagItem>> getTagItemsFlat(std::string tag) const;

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
    };
}

namespace global {
    extern std::shared_ptr<service::Database> database;
}
