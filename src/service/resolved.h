#pragma once

#include "cache.h"
#include "error.h"
#include "models/Project.h"
#include "util.h"

#include <nlohmann/json.hpp>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectPage {
        std::string content;
        std::string editUrl;
        std::string updatedAt;
    };

    struct ProjectMetadata {
        Json::Value platforms;
    };

    enum class ProjectError {
        OK,
        REQUIRES_AUTH,
        NO_REPOSITORY,
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

        std::tuple<nlohmann::ordered_json, Error> getDirectoryTree() const;

        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const;

        std::tuple<std::optional<nlohmann::json>, ProjectError> validateProjectMetadata() const;

        const Project &getProject() const;

        Json::Value toJson() const;
    private:
        Project project_;
        std::shared_ptr<ResolvedProject> defaultVersion_;

        std::filesystem::path rootDir_;
        std::filesystem::path docsDir_;

        std::string locale_;
    };
}