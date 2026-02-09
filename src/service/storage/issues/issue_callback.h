#pragma once

#include <service/project/project.h>

namespace service {
    class ProjectIssueCallback {
    public:
        explicit ProjectIssueCallback(const std::string &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<> addIssue(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, std::string details = "",
                                std::string file = "");

        void addIssueAsync(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, std::string details = "",
                           std::string file = "");

        bool hasErrors() const;

    private:
        const std::string deploymentId_;
        const std::shared_ptr<spdlog::logger> logger_;

        bool hasErrors_;
    };

    class ProjectFileIssueCallback {
    public:
        explicit ProjectFileIssueCallback(const std::shared_ptr<ProjectIssueCallback> &, const std::filesystem::path &);

        explicit ProjectFileIssueCallback(const ProjectFileIssueCallback &, const std::filesystem::path &);

        drogon::Task<> addIssue(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, std::string details = "") const;

        void addIssueAsync(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "") const;

        std::optional<ResourceLocation> validateResourceLocation(const std::string &str) const;

        std::optional<nlohmann::json> readAndValidateJson(const nlohmann::json &schema) const;

    private:
        std::shared_ptr<ProjectIssueCallback> issues_;
        const std::filesystem::path absolutePath_;
        const std::filesystem::path path_;
    };
}
