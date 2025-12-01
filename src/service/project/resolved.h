#pragma once

#include <models/Item.h>
#include <models/Project.h>
#include <models/ProjectVersion.h>
#include <nlohmann/json_fwd.hpp>
#include <service/project/project.h>
#include <service/cache.h>
#include <service/content/recipe/game_recipes.h>
#include <service/database/database.h>
#include <service/util.h>
#include <service/project/format.h>

namespace service {
    struct FolderMetadataEntry {
        std::string name;
        std::string icon;
    };

    struct FolderMetadata {
        std::vector<std::string> keys;
        std::map<std::string, FolderMetadataEntry> entries;
    };

    struct ProjectMetadata {
        Json::Value platforms;
    };

    struct ProjectErrorInstance {
        ProjectError error;
        std::string message;
    };

    class ResolvedProject final : public ProjectBase {
    public:
        explicit ResolvedProject(const Project &, const std::filesystem::path &, const ProjectVersion &,
                                 const std::shared_ptr<ProjectIssueCallback> &, const std::shared_ptr<spdlog::logger> &);

        // Basic info
        std::string getId() const override;
        const Project &getProject() const override;
        const ProjectVersion &getProjectVersion() const override;
        ProjectDatabaseAccess &getProjectDatabase() const override;

        // Parameters
        void setDefaultVersion(const ResolvedProject &defaultVersion);
        void setLocale(const std::optional<std::string> &locale);

        std::string getLocale() const override;
        bool hasLocale(const std::string &locale) const override;
        std::set<std::string> getLocales() const override;

        drogon::Task<std::unordered_map<std::string, std::string>> getAvailableVersions() const override;
        drogon::Task<bool> hasVersion(std::string version) const override;

        // Pages
        std::optional<std::string> getPagePath(const std::string &path) const override;
        std::optional<std::string> getPageTitle(const std::string &path) const override;
        TaskResult<ProjectPage> readPageFile(std::string path) const override;
        drogon::Task<TaskResult<ProjectPage>> readContentPage(std::string id) const override;
        std::optional<Frontmatter> readPageAttributes(const std::string &path) const override;

        // Content
        drogon::Task<PaginatedData<ItemContentPage>> getItemContentPages(TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullTagData>> getTags(TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullItemData>> getTagItems(std::string tag, TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullRecipeData>> getRecipes(TableQueryParams params) const override;
        drogon::Task<PaginatedData<ProjectVersion>> getVersions(TableQueryParams params) const override;

        drogon::Task<ItemData> getItemName(Item item) const override;
        drogon::Task<ItemData> getItemName(std::string loc) const override;

        // Files
        drogon::Task<nlohmann::json> readItemProperties(std::string id) const override;
        drogon::Task<std::optional<std::string>> readLangKey(const std::string &namespace_, const std::string &key) const override;

        drogon::Task<TaskResult<FileTree>> getDirectoryTree() override;
        drogon::Task<TaskResult<FileTree>> getProjectContents() override;

        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const override;
        drogon::Task<std::optional<content::GameRecipeType>> getRecipeType(const ResourceLocation &location) override;
        drogon::Task<std::optional<content::ResolvedGameRecipe>> getRecipe(std::string id) override;

        // Validation
        drogon::Task<> validatePages();
        std::tuple<std::optional<nlohmann::json>, ProjectError, std::string> validateProjectMetadata() const;

        // Access
        const std::filesystem::path &getRootDirectory() const;
        const ProjectFormat &getFormat() const;

        // Serialization
        drogon::Task<Json::Value> toJson(bool full = false) const override;
        drogon::Task<Json::Value> toJsonVerbose() override;

    private:
        FolderMetadata getFolderMetadata(const std::filesystem::path &path) const;
        FileTree getDirectoryTree(const std::filesystem::path &dir) const;
        void addPageMetadata(FileTree &tree) const;

        Project project_;
        std::filesystem::path docsDir_;
        ProjectFormat format_;
        std::shared_ptr<ResolvedProject> defaultVersion_;
        ProjectVersion version_;
        std::shared_ptr<ProjectDatabaseAccess> projectDb_;
        std::shared_ptr<ProjectIssueCallback> issues_;
        std::shared_ptr<spdlog::logger> logger_;
    };
}
