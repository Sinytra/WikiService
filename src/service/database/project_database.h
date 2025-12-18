#pragma once

#include <models/Item.h>
#include <models/ProjectVersion.h>
#include <models/RecipeType.h>
#include <service/project/resolved.h>
#include "database.h"
#include "database_base.h"

using namespace drogon_model::postgres;

namespace service {
    struct ProjectContent {
        std::string projectId;
        std::string loc;
        std::string path;
    };
    struct ProjectTag {
        int64_t id;
        std::string loc;
    };

    class ProjectDatabaseAccess : public DatabaseBase {
    public:
        explicit ProjectDatabaseAccess(const ProjectBase &);

        template<typename Ret, typename... Args>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQuery(const std::string query, const std::string searchQuery, const int page,
                                                              const std::function<Ret(const drogon::orm::Row &)> callback = DEFAULT_ROW_CB,
                                                              Args &&...args) const {
            const auto res = co_await handleDatabaseOperation(
                [&, ... args = std::forward<Args>(args)](const drogon::orm::DbClientPtr &client) -> drogon::Task<PaginatedData<Ret>> {
                    constexpr int size = 20;
                    const auto actualQuery = paginatedQuery(query, size, page);

                    const auto results = co_await client->execSqlCoro(actualQuery, versionId_, searchQuery, std::forward<Args>(args)...);

                    if (results.empty()) {
                        throw drogon::orm::DrogonDbException{};
                    }

                    const int totalRows = results[0]["total_rows"].template as<int>();
                    const int totalPages = results[0]["total_pages"].template as<int>();

                    std::vector<Ret> contents;
                    for (const auto &row: results) {
                        contents.emplace_back(callback(row));
                    }

                    co_return PaginatedData<Ret>{.total = totalRows, .pages = totalPages, .size = size, .data = contents};
                });
            co_return res.value_or(PaginatedData<Ret>{});
        }

        drogon::orm::DbClientPtr getDbClientPtr() const override;

        void setDBClientPointer(const drogon::orm::DbClientPtr &client);

        // Versions
        drogon::Task<std::vector<ProjectVersion>> getVersions() const;
        drogon::Task<PaginatedData<ProjectVersion>> getVersionsDev(std::string query, int page) const;
        drogon::Task<TaskResult<>> deleteUnusedVersions(std::vector<std::string> keep) const;

        // Project content registration
        drogon::Task<TaskResult<>> addProjectItem(std::string item) const;
        drogon::Task<TaskResult<>> addTag(std::string tag) const;
        drogon::Task<std::vector<Item>> getProjectTagItemsFlat(int64_t tag) const;
        drogon::Task<TaskResult<>> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<TaskResult<>> addTagTagEntry(std::string parentTag, std::string childTag) const;
        drogon::Task<TaskResult<>> addProjectContentPage(std::string id, std::string path) const;
        drogon::Task<size_t> addRecipeWorkbenches(std::string recipeType, std::vector<std::string> items) const;

        // Dev tables
        drogon::Task<PaginatedData<ProjectContent>> getProjectItemsDev(std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<ProjectContent>> getProjectTagItemsDev(std::string tag, std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<ProjectTag>> getProjectTagsDev(std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<Recipe>> getProjectRecipesDev(std::string searchQuery, int page) const;
        drogon::Task<int> getProjectContentCount() const;
        drogon::Task<TaskResult<std::string>> getProjectContentPath(std::string id) const;

        // Recipes
        drogon::Task<TaskResult<Recipe>> getProjectRecipe(std::string recipe) const;
        drogon::Task<TaskResult<RecipeType>> getRecipeType(std::string type) const;

        // Recipe item usage
        drogon::Task<std::vector<std::string>> getRecipesForItem(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getObtainableItemsBy(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getRecipeTypeWorkbenches(int64_t id) const;

    private:
        const ProjectBase &project_;
        const std::string projectId_;
        const int64_t versionId_;

        drogon::orm::DbClientPtr clientPtr_;
    };
}
