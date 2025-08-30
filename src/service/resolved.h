#pragma once

#include <models/Item.h>
#include <models/Project.h>
#include <models/ProjectVersion.h>
#include <nlohmann/json_fwd.hpp>
#include "cache.h"
#include "content/recipe/game_recipes.h"
#include "database/database.h"
#include "error.h"
#include "project/format.h"
#include "util.h"

using namespace drogon_model::postgres;

namespace service {
    enum class FileType {
        DIR,
        FILE
    };
    std::string enumToStr(FileType type);

    void to_json(nlohmann::json &j, const FileType &obj);

    struct FileTreeEntry {
        std::string id;
        std::string name;
        std::string icon;
        std::string path;
        FileType type;
        std::vector<FileTreeEntry> children;

        friend void to_json(nlohmann::json &j, const FileTreeEntry &obj) {
            j = {
                {"id", obj.id.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.id)},
                {"name", obj.name},
                {"icon", obj.icon.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.icon)},
                {"path", obj.path},
                {"type", obj.type},
                {"children", obj.children}
            };
        }
    };

    typedef std::vector<FileTreeEntry> FileTree;

    struct FolderMetadataEntry {
        std::string name;
        std::string icon;
    };

    struct FolderMetadata {
        std::vector<std::string> keys;
        std::map<std::string, FolderMetadataEntry> entries;
    };

    struct ProjectPage {
        std::string content;
        std::string editUrl;
    };

    struct ProjectMetadata {
        Json::Value platforms;
    };

    struct ProjectErrorInstance {
        ProjectError error;
        std::string message;
    };

    struct ItemData {
        std::string name;
        std::string path;
    };

    struct FullItemData {
        std::string id;
        std::string name;
        std::string path;

        friend void to_json(nlohmann::json &j, const FullItemData &obj) {
            j = nlohmann::json{
                {"id", obj.id}, {"name", obj.name}, {"path", obj.path.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.path)}};
        }
    };

    struct FullTagData {
        std::string id;
        std::vector<std::string> items;

        friend void to_json(nlohmann::json &j, const FullTagData &obj) {
            j = nlohmann::json{
                {"id", obj.id},
                {"items", obj.items},
            };
        }
    };

    struct FullRecipeData {
        std::string id;
        nlohmann::json data;

        friend void to_json(nlohmann::json &j, const FullRecipeData &obj) {
            j = nlohmann::json{{"id", obj.id}, {"data", obj.data}};
        }
    };

    // See resolved_db.h
    class ProjectDatabaseAccess;

    std::string formatCommitUrl(const Project &project, const std::string &hash);

    class ResolvedProject {
    public:
        explicit ResolvedProject(const Project &, const std::filesystem::path &, const ProjectVersion &);

        void setDefaultVersion(const ResolvedProject &defaultVersion);
        void setLocale(const std::optional<std::string> &locale);

        std::string getLocale() const;
        bool hasLocale(const std::string &locale) const;
        std::set<std::string> getLocales() const;

        drogon::Task<std::unordered_map<std::string, std::string>> getAvailableVersions() const;
        drogon::Task<bool> hasVersion(std::string version) const;

        std::optional<std::string> getPagePath(const std::string &path) const; // For project issues
        std::optional<std::string> getPageAttribute(const std::string &path, const std::string &prop) const;
        std::optional<std::string> getPageTitle(const std::string &path) const;
        std::tuple<ProjectPage, Error> readPageFile(std::string path) const;
        drogon::Task<std::tuple<ProjectPage, Error>> readContentPage(std::string id) const;
        drogon::Task<nlohmann::json> readItemProperties(std::string id) const;
        std::optional<std::string> readLangKey(const std::string &namespace_, const std::string &key) const;

        std::tuple<FileTree, Error> getDirectoryTree() const;
        std::tuple<FileTree, Error> getContentDirectoryTree() const;
        drogon::Task<std::tuple<std::optional<FileTree>, Error>> getProjectContents() const;

        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const;
        std::optional<content::GameRecipeType> getRecipeType(const ResourceLocation &location) const;
        drogon::Task<std::optional<content::ResolvedGameRecipe>> getRecipe(std::string id) const;

        std::tuple<std::optional<nlohmann::json>, ProjectError, std::string> validateProjectMetadata() const;

        drogon::Task<PaginatedData<FullItemData>> getItems(TableQueryParams params) const;
        drogon::Task<PaginatedData<FullTagData>> getTags(TableQueryParams params) const;
        drogon::Task<PaginatedData<FullItemData>> getTagItems(std::string tag, TableQueryParams params) const;
        drogon::Task<PaginatedData<FullRecipeData>> getRecipes(TableQueryParams params) const;
        drogon::Task<PaginatedData<ProjectVersion>> getVersions(TableQueryParams params) const;

        const Project &getProject() const;

        const ProjectVersion &getProjectVersion() const;

        const std::filesystem::path &getRootDirectory() const;

        const ProjectFormat &getFormat() const;

        ProjectDatabaseAccess &getProjectDatabase() const;

        drogon::Task<Json::Value> toJson(bool full = false) const;
        drogon::Task<Json::Value> toJsonVerbose() const;

        // Content
        drogon::Task<ItemData> getItemName(Item item) const;
        drogon::Task<ItemData> getItemName(std::string loc) const;

    private:
        FolderMetadata getFolderMetadata(const std::filesystem::path &path) const;
        FileTree getDirectoryTree(const std::filesystem::path &dir) const;

        Project project_;
        std::shared_ptr<ResolvedProject> defaultVersion_;
        ProjectFormat format_;

        std::filesystem::path docsDir_;

        ProjectVersion version_;

        std::shared_ptr<ProjectDatabaseAccess> projectDb_;
    };
}
