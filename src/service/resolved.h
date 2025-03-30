#pragma once

#include <models/Item.h>
#include <models/Project.h>
#include <models/ProjectVersion.h>
#include <models/RecipeIngredientItem.h>
#include <models/RecipeIngredientTag.h>
#include <nlohmann/json_fwd.hpp>
#include "cache.h"
#include "database.h"
#include "error.h"
#include "util.h"

using namespace drogon_model::postgres;

namespace service {
    struct ProjectPage {
        std::string content;
        std::string editUrl;
    };

    struct ProjectMetadata {
        Json::Value platforms;
    };

    enum class ProjectError { OK, REQUIRES_AUTH, NO_REPOSITORY, REPO_TOO_LARGE, NO_BRANCH, NO_PATH, INVALID_META, UNKNOWN };
    std::string projectErrorToString(ProjectError status);

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
                {"id", obj.id},
                {"name", obj.name},
                {"path", obj.path.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.path)}
            };
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

    struct GitRevision {
        std::string hash;
        std::string fullHash;
        std::string message;
        std::string authorName;
        std::string authorEmail;
        std::string date;

        friend void to_json(nlohmann::json &j, const GitRevision &obj) {
            j = nlohmann::json{
                {"hash", obj.hash},
                {"message", obj.message},
                {"authorName", obj.authorName},
                {"authorEmail", obj.authorEmail},
                {"date", obj.date}
            };
        }
    };

    // See resolved_db.h
    class ProjectDatabaseAccess;

    class ResolvedProject {
    public:
        explicit ResolvedProject(const Project &, const GitRevision &, const std::filesystem::path &, const std::filesystem::path &, const ProjectVersion &);

        void setDefaultVersion(const ResolvedProject &defaultVersion);

        bool setLocale(const std::optional<std::string> &locale);

        bool hasLocale(const std::string &locale) const;
        std::set<std::string> getLocales() const;

        drogon::Task<std::unordered_map<std::string, std::string>> getAvailableVersions() const;
        drogon::Task<bool> hasVersion(std::string version) const;

        std::tuple<ProjectPage, Error> readFile(std::string path) const;
        drogon::Task<std::tuple<ProjectPage, Error>> readContentPage(std::string id) const;
        std::optional<std::string> readPageAttribute(std::string path, std::string prop) const;
        std::optional<std::string> readLangKey(const std::string &locale, const std::string &key) const;

        std::tuple<nlohmann::ordered_json, Error> getDirectoryTree() const;
        std::tuple<nlohmann::ordered_json, Error> getContentDirectoryTree() const;
        drogon::Task<std::tuple<std::optional<nlohmann::ordered_json>, Error>> getProjectContents() const;

        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const;
        drogon::Task<std::optional<Json::Value>> getRecipe(std::string id) const;

        std::tuple<std::optional<nlohmann::json>, ProjectError, std::string> validateProjectMetadata() const;

        drogon::Task<PaginatedData<FullItemData>> getItems(TableQueryParams params) const;
        drogon::Task<PaginatedData<FullTagData>> getTags(TableQueryParams params) const;
        drogon::Task<PaginatedData<FullItemData>> getTagItems(std::string tag, TableQueryParams params) const;
        drogon::Task<PaginatedData<ProjectVersion>> getVersions(TableQueryParams params) const;

        const Project &getProject() const;

        const ProjectVersion &getProjectVersion() const;

        const std::filesystem::path &getDocsDirectoryPath() const;

        ProjectDatabaseAccess &getProjectDatabase() const;

        drogon::Task<Json::Value> toJson(bool full = false) const;
        drogon::Task<Json::Value> toJsonVerbose() const;

        // Content
        drogon::Task<ItemData> getItemName(Item item) const;
        drogon::Task<ItemData> getItemName(std::string loc) const;

    private:
        drogon::Task<Json::Value> ingredientToJson(int slot, std::vector<RecipeIngredientItem> ingredients) const;
        drogon::Task<Json::Value> ingredientToJson(const RecipeIngredientTag &tag) const;

        Project project_;
        GitRevision revision_;
        std::shared_ptr<ResolvedProject> defaultVersion_;

        std::filesystem::path rootDir_;
        std::filesystem::path docsDir_;

        std::string locale_;
        ProjectVersion version_;

        std::shared_ptr<ProjectDatabaseAccess> projectDb_;
    };
}
