#pragma once

#include "database.h"
#include "database_base.h"
#include <service/resolved.h>
#include <models/Item.h>
#include <models/ProjectVersion.h>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectContent {
        std::string id;
        std::string path;
    };
    struct ProjectTag {
        int64_t id;
        std::string loc;
    };

    class ProjectDatabaseAccess : public DatabaseBase {
    public:
        explicit ProjectDatabaseAccess(const ResolvedProject &);

        template<typename Ret>
        drogon::Task<PaginatedData<Ret>> handlePaginatedQuery(const std::string query, const std::string searchQuery, const int page,
                                                              const std::function<Ret(const drogon::orm::Row &)> callback =
                                                              [](const drogon::orm::Row &row) { return Ret(row); }) const {
            const auto [res, err] = co_await handleDatabaseOperation<PaginatedData<Ret>>(
                [&](const drogon::orm::DbClientPtr &client) -> drogon::Task<PaginatedData<Ret>> {
                    constexpr int size = 20;
                    const auto actualQuery = paginatedQuery(query, size, page);

                    const auto results = co_await client->execSqlCoro(actualQuery, versionId_, searchQuery);

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

        drogon::orm::DbClientPtr getDbClientPtr() const override;

        void setDBClientPointer(const drogon::orm::DbClientPtr &client);

        drogon::Task<std::vector<ProjectVersion>> getVersions() const;
        drogon::Task<PaginatedData<ProjectVersion>> getVersionsDev(std::string query, int page) const;
        drogon::Task<Error> deleteUnusedVersions(std::vector<std::string> keep) const;

        drogon::Task<Error> addProjectItem(std::string item) const;
        drogon::Task<Error> addTag(std::string tag) const;
        drogon::Task<std::vector<Item>> getTagItemsFlat(int64_t tag) const;
        drogon::Task<std::vector<Item>> getProjectTagItemsFlat(int64_t tag) const;
        drogon::Task<Error> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<Error> addTagTagEntry(std::string parentTag, std::string childTag) const;

        drogon::Task<PaginatedData<ProjectContent>> getProjectItemsDev(std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<ProjectContent>> getProjectTagItemsDev(std::string tag, std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<ProjectTag>> getProjectTagsDev(std::string searchQuery, int page) const;
        drogon::Task<PaginatedData<Recipe>> getProjectRecipesDev(std::string searchQuery, int page) const;
        drogon::Task<std::vector<ProjectContent>> getProjectItemPages() const;
        drogon::Task<int> getProjectContentCount() const;
        drogon::Task<std::optional<std::string>> getProjectContentPath(std::string id) const;
        drogon::Task<Error> addProjectContentPage(std::string id, std::string path) const;
        drogon::Task<size_t> addRecipeWorkbenches(std::string recipeType, std::vector<std::string> items) const;

        drogon::Task<std::optional<Recipe>> getProjectRecipe(std::string recipe) const;
        drogon::Task<std::optional<RecipeType>> getRecipeType(std::string type) const;
        drogon::Task<std::vector<std::string>> getItemRecipes(std::string item) const;

    private:
        const ResolvedProject &project_;
        const std::string projectId_;
        const int64_t versionId_;

        drogon::orm::DbClientPtr clientPtr_;
    };
}
