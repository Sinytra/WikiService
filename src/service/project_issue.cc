#include "project_issue.h"

#include "database/database.h"
#include "models/ProjectIssue.h"

using namespace drogon;

namespace service {
    // clang-format off
    ENUM_TO_STR(ProjectIssueLevel,
        {ProjectIssueLevel::WARNING, "warning"},
        {ProjectIssueLevel::ERROR, "error"}
    )

    ENUM_TO_STR(ProjectIssueType,
        {ProjectIssueType::GIT_CLONE, "git_clone"},
        {ProjectIssueType::GIT_INFO, "git_info"},
        {ProjectIssueType::INGESTOR, "ingestor"},
        {ProjectIssueType::PAGE_RENDER, "page_render"},
        {ProjectIssueType::INTERNAL, "internal"}
    )
    // clang-format on

    ProjectIssueCallback::ProjectIssueCallback(const std::string &id, const std::shared_ptr<spdlog::logger> &log) :
        deploymentId_(id), logger_(log), hasErrors_(false) {}

    Task<> ProjectIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                          const std::string &details, const std::string &file) {
        co_await service::addIssue(deploymentId_, level, type, subject, details, file);

        const auto logLevel = level == ProjectIssueLevel::ERROR ? spdlog::level::err : spdlog::level::warn;
        const auto logDetail = details.empty() ? "" : " '" + details + "' ";
        const auto logFile = file.empty() ? "" : " in file " + file;
        logger_->log(logLevel, "[Issue] {} / {}{}{}", enumToStr(type), enumToStr(subject), logDetail, logFile);

        if (level == ProjectIssueLevel::ERROR)
            hasErrors_ = true;
    }

    void ProjectIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                             const std::string &details, const std::string &file) {
        app().getLoop()->queueInLoop(async_func(
            [this, level, type, subject, details, file]() -> Task<> { co_await addIssue(level, type, subject, details, file); }));
    }

    bool ProjectIssueCallback::hasErrors() const { return hasErrors_; }

    ProjectFileIssueCallback::ProjectFileIssueCallback(ProjectIssueCallback &issues, const std::string &file) :
        issues_(issues), file_(file) {}

    Task<> ProjectFileIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                              const std::string &details) const {
        co_await issues_.addIssue(level, type, subject, details, file_);
    }

    void ProjectFileIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                                 const std::string &details) const {
        issues_.addIssueAsync(level, type, subject, details, file_);
    }

    Task<> addIssue(const std::string &deploymentId, const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                    const std::string &details, const std::string &file) {
        ProjectIssue issue;
        issue.setDeploymentId(deploymentId);
        issue.setLevel(enumToStr(level));
        issue.setType(enumToStr(type));
        issue.setSubject(enumToStr(subject));
        issue.setDetails(details);
        issue.setFile(file);

        co_await global::database->addProjectIssue(issue);
    }
}
