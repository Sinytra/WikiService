#include "issue_service.h"

namespace service {
    inline std::string createProjectIssueKey(const std::string &project, const std::string &level, const std::string &type,
                                             const std::string &subject, const std::string &path) {
        return std::format("project_issue:{}:{}:{}:{}:[{}]", project, level, type, subject, path);
    }

    IssueService::IssueService() {}

    ComputableTask<> IssueService::addProjectIssueExternal(const ProjectBasePtr project, const ProjectIssueLevel level,
                                                           const ProjectIssueType type, const ProjectError subject,
                                                           const std::string details, const std::string path) {
        const auto projectId = project->getId();

        const auto deployment(co_await global::database->getActiveDeployment(projectId));
        if (!deployment) {
            co_return Error::ErrNotFound;
        }

        const auto taskKey = createProjectIssueKey(projectId, enumToStr(level), enumToStr(type), enumToStr(subject), path);

        if (const auto pending = co_await getOrStartTask<Error>(taskKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        if (const auto existing = co_await global::database->getProjectIssue(deployment->getValueOfId(), level, type, path)) {
            co_return co_await completeTask<Error>(taskKey, Error::ErrBadRequest);
        }

        ProjectIssue issue;
        issue.setDeploymentId(deployment->getValueOfId());
        issue.setLevel(enumToStr(level));
        issue.setType(enumToStr(type));
        issue.setSubject(enumToStr(subject));
        issue.setDetails(details);
        if (!path.empty()) {
            issue.setFile(path);
        }
        if (const auto versionName = project->getProjectVersion().getName()) {
            issue.setVersionName(*versionName);
        }

        const auto result = co_await global::database->addModel(issue);

        co_return co_await completeTask<Error>(taskKey, result.error());
    }

    ComputableTask<> IssueService::addProjectIssueInternal(const std::string deploymentId, const ProjectIssueLevel level,
                                                           const ProjectIssueType type, const ProjectError subject,
                                                           const std::string details, const std::string file) const {
        // Skip adding issue if it already exists
        if (const auto existing = co_await global::database->getProjectIssue(deploymentId, level, type, file)) {
            co_return Error::Ok;
        }

        ProjectIssue issue;
        issue.setDeploymentId(deploymentId);
        issue.setLevel(enumToStr(level));
        issue.setType(enumToStr(type));
        issue.setSubject(enumToStr(subject));
        issue.setDetails(details);
        issue.setFile(file);

        co_return co_await global::database->addModel(issue);
    }
}
