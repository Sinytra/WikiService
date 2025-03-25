#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <models/Recipe.h>
#include <models/User.h>
#include <nlohmann/json.hpp>
#include <optional>
#include "database_base.h"
#include "error.h"

#include <models/ProjectVersion.h>

using namespace drogon_model::postgres;

namespace service {
    template<class T>
    struct PaginatedData {
        int pages;
        int total;
        int size;
        std::vector<T> data;

        friend void to_json(nlohmann::json &j, const PaginatedData &obj) {
            j = nlohmann::json{{"total", obj.total}, {"pages", obj.pages}, {"size", obj.size}, {"data", obj.data}};
        }
    };
    struct ProjectSearchResponse {
        int pages;
        int total;
        std::vector<Project> data;
    };
    struct ContentUsage {
        int64_t id;
        std::string loc;
        std::string project;
        std::string path;
    };

    class Database : public DatabaseBase {
    public:
        drogon::Task<std::vector<std::string>> getProjectIDs() const;
        drogon::Task<std::tuple<std::optional<Project>, Error>> createProject(const Project &project) const;
        drogon::Task<Error> updateProject(const Project &project) const;
        drogon::Task<Error> removeProject(const std::string &id) const;
        drogon::Task<std::tuple<std::optional<ProjectVersion>, Error>> createProjectVersion(ProjectVersion version) const;
        drogon::Task<std::optional<ProjectVersion>> getProjectVersion(std::string project, std::string name) const;
        drogon::Task<std::optional<ProjectVersion>> getDefaultProjectVersion(std::string project) const;
        drogon::Task<Error> deleteProjectVersions(std::string project) const;

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

        drogon::Task<Error> addTag(std::string tag) const;
        drogon::Task<Error> refreshFlatTagItemView() const;
        drogon::Task<std::vector<std::string>> getItemSourceProjects(int64_t item) const;

        drogon::Task<std::optional<Recipe>> getProjectRecipe(int64_t version, std::string recipe) const;
        drogon::Task<std::vector<Recipe>> getItemUsageInRecipes(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getObtainableItemsBy(std::string item) const;
    };
}

namespace global {
    extern std::shared_ptr<service::Database> database;
}
