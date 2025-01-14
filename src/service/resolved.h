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

    class ResolvedProject {
    public:
        explicit ResolvedProject(const Project &, const std::filesystem::path &, const std::filesystem::path &);

        bool setLocale(const std::optional<std::string> &locale);

        bool hasLocale(const std::string &locale) const;
        std::set<std::string> getLocales() const;

        std::optional<std::unordered_map<std::string, std::string>> getAvailableVersionsFiltered() const;
        std::unordered_map<std::string, std::string> getAvailableVersions() const;

        std::tuple<ProjectPage, Error> readFile(std::string path) const;

        std::tuple<nlohmann::ordered_json, Error> getDirectoryTree() const;

        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const;

        const Project &getProject() const;

        Json::Value toJson(bool isPublic) const;
    private:
        Project project_;

        std::filesystem::path rootDir_;
        std::filesystem::path docsDir_;

        std::string locale_;
    };
}