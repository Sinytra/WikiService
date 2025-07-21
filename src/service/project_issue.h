#pragma once

#include <models/Deployment.h>
#include <nlohmann/json.hpp>

#include "util.h"

namespace service {
    enum class ProjectIssueLevel { WARNING, ERROR, UNKNOWN };
    std::string enumToStr(ProjectIssueLevel level);
    ProjectIssueLevel parseProjectIssueLevel(const std::string &level);

    enum class ProjectIssueType { META, FILE, GIT_CLONE, GIT_INFO, PAGE_RENDER, INGESTOR, INTERNAL, UNKNOWN };
    std::string enumToStr(ProjectIssueType type);
    ProjectIssueType parseProjectIssueType(const std::string &level);

    // clang-format off
    enum class ProjectError {
        OK,
        REQUIRES_AUTH, NO_REPOSITORY, REPO_TOO_LARGE, NO_BRANCH, NO_PATH,
        INVALID_META, INVALID_PAGE,
        DUPLICATE_PAGE, UNKNOWN_RECIPE_TYPE, INVALID_INGREDIENT,
        INVALID_FILE, INVALID_FORMAT, INVALID_RESLOC,
        MISSING_PLATFORM_PROJECT,
        UNKNOWN
    };
    // clang-format on
    std::string enumToStr(ProjectError status);
    ProjectError parseProjectError(const std::string &str);

    class ProjectIssueCallback {
    public:
        explicit ProjectIssueCallback(const std::string &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<> addIssue(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "",
                                const std::string &file = "");

        void addIssueAsync(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "",
                           const std::string &file = "");

        bool hasErrors() const;
    private:
        const std::string &deploymentId_;
        const std::shared_ptr<spdlog::logger> logger_;

        bool hasErrors_;
    };

    class ProjectFileIssueCallback {
    public:
        explicit ProjectFileIssueCallback(ProjectIssueCallback &, const std::filesystem::path &);

        explicit ProjectFileIssueCallback(const ProjectFileIssueCallback &, const std::filesystem::path &);

        drogon::Task<> addIssue(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "") const;

        void addIssueAsync(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "") const;

        std::optional<ResourceLocation> validateResourceLocation(const std::string &str) const;

        std::optional<nlohmann::json> readAndValidateJson(const nlohmann::json &schema) const;
    private:
        ProjectIssueCallback &issues_;
        const std::filesystem::path absolutePath_;
        const std::filesystem::path path_;
    };

    drogon::Task<> addIssue(const std::string &deploymentId, ProjectIssueLevel level, ProjectIssueType type, ProjectError subject,
                            const std::string &details = "", const std::string &file = "");
}
