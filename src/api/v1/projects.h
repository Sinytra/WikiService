#pragma once

#include <drogon/HttpController.h>
#include <models/Project.h>
#include <nlohmann/json.hpp>
#include <service/platforms.h>

using namespace drogon_model::postgres;

namespace api::v1 {
    struct ValidatedProjectData {
        Project project;
        nlohmann::json platforms;
    };

    class ProjectsController final : public drogon::HttpController<ProjectsController, false> {
    public:
        explicit ProjectsController(bool localEnv);

        // clang-format off
        METHOD_LIST_BEGIN
        // Public
        ADD_METHOD_TO(ProjectsController::greet,                "/",                        drogon::Get);
        // Internal
        ADD_METHOD_TO(ProjectsController::listIDs,              "/api/v1/projects",         drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::listPopularProjects,  "/api/v1/projects/popular", drogon::Get, "AuthFilter");
        // Private
        ADD_METHOD_TO(ProjectsController::listUserProjects, "/api/v1/dev/projects",                             drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getProject,       "/api/v1/dev/projects/{1:id}",                      drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getProjectLog,    "/api/v1/dev/projects/{1:id}/log",                  drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::create,           "/api/v1/dev/projects",                             drogon::Post,   "AuthFilter");
        ADD_METHOD_TO(ProjectsController::update,           "/api/v1/dev/projects",                             drogon::Put,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::remove,           "/api/v1/dev/projects/{1:id}",                      drogon::Delete, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::deployProject,    "/api/v1/dev/projects/{1:id}/deploy",               drogon::Post,   "AuthFilter");
        // Content
        ADD_METHOD_TO(ProjectsController::getContentPages,  "/api/v1/dev/projects/{1:id}/content/pages",        drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getContentTags,   "/api/v1/dev/projects/{1:id}/content/tags",         drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getTagItems,      "/api/v1/dev/projects/{1:id}/content/tags/.*",      drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getRecipes,       "/api/v1/dev/projects/{1:id}/content/recipes",      drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getVersions,      "/api/v1/dev/projects/{1:id}/versions",             drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getDeployments,   "/api/v1/dev/projects/{1:id}/deployments",          drogon::Get,    "AuthFilter");
        // Deployments
        // TODO Deployment controller
        ADD_METHOD_TO(ProjectsController::getDeployment,    "/api/v1/dev/deployments/{1:id}",                   drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::deleteDeployment, "/api/v1/dev/deployments/{1:id}",                   drogon::Delete, "AuthFilter");
        // Project issues
        ADD_METHOD_TO(ProjectsController::getIssues,        "/api/v1/dev/projects/{1:id}/issues",               drogon::Get,    "AuthFilter");
        ADD_METHOD_TO(ProjectsController::addIssue,         "/api/v1/dev/projects/{1:id}/issues",               drogon::Post,   "AuthFilter");
        METHOD_LIST_END
        // clang-format on

        drogon::Task<> greet(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> listIDs(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> listUserProjects(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> getProject(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                  std::string id) const;

        drogon::Task<> getProjectLog(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                     std::string id) const;

        drogon::Task<> listPopularProjects(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> create(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> update(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> remove(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string id) const;

        drogon::Task<> deployProject(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                       std::string id) const;

        drogon::Task<> getContentPages(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                       std::string id) const;

        drogon::Task<> getContentTags(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                      std::string id) const;

        drogon::Task<> getTagItems(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                   std::string id) const;

        drogon::Task<> getRecipes(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                  std::string id) const;

        drogon::Task<> getVersions(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                   std::string id) const;

        drogon::Task<> getDeployments(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                      std::string id) const;

        drogon::Task<> getDeployment(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                     std::string id) const;

        drogon::Task<> deleteDeployment(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                        std::string id) const;

        drogon::Task<> getIssues(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                 std::string id) const;

        drogon::Task<> addIssue(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                std::string id) const;

    private:
        nlohmann::json processPlatforms(const nlohmann::json &metadata) const;

        drogon::Task<service::PlatformProject> validatePlatform(const std::string &id, const std::string &repo, const std::string &platform,
                                                                const std::string &slug, bool checkExisting, User user) const;

        drogon::Task<ValidatedProjectData> validateProjectData(const Json::Value &json, User user, bool checkExisting) const;

        void enqueueDeploy(const Project &project, std::string userId) const;

        bool localEnv_;
    };
}
