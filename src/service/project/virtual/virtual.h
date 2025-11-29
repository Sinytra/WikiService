#pragma once

#include <models/Item.h>
#include <service/database/project_database.h>
#include <service/project/project.h>

#define VIRTUAL_PROJECT_ID "minecraft"

namespace service {
    class VirtualProject final : public ProjectBase {
    public:
        explicit VirtualProject(const Project &, const ProjectVersion &);

        // Basic info
        std::string getId() const override;
        const Project &getProject() const override;
        const ProjectVersion &getProjectVersion() const override;
        ProjectDatabaseAccess &getProjectDatabase() const override;

        // Parameters
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

        // TODO TaskResult
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

        // Serialization
        drogon::Task<Json::Value> toJson(bool full = false) const override;
        drogon::Task<Json::Value> toJsonVerbose() override;
    private:
        Project project_;
        ProjectVersion version_;
        std::shared_ptr<ProjectDatabaseAccess> projectDb_;
    };

    drogon::Task<std::shared_ptr<VirtualProject>> createVirtualProject();
}

namespace global {
    extern std::shared_ptr<service::VirtualProject> virtualProject;
}