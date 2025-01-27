#pragma once

#include <drogon/HttpController.h>
#include <service/auth.h>

#include <service/cloudflare.h>
#include <service/database.h>
#include <service/github.h>
#include <service/platforms.h>
#include <service/storage.h>

using namespace service;

namespace api::v1 {
    struct ValidatedProjectData {
        Project project;
        nlohmann::json platforms;
    };

    class ProjectsController final : public drogon::HttpController<ProjectsController, false> {
    public:
        explicit ProjectsController(Auth &, Platforms &, Database &, Storage &, CloudFlare &);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProjectsController::greet, "/", drogon::Get);
        ADD_METHOD_TO(ProjectsController::listIDs, "/api/v1/projects", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::listUserProjects, "/api/v1/projects/dev?token={1:token}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getProjectLog, "/api/v1/project/{1:id}/log?token={2:token}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::listPopularProjects, "/api/v1/projects/popular", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::getProject, "/api/v1/project/{1:id}?token={2:token}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::create, "/api/v1/project/create?token={1:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::remove, "/api/v1/project/{1:id}/remove?token={2:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::update, "/api/v1/project/update?token={1:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::invalidate, "/api/v1/project/{1:id}/invalidate?token={2:token}", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> greet(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> listIDs(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> listUserProjects(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                        std::string token) const;

        drogon::Task<> getProject(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                  std::string project, std::string token) const;

        drogon::Task<> getProjectLog(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                     std::string project, std::string token) const;

        drogon::Task<> listPopularProjects(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> create(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> update(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> remove(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id,
                              std::string token) const;

        drogon::Task<> invalidate(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id,
                                  std::string token) const;

    private:
        nlohmann::json processPlatforms(const nlohmann::json &metadata) const;

        drogon::Task<std::optional<PlatformProject>> validatePlatform(const std::string &id, const std::string &repo,
                                                                      const std::string &platform, const std::string &slug,
                                                                      bool checkExisting, User user,
                                                                      std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<std::optional<ValidatedProjectData>> validateProjectData(const Json::Value &json, User user,
                                                                              std::function<void(const drogon::HttpResponsePtr &)> callback,
                                                                              bool checkExisting) const;

        void reloadProject(const Project &project, bool invalidate = true) const;

        Auth &auth_;
        Platforms &platforms_;
        Database &database_;
        Storage &storage_;
        CloudFlare &cloudflare_;
    };
}
