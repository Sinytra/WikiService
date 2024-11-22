#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/github.h"
#include "service/platforms.h"

namespace api::v1 {
    class ProjectsController final : public drogon::HttpController<ProjectsController, false> {
    public:
        explicit ProjectsController(service::GitHub &, service::Platforms &, service::Database &);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProjectsController::create, "/api/v1/project/create?token={1}", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::remove, "/api/v1/project/{id}?token={token}", drogon::Delete, "AuthFilter");
        ADD_METHOD_TO(ProjectsController::update, "/api/v1/project/update?token={token}", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> create(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> update(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string token) const;

        drogon::Task<> remove(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id,
                              std::string token) const;

    private:
        drogon::Task<std::optional<Project>>
        ProjectsController::validateProjectData(const Json::Value &json, const std::string &token,
                                                std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        service::GitHub &github_;
        service::Platforms &platforms_;
        service::Database &database_;
    };
}
