#pragma once

#include <database.h>
#include <database_base.h>
#include <models/Item.h>
#include <models/Project.h>
#include <models/ProjectVersion.h>
#include <resolved.h>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectContent {
        std::string id;
        std::string path;
    };

    class ProjectDatabaseAccess : public DatabaseBase {
    public:
        explicit ProjectDatabaseAccess(const ResolvedProject &);

        drogon::Task<std::vector<ProjectVersion>> getVersions() const;

        drogon::Task<Error> addProjectItem(std::string item) const;
        drogon::Task<Error> addTag(std::string tag) const;
        drogon::Task<std::vector<Item>> getTagItemsFlat(int64_t tag) const;
        drogon::Task<Error> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<Error> addTagTagEntry(std::string parentTag, std::string childTag) const;

        drogon::Task<PaginatedData<ProjectContent>> getProjectItemsDev(std::string query, int page) const;
        drogon::Task<std::vector<ProjectContent>> getProjectItemPages() const;
        drogon::Task<int> getProjectContentCount() const;
        drogon::Task<std::optional<std::string>> getProjectContentPath(std::string id) const;
        drogon::Task<Error> addProjectContentPage(std::string id, std::string path) const;

    private:
        const ResolvedProject &project_;
        const std::string projectId_;
    };
}
