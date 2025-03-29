#pragma once

#include <database.h>
#include <database_base.h>
#include <models/Item.h>
#include <models/ProjectVersion.h>
#include <resolved.h>

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

        drogon::orm::DbClientPtr getDbClientPtr() const override;

        void setDBClientPointer(const drogon::orm::DbClientPtr &client);

        drogon::Task<std::vector<ProjectVersion>> getVersions() const;
        drogon::Task<PaginatedData<ProjectVersion>> getVersionsDev(std::string query, int page) const;

        drogon::Task<Error> addProjectItem(std::string item) const;
        drogon::Task<Error> addTag(std::string tag) const;
        drogon::Task<std::vector<Item>> getTagItemsFlat(int64_t tag) const;
        drogon::Task<std::vector<Item>> getProjectTagItemsFlat(int64_t tag) const;
        drogon::Task<Error> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<Error> addTagTagEntry(std::string parentTag, std::string childTag) const;

        drogon::Task<PaginatedData<ProjectContent>> getProjectItemsDev(std::string query, int page) const;
        drogon::Task<PaginatedData<ProjectTag>> getProjectTagsDev(std::string query, int page) const;
        drogon::Task<std::vector<ProjectContent>> getProjectItemPages() const;
        drogon::Task<int> getProjectContentCount() const;
        drogon::Task<std::optional<std::string>> getProjectContentPath(std::string id) const;
        drogon::Task<Error> addProjectContentPage(std::string id, std::string path) const;

        drogon::Task<std::optional<Recipe>> getProjectRecipe(std::string recipe) const;

        drogon::Task<Error> refreshFlatTagItemView() const;
    private:
        const ResolvedProject &project_;
        const std::string projectId_;
        const int64_t versionId_;

        drogon::orm::DbClientPtr clientPtr_;
    };
}
