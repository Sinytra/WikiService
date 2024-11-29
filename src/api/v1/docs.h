#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/documentation.h"
#include "service/github.h"

namespace api::v1 {
    struct ProjectDetails {
        Project project;
        std::string token;
    };

    class DocsController final : public drogon::HttpController<DocsController, false> {
    public:
        explicit DocsController(service::GitHub &, service::Database &, service::Documentation &);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DocsController::project, "/api/v1/project/{1:project}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::page, "/api/v1/project/{1:project}/page/.*", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::tree, "/api/v1/project/{1:project}/tree", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::asset, "/api/v1/project/{1:project}/asset/.*", drogon::Get, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> project(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string project) const;

        drogon::Task<> page(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string project) const;

        drogon::Task<> tree(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string project) const;

        drogon::Task<> asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string project) const;

    private:
        service::GitHub &github_;
        service::Database &database_;
        service::Documentation &documentation_;

        drogon::Task<std::optional<ProjectDetails>> getProject(const std::string& project, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
    };
}
