#include "project_access.h"

#include <models/UserProject.h>

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;

namespace service {
    ENUM_FROM_TO_STR(ProjectMemberRole, {ProjectMemberRole::OWNER, "owner"}, {ProjectMemberRole::MEMBER, "member"})
    void to_json(nlohmann::json &j, const ProjectMemberRole &obj) { j = enumToStr(obj); }
    void from_json(const nlohmann::json &j, ProjectMemberRole &obj) { obj = parseProjectMemberRole(j); }

    Task<TaskResult<>> assignUserProject(const std::string username, const std::string id, const ProjectMemberRole role) {
        UserProject userProject;
        userProject.setUserId(username);
        userProject.setProjectId(id);
        userProject.setRole(enumToStr(role));

        const auto result = co_await global::database->addModel(userProject);
        co_return result.error();
    }

    Task<ProjectMemberRole> getUserAccessLevel(const Project project, const UserSession actor) {
        if (actor.user.getValueOfRole() == ROLE_ADMIN) {
            co_return ProjectMemberRole::OWNER;
        }
        const auto actorMember = co_await global::database->getProjectMember(project.getValueOfId(), actor.username);
        if (!actorMember) {
            throw ApiException{Error::ErrInternal, "User not found in project members"};
        }
        co_return parseProjectMemberRole(actorMember->getValueOfRole());
    }

    Task<ProjectMembersData> getProjectMembers(const Project project, const UserSession actor) {
        const auto username = actor.username;
        const auto projectId = project.getValueOfId();
        const auto actorMember = co_await global::database->getProjectMember(projectId, actor.username);

        const auto members = co_await global::database->getProjectMembers(projectId);
        std::vector<ProjectMember> results;
        for (const auto &member: members) {
            const auto role = parseProjectMemberRole(member.getValueOfRole());
            const auto isActor = member.getValueOfUserId() == username;
            results.push_back(ProjectMember{member.getValueOfUserId(), role, isActor});
        }
        // Move actor to first place
        std::ranges::stable_partition(results, [&](const ProjectMember &a) { return a.username == username; });

        const auto canEdit = actor.user.getValueOfRole() == ROLE_ADMIN ||
                             actorMember && parseProjectMemberRole(actorMember->getValueOfRole()) == ProjectMemberRole::OWNER;
        const auto canLeave = co_await global::database->canUserLeaveProject(projectId, username);

        co_return {results, canEdit, canLeave};
    }

    Task<TaskResult<>> addProjectMember(const Project project, const UserSession actor, const UserProject member) {
        if (actor.user.getValueOfRole() != ROLE_ADMIN) {
            const auto actorMember(co_await global::database->getProjectMember(project.getValueOfId(), actor.username));
            if (!actorMember || parseProjectMemberRole(actorMember->getValueOfRole()) != ProjectMemberRole::OWNER) {
                // TODO PermissionDenied exception
                throw ApiException{Error::ErrForbidden, "insufficient_permissions"};
            }
        }

        if (const auto projectMember = co_await global::database->getUserProject(member.getValueOfUserId(), member.getValueOfProjectId())) {
            throw ApiException{Error::ErrBadRequest, "duplicate_member"};
        }

        if (const auto user(co_await global::database->findByPrimaryKey<User>(member.getValueOfUserId())); !user) {
            throw ApiException{Error::ErrNotFound, "user_not_found"};
        }

        const auto result = co_await global::database->addModel(member);
        co_return result.error();
    }

    Task<TaskResult<>> removeProjectMember(const Project project, const UserSession actor, const UserProject member) {
        if (actor.user.getValueOfRole() != ROLE_ADMIN && actor.username != member.getValueOfUserId()) {
            const auto actorMember(co_await global::database->getProjectMember(project.getValueOfId(), actor.username));
            if (!actorMember || parseProjectMemberRole(actorMember->getValueOfRole()) != ProjectMemberRole::OWNER) {
                throw ApiException{Error::ErrForbidden, "insufficient_permissions"};
            }
        }

        const auto canRemove = co_await global::database->canUserLeaveProject(project.getValueOfId(), member.getValueOfUserId());
        if (!canRemove) {
            throw ApiException{Error::ErrBadRequest, "single_owner"};
        }

        co_return co_await global::database->deleteByPrimaryKey<UserProject>(member.getPrimaryKey());
    }
}
