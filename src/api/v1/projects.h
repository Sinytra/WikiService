#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/documentation.h"
#include "service/github.h"
#include "service/platforms.h"

namespace api::v1 {
    class ProjectsController final : public drogon::HttpController<ProjectsController, false> {
    public:
        explicit ProjectsController(service::GitHub &, service::Platforms &, service::Database &, service::Documentation &);

        // TODO Route to retrieve user projects (+community projects for admins)
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProjectsController::listIDs, "/api/v1/projects", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::create, "/api/v1/project/create?token={1:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::remove, "/api/v1/project/{1:id}/remove?token={2:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::update, "/api/v1/project/update?token={1:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::migrate, "/api/v1/project/migrate?token={1:token}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::invalidate, "/api/v1/project/{1:id}/invalidate?token={2:token}", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> listIDs(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> create(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> update(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> remove(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id,
                              std::string token) const;

        drogon::Task<> migrate(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                               std::string token) const;

        drogon::Task<> invalidate(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id,
                                  std::string token) const;

    private:
        drogon::Task<std::optional<Project>>
        validateProjectData(const Json::Value &json, const std::string &token,
                                                std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<std::optional<Project>>
        validateProjectAccess(const std::string &id, const std::string &token,
                                                  std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<bool>
        validateRepositoryAccess(const std::string &repo, const std::string &token,
                                                     std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        service::GitHub &github_;
        service::Platforms &platforms_;
        service::Database &database_;
        service::Documentation &documentation_;
    };
}
