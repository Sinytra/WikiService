#pragma once

#include "cache.h"
#include "error.h"
#include <models/Project.h>
#include <models/RecipeIngredientItem.h>
#include <models/RecipeIngredientTag.h>
#include "util.h"

#include <nlohmann/json_fwd.hpp>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectPage {
        std::string content;
        std::string editUrl;
    };

    struct ProjectMetadata {
        Json::Value platforms;
    };

    enum class ProjectError {
        OK,
        REQUIRES_AUTH,
        NO_REPOSITORY,
        REPO_TOO_LARGE,
        NO_BRANCH,
        NO_PATH,
        INVALID_META,
        UNKNOWN
    };
    std::string projectErrorToString(ProjectError status);

    class ResolvedProject {
    public:
        explicit ResolvedProject(const Project &, const std::filesystem::path &, const std::filesystem::path &);

        void setDefaultVersion(const ResolvedProject &defaultVersion);

        bool setLocale(const std::optional<std::string> &locale);

        bool hasLocale(const std::string &locale) const;
        std::set<std::string> getLocales() const;

        std::optional<std::unordered_map<std::string, std::string>> getAvailableVersionsFiltered() const;
        std::unordered_map<std::string, std::string> getAvailableVersions() const;

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

        const Project &getProject() const;

        const std::filesystem::path &getDocsDirectoryPath() const;

        Json::Value toJson(bool full = false) const;
        drogon::Task<Json::Value> toJsonVerbose() const;

        // Content
        drogon::Task<std::optional<std::string>> getItemName(std::string id) const;
    private:
        drogon::Task<Json::Value> ingredientToJson(int slot, std::vector<RecipeIngredientItem> items) const;
        drogon::Task<Json::Value> ingredientToJson(const RecipeIngredientTag &tag) const;

        Project project_;
        std::shared_ptr<ResolvedProject> defaultVersion_;

        std::filesystem::path rootDir_;
        std::filesystem::path docsDir_;

        std::string locale_;
    };
}