#pragma once

#include <models/Deployment.h>
#include "resolved.h"

namespace service {
    enum class ProjectIssueLevel { WARNING, ERROR, UNKNOWN };
    std::string enumToStr(ProjectIssueLevel level);

    enum class ProjectIssueType { GIT_CLONE, GIT_INFO, PAGE_RENDER, INGESTOR, INTERNAL, UNKNOWN };
    std::string enumToStr(ProjectIssueType type);

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
        explicit ProjectFileIssueCallback(ProjectIssueCallback &, const std::string &);

        drogon::Task<> addIssue(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "") const;

        void addIssueAsync(ProjectIssueLevel level, ProjectIssueType type, ProjectError subject, const std::string &details = "") const;
    private:
        ProjectIssueCallback &issues_;
        const std::string &file_;
    };

    drogon::Task<> addIssue(const std::string &deploymentId, ProjectIssueLevel level, ProjectIssueType type, ProjectError subject,
                            const std::string &details = "", const std::string &file = "");
}
