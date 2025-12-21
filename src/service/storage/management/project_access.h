#pragma once

#include <service/auth.h>
#include <service/database/database.h>

namespace service {
    enum class ProjectMemberRole {
        OWNER,
        MEMBER,
        UNKNOWN
    };
    std::string enumToStr(ProjectMemberRole type);
    ProjectMemberRole parseProjectMemberRole(const std::string &type);
    void to_json(nlohmann::json &j, const ProjectMemberRole &obj);
    void from_json(const nlohmann::json &j, ProjectMemberRole &obj);

    struct ProjectMember {
        std::string username;
        ProjectMemberRole role;
        bool isActor;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProjectMember, username, role, isActor)
    };

    struct ProjectMembersData {
        std::vector<ProjectMember> members;
        bool canEdit;
        bool canLeave;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProjectMembersData, members, canEdit, canLeave);
    };

    drogon::Task<TaskResult<>> assignUserProject(std::string username, std::string id, ProjectMemberRole role);
    drogon::Task<ProjectMemberRole> getUserAccessLevel(Project project, UserSession actor);
    drogon::Task<ProjectMembersData> getProjectMembers(Project project, UserSession actor);
    drogon::Task<TaskResult<>> addProjectMember(Project project, UserSession actor, UserProject member);
    drogon::Task<TaskResult<>> removeProjectMember(Project project, UserSession actor, UserProject member);
}